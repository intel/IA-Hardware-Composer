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

#define LOG_TAG "hwc-drm-display-composition"

#include "drmdisplaycomposition.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"

#include <stdlib.h>

#include <cutils/log.h>
#include <sw_sync.h>
#include <sync/sync.h>
#include <xf86drmMode.h>

namespace android {

DrmCompositionLayer::DrmCompositionLayer() : crtc(NULL), plane(NULL) {
  memset(&layer, 0, sizeof(layer));
  layer.acquireFenceFd = -1;
  memset(&bo, 0, sizeof(bo));
}

DrmCompositionLayer::~DrmCompositionLayer() {
}

DrmDisplayComposition::DrmDisplayComposition()
    : drm_(NULL),
      importer_(NULL),
      type_(DRM_COMPOSITION_TYPE_EMPTY),
      timeline_fd_(-1),
      timeline_(0),
      timeline_current_(0),
      dpms_mode_(DRM_MODE_DPMS_ON) {
}

DrmDisplayComposition::~DrmDisplayComposition() {
  for (DrmCompositionLayerVector_t::iterator iter = layers_.begin();
       iter != layers_.end(); ++iter) {
    if (importer_ && iter->bo.fb_id)
      importer_->ReleaseBuffer(&iter->bo);

    if (iter->layer.acquireFenceFd >= 0)
      close(iter->layer.acquireFenceFd);
  }

  if (timeline_fd_ >= 0) {
    FinishComposition();
    close(timeline_fd_);
    timeline_fd_ = -1;
  }
}

int DrmDisplayComposition::Init(DrmResources *drm, Importer *importer) {
  drm_ = drm;
  importer_ = importer;

  int ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    return ret;
  }
  timeline_fd_ = ret;
  return 0;
}

DrmCompositionType DrmDisplayComposition::type() const {
  return type_;
}

bool DrmDisplayComposition::validate_composition_type(DrmCompositionType des) {
  return type_ == DRM_COMPOSITION_TYPE_EMPTY || type_ == des;
}

int DrmDisplayComposition::AddLayer(hwc_layer_1_t *layer, hwc_drm_bo_t *bo,
                                    DrmCrtc *crtc, DrmPlane *plane) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  ++timeline_;
  layer->releaseFenceFd =
      sw_sync_fence_create(timeline_fd_, "drm_fence", timeline_);
  if (layer->releaseFenceFd < 0) {
    ALOGE("Could not create release fence %d", layer->releaseFenceFd);
    return layer->releaseFenceFd;
  }

  DrmCompositionLayer_t c_layer;
  c_layer.layer = *layer;
  c_layer.bo = *bo;
  c_layer.crtc = crtc;
  c_layer.plane = plane;

  layer->acquireFenceFd = -1;  // We own this now
  layers_.push_back(c_layer);
  type_ = DRM_COMPOSITION_TYPE_FRAME;
  return 0;
}

int DrmDisplayComposition::AddLayer(hwc_layer_1_t *layer, DrmCrtc *crtc,
                                    DrmPlane *plane) {
  if (layer->transform != 0)
    return -EINVAL;

  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  hwc_drm_bo_t bo;
  int ret = importer_->ImportBuffer(layer->handle, &bo);
  if (ret) {
    ALOGE("Failed to import handle of layer %d", ret);
    return ret;
  }

  ret = AddLayer(layer, &bo, crtc, plane);
  if (ret)
    importer_->ReleaseBuffer(&bo);

  return ret;
}

int DrmDisplayComposition::AddDpmsMode(uint32_t dpms_mode) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_DPMS))
    return -EINVAL;
  dpms_mode_ = dpms_mode;
  type_ = DRM_COMPOSITION_TYPE_DPMS;
  return 0;
}

int DrmDisplayComposition::AddPlaneDisable(DrmPlane *plane) {
  DrmCompositionLayer_t c_layer;
  c_layer.crtc = NULL;
  c_layer.plane = plane;
  layers_.push_back(c_layer);
  return 0;
}

int DrmDisplayComposition::FinishComposition() {
  int timeline_increase = timeline_ - timeline_current_;
  if (timeline_increase <= 0)
    return 0;

  int ret = sw_sync_timeline_inc(timeline_fd_, timeline_increase);
  if (ret)
    ALOGE("Failed to increment sync timeline %d", ret);
  else
    timeline_current_ = timeline_;

  return ret;
}

DrmCompositionLayerVector_t *DrmDisplayComposition::GetCompositionLayers() {
  return &layers_;
}

uint32_t DrmDisplayComposition::dpms_mode() const {
  return dpms_mode_;
}

Importer *DrmDisplayComposition::importer() const {
  return importer_;
}
}
