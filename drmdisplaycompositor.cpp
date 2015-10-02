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

#include <algorithm>
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

static bool drm_composition_layer_has_plane(
    const DrmCompositionLayer &comp_layer) {
  if (comp_layer.plane != NULL)
    if (comp_layer.plane->type() == DRM_PLANE_TYPE_OVERLAY ||
        comp_layer.plane->type() == DRM_PLANE_TYPE_PRIMARY)
      return true;
  return false;
}

static bool drm_composition_layer_has_no_plane(
    const DrmCompositionLayer &comp_layer) {
  return comp_layer.plane == NULL;
}

int DrmDisplayCompositor::ApplyPreComposite(
    DrmDisplayComposition *display_comp) {
  int ret = 0;
  std::vector<DrmCompositionLayer> *layers =
      display_comp->GetCompositionLayers();

  DrmConnector *connector = drm_->GetConnectorForDisplay(display_);
  if (connector == NULL) {
    ALOGE("Failed to determine display mode: no connector for display %d",
          display_);
    return -ENODEV;
  }

  const DrmMode &mode = connector->active_mode();
  DrmFramebuffer &fb = framebuffers_[framebuffer_index_];
  ret = fb.WaitReleased(fb.kReleaseWaitTimeoutMs);
  if (ret) {
    ALOGE("Failed to wait for framebuffer release %d", ret);
    return ret;
  }
  fb.set_release_fence_fd(-1);
  if (!fb.Allocate(mode.h_display(), mode.v_display())) {
    ALOGE("Failed to allocate framebuffer with size %dx%d", mode.h_display(),
          mode.v_display());
    return -ENOMEM;
  }

  std::vector<DrmCompositionLayer> pre_comp_layers;
  for (auto &comp_layer : *layers) {
    if (comp_layer.plane == NULL) {
      pre_comp_layers.emplace_back(std::move(comp_layer));
    }
  }

  ret = pre_compositor_->Composite(pre_comp_layers.data(),
                                   pre_comp_layers.size(), fb.buffer());
  pre_compositor_->Finish();

  if (ret) {
    ALOGE("Failed to composite layers");
    return ret;
  }

  DrmCompositionLayer &pre_comp_layer =
      layers->at(display_comp->pre_composition_layer_index());
  ret = pre_comp_layer.buffer.ImportBuffer(fb.buffer()->handle,
                                           display_comp->importer());
  if (ret) {
    ALOGE("Failed to import handle of layer %d", ret);
    return ret;
  }
  pre_comp_layer.source_crop = DrmHwcRect<float>(0, 0, fb.buffer()->getWidth(),
                                                 fb.buffer()->getHeight());
  pre_comp_layer.display_frame =
      DrmHwcRect<int>(0, 0, fb.buffer()->getWidth(), fb.buffer()->getHeight());

  // TODO(zachr) get a release fence
  // fb.set_release_fence_fd(pre_comp_layer.release_fence.Release());
  framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;

  display_comp->RemoveNoPlaneLayers();
  display_comp->SignalPreCompositionDone();
  return ret;
}

