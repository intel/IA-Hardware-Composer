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

#include <pthread.h>
#include <sstream>
#include <stdlib.h>
#include <time.h>
#include <vector>

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

int DrmDisplayCompositor::ApplyFrame(DrmDisplayComposition *display_comp) {
  int ret = 0;

  drmModePropertySetPtr pset = drmModePropertySetAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  DrmCompositionLayerVector_t *layers = display_comp->GetCompositionLayers();
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
    ret =
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_property().id(),
                              iter->crtc->id()) ||
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
                              layer->sourceCropf.left) ||
        drmModePropertySetAdd(pset, plane->id(), plane->src_y_property().id(),
                              layer->sourceCropf.top) ||
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

  active_composition_.swap(composition);
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

  ret |= pthread_mutex_unlock(&lock_);
  if (ret)
    return;

  cur_ts = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  uint64_t num_ms = (cur_ts - dump_last_timestamp_ns_) / (1000 * 1000);
  unsigned fps = num_ms ? (num_frames * 1000) / (num_ms) : 0;

  *out << "--DrmDisplayCompositor[" << display_
       << "]: num_frames=" << num_frames << " num_ms=" << num_ms
       << " fps=" << fps << "\n";

  dump_last_timestamp_ns_ = cur_ts;
}
}
