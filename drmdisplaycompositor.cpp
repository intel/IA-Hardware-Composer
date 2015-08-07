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
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <vector>

#include <drm/drm_mode.h>
#include <cutils/log.h>
#include <sync/sync.h>
#include <utils/Trace.h>

namespace android {

DrmDisplayCompositor::DrmDisplayCompositor()
    : drm_(NULL),
      display_(-1),
      worker_(this),
      frame_no_(0),
      initialized_(false),
      active_(false),
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
    const DrmCompositionLayer_t &comp_layer) {
  if (comp_layer.plane != NULL)
    if (comp_layer.plane->type() == DRM_PLANE_TYPE_OVERLAY ||
        comp_layer.plane->type() == DRM_PLANE_TYPE_PRIMARY)
      return true;
  return false;
}

static bool drm_composition_layer_has_no_plane(
    const DrmCompositionLayer_t &comp_layer) {
  return comp_layer.plane == NULL;
}

int DrmDisplayCompositor::ApplyPreComposite(
    DrmDisplayComposition *display_comp) {
  int ret = 0;
  DrmCompositionLayerVector_t *layers = display_comp->GetCompositionLayers();

  auto last_layer = find_if(layers->rbegin(), layers->rend(),
                            drm_composition_layer_has_plane);
  if (last_layer == layers->rend()) {
    ALOGE("Frame has no overlays");
    return -EINVAL;
  }

  DrmCompositionLayer_t &comp_layer = *last_layer;
  DrmPlane *stolen_plane = NULL;
  std::swap(stolen_plane, comp_layer.plane);

  DrmConnector *connector = drm_->GetConnectorForDisplay(display_);
  if (connector == NULL) {
    ALOGE("Failed to determine display mode: no connector for display %d",
          display_);
    return -ENODEV;
  }

  const DrmMode &mode = connector->active_mode();
  DrmFramebuffer &fb = framebuffers_[framebuffer_index_];
  ret = fb.WaitReleased(-1);
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

  if (!pre_compositor_) {
    pre_compositor_.reset(new GLWorkerCompositor());
    ret = pre_compositor_->Init();
    if (ret) {
      ALOGE("Failed to initialize OpenGL compositor %d", ret);
      return ret;
    }
  }

  std::vector<hwc_layer_1_t> pre_comp_layers;
  for (auto &comp_layer : *layers) {
    if (comp_layer.plane == NULL) {
      pre_comp_layers.push_back(comp_layer.layer);
      pre_comp_layers.back().handle = comp_layer.handle;
      comp_layer.layer.acquireFenceFd = -1;
    }
  }

  ret = pre_compositor_->CompositeAndFinish(
      pre_comp_layers.data(), pre_comp_layers.size(), fb.buffer());

  for (auto &pre_comp_layer : pre_comp_layers) {
    if (pre_comp_layer.acquireFenceFd >= 0) {
      close(pre_comp_layer.acquireFenceFd);
      pre_comp_layer.acquireFenceFd = -1;
    }
  }

  if (ret) {
    ALOGE("Failed to composite layers");
    return ret;
  }

  display_comp->RemoveNoPlaneLayers();

  hwc_layer_1_t pre_comp_output_layer;
  memset(&pre_comp_output_layer, 0, sizeof(pre_comp_output_layer));
  pre_comp_output_layer.compositionType = HWC_OVERLAY;
  pre_comp_output_layer.handle = fb.buffer()->handle;
  pre_comp_output_layer.acquireFenceFd = -1;
  pre_comp_output_layer.releaseFenceFd = -1;
  pre_comp_output_layer.planeAlpha = 0xff;
  pre_comp_output_layer.visibleRegionScreen.numRects = 1;
  pre_comp_output_layer.visibleRegionScreen.rects =
      &pre_comp_output_layer.displayFrame;
  pre_comp_output_layer.sourceCropf.top =
      pre_comp_output_layer.displayFrame.top = 0;
  pre_comp_output_layer.sourceCropf.left =
      pre_comp_output_layer.displayFrame.left = 0;
  pre_comp_output_layer.sourceCropf.right =
      pre_comp_output_layer.displayFrame.right = fb.buffer()->getWidth();
  pre_comp_output_layer.sourceCropf.bottom =
      pre_comp_output_layer.displayFrame.bottom = fb.buffer()->getHeight();

  ret = display_comp->AddLayer(&pre_comp_output_layer,
                               drm_->GetCrtcForDisplay(display_), stolen_plane);
  if (ret) {
    ALOGE("Failed to add composited layer %d", ret);
    return ret;
  }

  fb.set_release_fence_fd(pre_comp_output_layer.releaseFenceFd);
  framebuffer_index_ = (framebuffer_index_ + 1) % DRM_DISPLAY_BUFFERS;

  return ret;
}

