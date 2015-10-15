/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-display-compositor"

#include "drmdisplaycompositor.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"
#include "glworker.h"

#include <pthread.h>
#include <sched.h>
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <vector>

#include <drm/drm_mode.h>
#include <cutils/log.h>
#include <sync/sync.h>
#include <utils/Trace.h>

#define DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH 3

namespace android {

void SquashState::Init(DrmHwcLayer *layers, size_t num_layers) {
  generation_number_++;
  valid_history_ = 0;
  regions_.clear();
  last_handles_.clear();

  std::vector<DrmHwcRect<int>> in_rects;
  for (size_t i = 0; i < num_layers; i++) {
    DrmHwcLayer *layer = &layers[i];
    in_rects.emplace_back(layer->display_frame);
    last_handles_.push_back(layer->sf_handle);
  }

  std::vector<seperate_rects::RectSet<uint64_t, int>> out_regions;
  seperate_rects::seperate_rects_64(in_rects, &out_regions);

  for (const seperate_rects::RectSet<uint64_t, int> &out_region : out_regions) {
    regions_.emplace_back();
    Region &region = regions_.back();
    region.rect = out_region.rect;
    region.layer_refs = out_region.id_set.getBits();
  }
}

void SquashState::GenerateHistory(DrmHwcLayer *layers,
                                  std::vector<bool> &changed_regions) const {
  std::bitset<kMaxLayers> changed_layers;
  for (size_t i = 0; i < last_handles_.size(); i++) {
    DrmHwcLayer *layer = &layers[i];
    if (last_handles_[i] != layer->sf_handle) {
      changed_layers.set(i);
    }
  }

  changed_regions.resize(regions_.size());
  for (size_t i = 0; i < regions_.size(); i++) {
    changed_regions[i] = (regions_[i].layer_refs & changed_layers).any();
  }
}

void SquashState::StableRegionsWithMarginalHistory(
    const std::vector<bool> &changed_regions,
    std::vector<bool> &stable_regions) const {
  stable_regions.resize(regions_.size());
  for (size_t i = 0; i < regions_.size(); i++) {
    stable_regions[i] = !changed_regions[i] && is_stable(i);
  }
}

void SquashState::RecordHistory(DrmHwcLayer *layers,
                                const std::vector<bool> &changed_regions) {
  for (size_t i = 0; i < last_handles_.size(); i++) {
    DrmHwcLayer *layer = &layers[i];
    last_handles_[i] = layer->sf_handle;
  }

  for (size_t i = 0; i < regions_.size(); i++) {
    regions_[i].change_history <<= 1;
    regions_[i].change_history.set(/* LSB */ 0, changed_regions[i]);
  }

  valid_history_++;
}

void SquashState::RecordSquashed(const std::vector<bool> &squashed_regions) {
  for (size_t i = 0; i < regions_.size(); i++) {
    regions_[i].squashed = squashed_regions[i];
  }
}

void SquashState::Dump(std::ostringstream *out) const {
  *out << "----SquashState generation=" << generation_number_
       << " history=" << valid_history_ << "\n"
       << "    Regions: count=" << regions_.size() << "\n";
  for (size_t i = 0; i < regions_.size(); i++) {
    const Region &region = regions_[i];
    *out << "      [" << i << "]"
         << " history=" << region.change_history << " rect";
    region.rect.Dump(out);
    *out << " layers=(";
    bool first = true;
    for (size_t layer_index = 0; layer_index < kMaxLayers; layer_index++) {
      if ((region.layer_refs &
           std::bitset<kMaxLayers>((size_t)1 << layer_index))
              .any()) {
        if (!first)
          *out << " ";
        first = false;
        *out << layer_index;
      }
    }
    *out << ")";
    if (region.squashed)
      *out << " squashed";
    *out << "\n";
  }
}

DrmDisplayCompositor::DrmDisplayCompositor()
    : drm_(NULL),
      display_(-1),
      worker_(this),
      initialized_(false),
      active_(false),
      needs_modeset_(false),
      framebuffer_index_(0),
      dump_frames_composited_(0),
      dump_last_timestamp_ns_(0) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    return;
  dump_last_timestamp_ns_ = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

DrmDisplayCompositor::~DrmDisplayCompositor() {
  if (!initialized_)
    return;

  worker_.Exit();

  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

  while (!composite_queue_.empty()) {
    composite_queue_.front().reset();
    composite_queue_.pop();
  }
  active_composition_.reset();

  ret = pthread_mutex_unlock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

  pthread_mutex_destroy(&lock_);
}

int DrmDisplayCompositor::Init(DrmResources *drm, int display) {
  drm_ = drm;
  display_ = display;

  int ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to initialize drm compositor lock %d\n", ret);
    return ret;
  }
  ret = worker_.Init();
  if (ret) {
    pthread_mutex_destroy(&lock_);
    ALOGE("Failed to initialize compositor worker %d\n", ret);
    return ret;
  }

