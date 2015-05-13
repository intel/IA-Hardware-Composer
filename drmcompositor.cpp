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

#define LOG_TAG "hwc-drm-compositor"

#include "drmcompositor.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"

#include <pthread.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <sync/sync.h>

namespace android {

DrmCompositor::DrmCompositor(DrmResources *drm)
    : drm_(drm),
      worker_(this),
      active_composition_(NULL),
      frame_no_(0),
      initialized_(false) {
}

DrmCompositor::~DrmCompositor() {
  if (initialized_)
    pthread_mutex_destroy(&lock_);
}

int DrmCompositor::Init() {
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

Composition *DrmCompositor::CreateComposition(Importer *importer) {
  DrmComposition *composition = new DrmComposition(drm_, importer, frame_no_++);
  if (!composition) {
    ALOGE("Failed to allocate drm composition");
    return NULL;
  }
  int ret = composition->Init();
  if (ret) {
    ALOGE("Failed to initialize drm composition %d", ret);
    delete composition;
    return NULL;
  }
  return composition;
}

int DrmCompositor::QueueComposition(Composition *composition) {
  int ret = pthread_mutex_lock(&lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
    return ret;
  }

  composite_queue_.push((DrmComposition *)composition);

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  worker_.Signal();
  return 0;
}

int DrmCompositor::PerformModeset(DrmCompositionLayerMap_t::iterator begin,
                                  DrmCompositionLayerMap_t::iterator end) {
  DrmCompositionLayer *layer = NULL;
  for (DrmCompositionLayerMap_t::iterator iter = begin; iter != end; ++iter) {
    if (iter->second.layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
      layer = &iter->second;
      break;
    }
  }
  int display = begin->first;
  if (!layer) {
    ALOGE("Could not find target framebuffer for display %d", display);
    return -ENOENT;
  }

  drmModeModeInfo m;
  DrmConnector *connector = drm_->GetConnectorForDisplay(display);
  connector->active_mode().ToModeModeInfo(&m);

  uint32_t connectors = connector->id();
  int ret = drmModeSetCrtc(drm_->fd(), layer->crtc->id(), layer->bo.fb_id, 0, 0,
                           &connectors, 1, &m);
  if (ret)
    ALOGE("Failed set crtc for disp %d/%d", display, ret);
  else
    layer->crtc->set_requires_modeset(false);

  return ret;
}

int DrmCompositor::CompositeDisplay(DrmCompositionLayerMap_t::iterator begin,
                                    DrmCompositionLayerMap_t::iterator end) {
  int ret = 0;
  // Wait for all acquire fences to signal
  for (DrmCompositionLayerMap_t::iterator iter = begin; iter != end; ++iter) {
    hwc_layer_1_t *layer = &iter->second.layer;

    if (layer->acquireFenceFd < 0)
      continue;

    ret = sync_wait(layer->acquireFenceFd, -1);
    if (ret) {
      ALOGE("Failed to wait for acquire %d/%d", layer->acquireFenceFd, ret);
      return ret;
    }
    close(layer->acquireFenceFd);
    layer->acquireFenceFd = -1;
  }

  DrmCrtc *crtc = begin->second.crtc;
  if (crtc->requires_modeset()) {
    ret = PerformModeset(begin, end);
    if (ret)
      ALOGE("Failed modeset on display %d", begin->first);
    return ret;
  }

  drmModePropertySetPtr pset = drmModePropertySetAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  for (DrmCompositionLayerMap_t::iterator iter = begin; iter != end; ++iter) {
    DrmCompositionLayer_t *comp = &iter->second;
    hwc_layer_1_t *layer = &comp->layer;
    DrmPlane *plane = comp->plane;

    ret =
        drmModePropertySetAdd(pset, plane->id(), plane->crtc_property().id(),
                              crtc->id()) ||
        drmModePropertySetAdd(pset, plane->id(), plane->fb_property().id(),
                              comp->bo.fb_id) ||
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

int DrmCompositor::Composite() {
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

  DrmComposition *composition = composite_queue_.front();
  composite_queue_.pop();

  ret = pthread_mutex_unlock(&lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
    return ret;
  }

  DrmCompositionLayerMap_t *map = composition->GetCompositionMap();
  for (DrmResources::ConnectorIter iter = drm_->begin_connectors();
       iter != drm_->end_connectors(); ++iter) {
    int display = (*iter)->display();
    std::pair<DrmCompositionLayerMap_t::iterator,
              DrmCompositionLayerMap_t::iterator> layer_iters =
        map->equal_range(display);

    if (layer_iters.first != layer_iters.second) {
      ret = CompositeDisplay(layer_iters.first, layer_iters.second);
      if (ret) {
        ALOGE("Composite failed for display %d:", display);
        break;
      }
    }
  }

  if (active_composition_) {
    active_composition_->FinishComposition();
    delete active_composition_;
  }
  active_composition_ = composition;
  return ret;
}

bool DrmCompositor::HaveQueuedComposites() const {
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
}