int DrmDisplayCompositor::ApplyFrame(DrmDisplayComposition *display_comp) {
  int ret = 0;

  if (display_comp->pre_composition_layer_index() >= 0) {
    ret = ApplyPreComposite(display_comp);
    if (ret)
      return ret;
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

  std::vector<DrmCompositionLayer> *layers =
      display_comp->GetCompositionLayers();
  for (DrmCompositionLayer &layer : *layers) {
    int acquire_fence = layer.acquire_fence.get();
    if (acquire_fence >= 0) {
      ret = sync_wait(acquire_fence, kAcquireWaitTimeoutMs);
      if (ret) {
        ALOGE("Failed to wait for acquire %d/%d", acquire_fence, ret);
        drmModePropertySetFree(pset);
        drm_->DestroyPropertyBlob(blob_id);
        return ret;
      }
      layer.acquire_fence.Close();
    }

    DrmPlane *plane = layer.plane;

    // Disable the plane if there's no crtc
    if (!layer.crtc) {
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

    if (!layer.buffer) {
      ALOGE("Expected a valid framebuffer for pset");
      ret = -EINVAL;
      break;
    }

    uint64_t rotation;
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
        ret = -EINVAL;
        break;
    }
    if (ret)
      break;

    // TODO: Once we have atomic test, this should fall back to GL
    if (rotation && plane->rotation_property().id() == 0) {
      ALOGE("Rotation is not supported on plane %d", plane->id());
      ret = -EINVAL;
      break;
    }

    ret =
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_property().id(),
                              layer.crtc->id()) ||
        drmModePropertySetAdd(pset, plane->id(), plane->fb_property().id(),
                              layer.buffer->fb_id) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_x_property().id(),
                              layer.display_frame.left) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_y_property().id(),
                              layer.display_frame.top) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->crtc_w_property().id(),
            layer.display_frame.right - layer.display_frame.left) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->crtc_h_property().id(),
            layer.display_frame.bottom - layer.display_frame.top) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_x_property().id(),
                              (int)(layer.source_crop.left) << 16) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_y_property().id(),
                              (int)(layer.source_crop.top) << 16) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->src_w_property().id(),
            (int)(layer.source_crop.right - layer.source_crop.left) << 16) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->src_h_property().id(),
            (int)(layer.source_crop.bottom - layer.source_crop.top) << 16);
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
    active_composition_->FinishComposition();

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

struct DrmDumpLayer {
  int plane_id;
  int crtc_id;
  DrmHwcTransform transform;
  DrmHwcRect<float> source_crop;
  DrmHwcRect<int> display_frame;

  DrmDumpLayer(DrmCompositionLayer &rhs)
      : plane_id(rhs.plane->id()),
        crtc_id(rhs.crtc ? rhs.crtc->id() : -1),
        transform(rhs.transform),
        source_crop(rhs.source_crop),
        display_frame(rhs.display_frame) {
  }
};

void DrmDisplayCompositor::Dump(std::ostringstream *out) const {
  uint64_t cur_ts;

  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    return;

  uint64_t num_frames = dump_frames_composited_;
  dump_frames_composited_ = 0;

  struct timespec ts;
  ret = clock_gettime(CLOCK_MONOTONIC, &ts);

  std::vector<DrmCompositionLayer> *input_layers =
      active_composition_->GetCompositionLayers();
  std::vector<DrmDumpLayer> layers;
  if (active_composition_)
    layers.insert(layers.begin(), input_layers->begin(), input_layers->end());
  else
    ret = -EAGAIN;

  ret |= pthread_mutex_unlock(&lock_);
  if (ret)
    return;

  cur_ts = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  uint64_t num_ms = (cur_ts - dump_last_timestamp_ns_) / (1000 * 1000);
  float fps = num_ms ? (num_frames * 1000.0f) / (num_ms) : 0.0f;

  *out << "--DrmDisplayCompositor[" << display_
       << "]: num_frames=" << num_frames << " num_ms=" << num_ms
       << " fps=" << fps << "\n";

  dump_last_timestamp_ns_ = cur_ts;

  *out << "---- DrmDisplayCompositor Layers: num=" << layers.size() << "\n";
  for (std::vector<DrmDumpLayer>::iterator iter = layers.begin();
       iter != layers.end(); ++iter) {
    *out << "------ DrmDisplayCompositor Layer: plane=" << iter->plane_id
         << " ";

    if (iter->crtc_id < 0) {
      *out << "disabled\n";
      continue;
    }

    *out << "crtc=" << iter->crtc_id
         << " crtc[x/y/w/h]=" << iter->display_frame.left << "/"
         << iter->display_frame.top << "/"
         << iter->display_frame.right - iter->display_frame.left << "/"
         << iter->display_frame.bottom - iter->display_frame.top << " "
         << " src[x/y/w/h]=" << iter->source_crop.left << "/"
         << iter->source_crop.top << "/"
         << iter->source_crop.right - iter->source_crop.left << "/"
         << iter->source_crop.bottom - iter->source_crop.top
         << " transform=" << (uint32_t)iter->transform << "\n";
  }
}
}