  initialized_ = true;
  return 0;
}

std::unique_ptr<DrmDisplayComposition> DrmDisplayCompositor::CreateComposition()
    const {
  return std::unique_ptr<DrmDisplayComposition>(new DrmDisplayComposition());
}

int DrmDisplayCompositor::QueueComposition(
    std::unique_ptr<DrmDisplayComposition> composition) {
  switch (composition->type()) {
    case DRM_COMPOSITION_TYPE_FRAME:
      if (!active_)
        return -ENODEV;
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      /*
       * Update the state as soon as we get it so we can start/stop queuing
       * frames asap.
       */
      active_ = (composition->dpms_mode() == DRM_MODE_DPMS_ON);
      break;
    case DRM_COMPOSITION_TYPE_MODESET:
      break;
    case DRM_COMPOSITION_TYPE_EMPTY:
      return 0;
    default:
      ALOGE("Unknown composition type %d/%d", composition->type(), display_);
      return -ENOENT;
  }

  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return ret;
  }

  // Block the queue if it gets too large. Otherwise, SurfaceFlinger will start
  // to eat our buffer handles when we get about 1 second behind.
  while (composite_queue_.size() >= DRM_DISPLAY_COMPOSITOR_MAX_QUEUE_DEPTH) {
    pthread_mutex_unlock(&lock_);
    sched_yield();
    pthread_mutex_lock(&lock_);
  }

  composite_queue_.push(std::move(composition));

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  worker_.Signal();
  return 0;
}

std::tuple<uint32_t, uint32_t, int>
DrmDisplayCompositor::GetActiveModeResolution() {
  DrmConnector *connector = drm_->GetConnectorForDisplay(display_);
  if (connector == NULL) {
    ALOGE("Failed to determine display mode: no connector for display %d",
          display_);
    return std::make_tuple(0, 0, -ENODEV);
  }

  const DrmMode &mode = connector->active_mode();
  return std::make_tuple(mode.h_display(), mode.v_display(), 0);
}

int DrmDisplayCompositor::PrepareFramebuffer(
    DrmFramebuffer &fb, DrmDisplayComposition *display_comp) {
  int ret = fb.WaitReleased(-1);
  if (ret) {
    ALOGE("Failed to wait for framebuffer release %d", ret);
    return ret;
  }
  uint32_t width, height;
  std::tie(width, height, ret) = GetActiveModeResolution();
  if (ret) {
    ALOGE(
        "Failed to allocate framebuffer because the display resolution could "
        "not be determined %d",
        ret);
    return ret;
  }

  fb.set_release_fence_fd(-1);
  if (!fb.Allocate(width, height)) {
    ALOGE("Failed to allocate framebuffer with size %dx%d", width, height);
    return -ENOMEM;
  }

  display_comp->layers().emplace_back();
  DrmHwcLayer &pre_comp_layer = display_comp->layers().back();
  pre_comp_layer.sf_handle = fb.buffer()->handle;
  pre_comp_layer.source_crop = DrmHwcRect<float>(0, 0, width, height);
  pre_comp_layer.display_frame = DrmHwcRect<int>(0, 0, width, height);
  ret = pre_comp_layer.buffer.ImportBuffer(fb.buffer()->handle,
                                           display_comp->importer());
  if (ret) {
    ALOGE("Failed to import framebuffer for display %d", ret);
    return ret;
  }

  return ret;
}

