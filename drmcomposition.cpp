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

#define LOG_TAG "hwc-drm-composition"

#include "drmcomposition.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"

#include <stdlib.h>

#include <cutils/log.h>
#include <sw_sync.h>
#include <sync/sync.h>

namespace android {

static const bool kUseOverlayPlanes = false;

DrmCompositionLayer::DrmCompositionLayer() : crtc(NULL), plane(NULL) {
  memset(&layer, 0, sizeof(layer));
  layer.acquireFenceFd = -1;
  memset(&bo, 0, sizeof(bo));
}

DrmCompositionLayer::~DrmCompositionLayer() {
}

DrmComposition::DrmComposition(DrmResources *drm, Importer *importer,
                               uint64_t frame_no)
    : drm_(drm),
      importer_(importer),
      frame_no_(frame_no),
      timeline_fd_(-1),
      timeline_(0) {
  for (DrmResources::PlaneIter iter = drm_->begin_planes();
       iter != drm_->end_planes(); ++iter) {
    if ((*iter)->type() == DRM_PLANE_TYPE_PRIMARY)
      primary_planes_.push_back(*iter);
    else if (kUseOverlayPlanes && (*iter)->type() == DRM_PLANE_TYPE_OVERLAY)
      overlay_planes_.push_back(*iter);
  }
}

DrmComposition::~DrmComposition() {
  for (DrmCompositionLayerMap_t::iterator iter = composition_map_.begin();
       iter != composition_map_.end(); ++iter) {
    importer_->ReleaseBuffer(&iter->second.bo);

    if (iter->second.layer.acquireFenceFd >= 0)
      close(iter->second.layer.acquireFenceFd);
  }

  if (timeline_fd_ >= 0)
    close(timeline_fd_);
}

int DrmComposition::Init() {
  int ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    return ret;
  }
  timeline_fd_ = ret;
  return 0;
}

unsigned DrmComposition::GetRemainingLayers(int display,
                                            unsigned num_needed) const {
  DrmCrtc *crtc = drm_->GetCrtcForDisplay(display);
  if (!crtc) {
    ALOGW("Failed to find crtc for display %d", display);
    return 0;
  }

  unsigned num_planes = 0;
  for (std::vector<DrmPlane *>::const_iterator iter = primary_planes_.begin();
       iter != primary_planes_.end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc))
      ++num_planes;
  }
  for (std::deque<DrmPlane *>::const_iterator iter = overlay_planes_.begin();
       iter != overlay_planes_.end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc))
      ++num_planes;
  }
  return std::min(num_planes, num_needed);
}

int DrmComposition::AddLayer(int display, hwc_layer_1_t *layer,
                             hwc_drm_bo *bo) {
  if (layer->transform != 0)
    return -EINVAL;

  DrmCrtc *crtc = drm_->GetCrtcForDisplay(display);
  if (!crtc) {
    ALOGE("Could not find crtc for display %d", display);
    return -ENODEV;
  }

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

  // First try to find a primary plane for the layer, then fallback on overlays
  for (std::vector<DrmPlane *>::iterator iter = primary_planes_.begin();
       iter != primary_planes_.end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc)) {
      c_layer.plane = (*iter);
      primary_planes_.erase(iter);
      break;
    }
  }
  for (std::deque<DrmPlane *>::iterator iter = overlay_planes_.begin();
       !c_layer.plane && iter != overlay_planes_.end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc)) {
      c_layer.plane = (*iter);
      overlay_planes_.erase(iter);
      break;
    }
  }
  if (!c_layer.plane) {
    close(layer->releaseFenceFd);
    layer->releaseFenceFd = -1;
    return -ENOENT;
  }

  layer->acquireFenceFd = -1;  // We own this now
  composition_map_.insert(DrmCompositionLayerPair_t(display, c_layer));
  return 0;
}

int DrmComposition::FinishComposition() {
  int ret = sw_sync_timeline_inc(timeline_fd_, timeline_);
  if (ret)
    ALOGE("Failed to increment sync timeline %d", ret);

  return ret;
}

DrmCompositionLayerMap_t *DrmComposition::GetCompositionMap() {
  return &composition_map_;
}
}