int DrmDisplayCompositor::ApplyFrame(DrmDisplayComposition *display_comp) {
  int ret = 0;

  DrmCompositionLayerVector_t *layers = display_comp->GetCompositionLayers();
  bool use_pre_comp = std::any_of(layers->begin(), layers->end(),
                                  drm_composition_layer_has_no_plane);

  if (use_pre_comp) {
    ret = ApplyPreComposite(display_comp);
    if (ret)
      return ret;
  }

  drmModePropertySetPtr pset = drmModePropertySetAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  for (DrmCompositionLayerVector_t::iterator iter = layers->begin();
       iter != layers->end(); ++iter) {
    hwc_layer_1_t *layer = &iter->layer;

    if (layer->acquireFenceFd >= 0) {
      ret = sync_wait(layer->acquireFenceFd, -1);
      if (ret) {
        ALOGE("Failed to wait for acquire %d/%d", layer->acquireFenceFd, ret);
        drmModePropertySetFree(pset);
        return ret;
      }
      close(layer->acquireFenceFd);
      layer->acquireFenceFd = -1;
    }

    DrmPlane *plane = iter->plane;
    DrmCrtc *crtc = iter->crtc;

    // Disable the plane if there's no crtc
    if (!crtc) {
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

    uint64_t rotation;
    switch (layer->transform) {
      case HWC_TRANSFORM_FLIP_H:
        rotation = 1 << DRM_REFLECT_X;
        break;
      case HWC_TRANSFORM_FLIP_V:
        rotation = 1 << DRM_REFLECT_Y;
        break;
      case HWC_TRANSFORM_ROT_90:
        rotation = 1 << DRM_ROTATE_90;
        break;
      case HWC_TRANSFORM_ROT_180:
        rotation = 1 << DRM_ROTATE_180;
        break;
      case HWC_TRANSFORM_ROT_270:
        rotation = 1 << DRM_ROTATE_270;
        break;
      case 0:
        rotation = 0;
        break;
      default:
        ALOGE("Invalid transform value 0x%x given", layer->transform);
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
                              crtc->id()) ||
        drmModePropertySetAdd(pset, plane->id(), plane->fb_property().id(),
                              iter->bo.fb_id) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_x_property().id(),
                              layer->displayFrame.left) ||
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_y_property().id(),
                              layer->displayFrame.top) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->crtc_w_property().id(),
            layer->displayFrame.right - layer->displayFrame.left) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->crtc_h_property().id(),
            layer->displayFrame.bottom - layer->displayFrame.top) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_x_property().id(),
                              (int)(layer->sourceCropf.left) << 16) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_y_property().id(),
                              (int)(layer->sourceCropf.top) << 16) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->src_w_property().id(),
            (int)(layer->sourceCropf.right - layer->sourceCropf.left) << 16) ||
        drmModePropertySetAdd(
            pset, plane->id(), plane->src_h_property().id(),
            (int)(layer->sourceCropf.bottom - layer->sourceCropf.top) << 16);
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
    ret = drmModePropertySetCommit(drm_->fd(), 0, drm_, pset);
    if (ret)
      ALOGE("Failed to commit pset ret=%d\n", ret);
  }
  if (pset)
    drmModePropertySetFree(pset);

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

void DrmDisplayCompositor::Dump(std::ostringstream *out) const {
  uint64_t cur_ts;

  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    return;

  uint64_t num_frames = dump_frames_composited_;
  dump_frames_composited_ = 0;

  struct timespec ts;
  ret = clock_gettime(CLOCK_MONOTONIC, &ts);

  DrmCompositionLayerVector_t layers;
  if (active_composition_)
    layers = *active_composition_->GetCompositionLayers();
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
  for (DrmCompositionLayerVector_t::iterator iter = layers.begin();
       iter != layers.end(); ++iter) {
    hwc_layer_1_t *layer = &iter->layer;
    DrmPlane *plane = iter->plane;

    *out << "------ DrmDisplayCompositor Layer: plane=" << plane->id() << " ";

    DrmCrtc *crtc = iter->crtc;
    if (!crtc) {
      *out << "disabled\n";
      continue;
    }

    *out << "crtc=" << crtc->id()
         << " crtc[x/y/w/h]=" << layer->displayFrame.left << "/"
         << layer->displayFrame.top << "/"
         << layer->displayFrame.right - layer->displayFrame.left << "/"
         << layer->displayFrame.bottom - layer->displayFrame.top << " "
         << " src[x/y/w/h]=" << layer->sourceCropf.left << "/"
         << layer->sourceCropf.top << "/"
         << layer->sourceCropf.right - layer->sourceCropf.left << "/"
         << layer->sourceCropf.bottom - layer->sourceCropf.top
         << " transform=" << layer->transform << "\n";
  }
}
}