int DrmDisplayCompositor::ApplyPreComposite(
    DrmDisplayComposition *display_comp) {
  int ret = 0;

  DrmFramebuffer &fb = framebuffers_[framebuffer_index_];
  ret = PrepareFramebuffer(fb, display_comp);
  if (ret) {
    ALOGE("Failed to prepare framebuffer for precomposite %d", ret);
    return ret;
  }

  std::vector<DrmCompositionRegion> &regions = display_comp->pre_comp_regions();
  ret = pre_compositor_->Composite(display_comp->layers().data(),
                                   regions.data(), regions.size(), fb.buffer());
  pre_compositor_->Finish();

  if (ret) {
    ALOGE("Failed to composite layers");
    return ret;
  }

  ret = display_comp->CreateNextTimelineFence();
  if (ret <= 0) {
    ALOGE("Failed to create pre comp framebuffer release fence %d", ret);
    return ret;
  }

  fb.set_release_fence_fd(ret);
  display_comp->SignalPreCompDone();

  return 0;
}

int DrmDisplayCompositor::DisablePlanes(DrmDisplayComposition *display_comp) {
  drmModePropertySetPtr pset = drmModePropertySetAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  int ret;
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();
  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane;
    ret =
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_property().id(),
                              0) ||
        drmModePropertySetAdd(pset, plane->id(), plane->fb_property().id(), 0);
    if (ret) {
      ALOGE("Failed to add plane %d disable to pset", plane->id());
      drmModePropertySetFree(pset);
      return ret;
    }
  }

  ret = drmModePropertySetCommit(drm_->fd(), 0, drm_, pset);
  if (ret) {
    ALOGE("Failed to commit pset ret=%d\n", ret);
    drmModePropertySetFree(pset);
    return ret;
  }

  drmModePropertySetFree(pset);
  return 0;
}

int DrmDisplayCompositor::ApplyFrame(DrmDisplayComposition *display_comp) {
  int ret = 0;

  std::vector<DrmHwcLayer> &layers = display_comp->layers();
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();
  std::vector<DrmCompositionRegion> &pre_comp_regions =
      display_comp->pre_comp_regions();

  bool do_pre_comp = pre_comp_regions.size() > 0;
  DrmFramebuffer *pre_comp_fb;
  int pre_comp_layer_index = -1;

  if (do_pre_comp) {
    ret = ApplyPreComposite(display_comp);
    if (ret)
      return ret;

    pre_comp_layer_index = layers.size() - 1;
    framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;
  }

  DrmConnector *connector = drm_->GetConnectorForDisplay(display_);
  if (!connector) {
    ALOGE("Could not locate connector for display %d", display_);
    return -ENODEV;
  }
  DrmCrtc *crtc = drm_->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Could not locate crtc for display %d", display_);
    return -ENODEV;
  }

  drmModePropertySetPtr pset = drmModePropertySetAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  uint32_t blob_id = 0;
  uint64_t old_blob_id;
  if (needs_modeset_) {
    DrmProperty old_mode;
    ret = drm_->GetCrtcProperty(*crtc, crtc->mode_property().name().c_str(),
                                &old_mode);
    if (ret) {
      ALOGE("Failed to get old mode property from crtc %d", crtc->id());
      drmModePropertySetFree(pset);
      return ret;
    }
    ret = old_mode.value(&old_blob_id);
    if (ret) {
      ALOGE("Could not get old blob id value %d", ret);
      drmModePropertySetFree(pset);
      return ret;
    }

    struct drm_mode_modeinfo drm_mode;
    memset(&drm_mode, 0, sizeof(drm_mode));
    next_mode_.ToDrmModeModeInfo(&drm_mode);

    ret = drm_->CreatePropertyBlob(&drm_mode, sizeof(struct drm_mode_modeinfo),
                                   &blob_id);
    if (ret) {
      ALOGE("Failed to create mode property blob %d", ret);
      drmModePropertySetFree(pset);
      return ret;
    }

    ret = drmModePropertySetAdd(pset, crtc->id(), crtc->mode_property().id(),
                                blob_id) ||
          drmModePropertySetAdd(pset, connector->id(),
                                connector->crtc_id_property().id(), crtc->id());
    if (ret) {
      ALOGE("Failed to add blob %d to pset", blob_id);
      drmModePropertySetFree(pset);
      drm_->DestroyPropertyBlob(blob_id);
      return ret;
    }
  }

  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane;
    DrmCrtc *crtc = comp_plane.crtc;

    int fb_id = -1;
    DrmHwcRect<int> display_frame;
    DrmHwcRect<float> source_crop;
    uint64_t rotation = 0;
    switch (comp_plane.source_layer) {
      case DrmCompositionPlane::kSourceNone:
        break;
      case DrmCompositionPlane::kSourcePreComp: {
        if (!do_pre_comp) {
          ALOGE(
              "Can not use pre composite framebuffer with no pre composite "
              "layers");
          ret = -EINVAL;
          goto out;
        }
        DrmHwcLayer &layer = layers[pre_comp_layer_index];
        fb_id = layer.buffer->fb_id;
        display_frame = layer.display_frame;
        source_crop = layer.source_crop;
        break;
      }
      case DrmCompositionPlane::kSourceSquash:
        break;
      default: {
        if (comp_plane.source_layer >= layers.size()) {
          ALOGE("Source layer index %zu out of bounds %zu",
                comp_plane.source_layer, layers.size());
          break;
        }
        DrmHwcLayer &layer = layers[comp_plane.source_layer];
        if (layer.acquire_fence.get() >= 0) {
          int acquire_fence = layer.acquire_fence.get();
          for (int i = 0; i < kAcquireWaitTries; ++i) {
            ret = sync_wait(acquire_fence, kAcquireWaitTimeoutMs);
            if (ret)
              ALOGW("Acquire fence %d wait %d failed (%d). Total time %d",
                    acquire_fence, i, ret, (i + 1) * kAcquireWaitTimeoutMs);
          }
          if (ret) {
            ALOGE("Failed to wait for acquire %d/%d", acquire_fence, ret);
            break;
          }
          layer.acquire_fence.Close();
        }
        if (!layer.buffer) {
          ALOGE("Expected a valid framebuffer for pset");
          break;
        }
        fb_id = layer.buffer->fb_id;
        display_frame = layer.display_frame;
        source_crop = layer.source_crop;
        switch (layer.transform) {
          case DrmHwcTransform::kFlipH:
            rotation = 1 << DRM_REFLECT_X;
            break;
          case DrmHwcTransform::kFlipV:
            rotation = 1 << DRM_REFLECT_Y;
            break;
          case DrmHwcTransform::kRotate90:
            rotation = 1 << DRM_ROTATE_90;
            break;
          case DrmHwcTransform::kRotate180:
            rotation = 1 << DRM_ROTATE_180;
            break;
          case DrmHwcTransform::kRotate270:
            rotation = 1 << DRM_ROTATE_270;
            break;
          case DrmHwcTransform::kIdentity:
            rotation = 0;
            break;
          default:
            ALOGE("Invalid transform value 0x%x given", layer.transform);
            break;
        }
        break;
      }
    }

    // Disable the plane if there's no framebuffer
    if (fb_id < 0) {
      ret = drmModePropertySetAdd(pset, plane->id(),
                                  plane->crtc_property().id(), 0) ||
            drmModePropertySetAdd(pset, plane->id(), plane->fb_property().id(),
                                  0);
      if (ret) {
        ALOGE("Failed to add plane %d disable to pset", plane->id());
        break;
      }
      continue;
    }

    // TODO: Once we have atomic test, this should fall back to GL
    if (rotation && plane->rotation_property().id() == 0) {
      ALOGE("Rotation is not supported on plane %d", plane->id());
      ret = -EINVAL;
      break;
    }

    ret =
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_property().id(),
                              crtc->id()) ||
        drmModePropertySetAdd(pset, plane->id(), plane->fb_property().id(),
                              fb_id) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_x_property().id(),
                              display_frame.left) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_y_property().id(),
                              display_frame.top) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_w_property().id(),
                              display_frame.right - display_frame.left) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_h_property().id(),
                              display_frame.bottom - display_frame.top) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_x_property().id(),
                              (int)(source_crop.left) << 16) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_y_property().id(),
                              (int)(source_crop.top) << 16) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_w_property().id(),
                              (int)(source_crop.right - source_crop.left)
                                  << 16) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_h_property().id(),
                              (int)(source_crop.bottom - source_crop.top)
                                  << 16);
    if (ret) {
      ALOGE("Failed to add plane %d to set", plane->id());
      break;
    }

    if (plane->rotation_property().id()) {
      ret = drmModePropertySetAdd(pset, plane->id(),
                                  plane->rotation_property().id(), rotation);
      if (ret) {
        ALOGE("Failed to add rotation property %d to plane %d",
              plane->rotation_property().id(), plane->id());
        break;
      }
    }
  }

out:
  if (!ret) {
    ret = drmModePropertySetCommit(drm_->fd(), DRM_MODE_ATOMIC_ALLOW_MODESET,
                                   drm_, pset);
    if (ret) {
      ALOGE("Failed to commit pset ret=%d\n", ret);
      drmModePropertySetFree(pset);
      if (needs_modeset_)
        drm_->DestroyPropertyBlob(blob_id);
      return ret;
    }
  }
  if (pset)
    drmModePropertySetFree(pset);

  if (needs_modeset_) {
    ret = drm_->DestroyPropertyBlob(old_blob_id);
    if (ret) {
      ALOGE("Failed to destroy old mode property blob %lld/%d", old_blob_id,
            ret);
      return ret;
    }

    /* TODO: Add dpms to the pset when the kernel supports it */
    ret = ApplyDpms(display_comp);
    if (ret) {
      ALOGE("Failed to apply DPMS after modeset %d\n", ret);
      return ret;
    }

    connector->set_active_mode(next_mode_);
    needs_modeset_ = false;
  }

  return ret;
}

int DrmDisplayCompositor::ApplyDpms(DrmDisplayComposition *display_comp) {
  DrmConnector *conn = drm_->GetConnectorForDisplay(display_);
  if (!conn) {
    ALOGE("Failed to get DrmConnector for display %d", display_);
    return -ENODEV;
  }

  const DrmProperty &prop = conn->dpms_property();
  int ret = drmModeConnectorSetProperty(drm_->fd(), conn->id(), prop.id(),
                                        display_comp->dpms_mode());
  if (ret) {
    ALOGE("Failed to set DPMS property for connector %d", conn->id());
    return ret;
  }
  return 0;
}

int DrmDisplayCompositor::Composite() {
  ATRACE_CALL();

  if (!pre_compositor_) {
    pre_compositor_.reset(new GLWorkerCompositor());
    int ret = pre_compositor_->Init();
    if (ret) {
      ALOGE("Failed to initialize OpenGL compositor %d", ret);
      return ret;
    }
  }

  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return ret;
  }
  if (composite_queue_.empty()) {
    ret = pthread_mutex_unlock(&lock_);
    if (ret)
      ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  std::unique_ptr<DrmDisplayComposition> composition(
      std::move(composite_queue_.front()));

  composite_queue_.pop();

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  switch (composition->type()) {
    case DRM_COMPOSITION_TYPE_FRAME:
      ret = ApplyFrame(composition.get());
      if (ret) {
        ALOGE("Composite failed for display %d", display_);

        // Disable the hw used by the last active composition. This allows us to
        // signal the release fences from that composition to avoid hanging.
        if (DisablePlanes(active_composition_.get()))
          return ret;
      }
      ++dump_frames_composited_;
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      ret = ApplyDpms(composition.get());
      if (ret)
        ALOGE("Failed to apply dpms for display %d", display_);
      return ret;
    case DRM_COMPOSITION_TYPE_MODESET:
      next_mode_ = composition->display_mode();
      needs_modeset_ = true;
      return 0;
    default:
      ALOGE("Unknown composition type %d", composition->type());
      return -EINVAL;
  }

  if (active_composition_)
    active_composition_->SignalCompositionDone();

  ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire lock for active_composition swap");

  active_composition_.swap(composition);

  if (!ret)
    ret = pthread_mutex_unlock(&lock_);
  if (ret)
    ALOGE("Failed to release lock for active_composition swap");

  return ret;
}

bool DrmDisplayCompositor::HaveQueuedComposites() const {
  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return false;
  }

  bool empty_ret = !composite_queue_.empty();

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return false;
  }

  return empty_ret;
}

void DrmDisplayCompositor::Dump(std::ostringstream *out) const {
  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    return;

  uint64_t num_frames = dump_frames_composited_;
  dump_frames_composited_ = 0;

  struct timespec ts;
  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret) {
    pthread_mutex_unlock(&lock_);
    return;
  }

  uint64_t cur_ts = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  uint64_t num_ms = (cur_ts - dump_last_timestamp_ns_) / (1000 * 1000);
  float fps = num_ms ? (num_frames * 1000.0f) / (num_ms) : 0.0f;

  *out << "--DrmDisplayCompositor[" << display_
       << "]: num_frames=" << num_frames << " num_ms=" << num_ms
       << " fps=" << fps << "\n";

  dump_last_timestamp_ns_ = cur_ts;

  if (active_composition_)
    active_composition_->Dump(out);

  pthread_mutex_unlock(&lock_);
}
}
