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

static const bool kUseOverlayPlanes = true;

DrmComposition::DrmComposition(DrmResources *drm, Importer *importer)
    : drm_(drm), importer_(importer) {
  for (DrmResources::PlaneIter iter = drm_->begin_planes();
       iter != drm_->end_planes(); ++iter) {
    if ((*iter)->type() == DRM_PLANE_TYPE_PRIMARY)
      primary_planes_.push_back(*iter);
    else if (kUseOverlayPlanes && (*iter)->type() == DRM_PLANE_TYPE_OVERLAY)
      overlay_planes_.push_back(*iter);
  }
}

DrmComposition::~DrmComposition() {
}

int DrmComposition::Init() {
  for (DrmResources::ConnectorIter iter = drm_->begin_connectors();
       iter != drm_->end_connectors(); ++iter) {
    int display = (*iter)->display();
    composition_map_[display].reset(new DrmDisplayComposition());
    if (!composition_map_[display]) {
      ALOGE("Failed to allocate new display composition\n");
      return -ENOMEM;
    }
    int ret = composition_map_[(*iter)->display()]->Init(drm_, importer_);
    if (ret) {
      ALOGE("Failed to init display composition for %d", (*iter)->display());
      return ret;
    }
  }
  return 0;
}

unsigned DrmComposition::GetRemainingLayers(int display,
                                            unsigned num_needed) const {
  DrmCrtc *crtc = drm_->GetCrtcForDisplay(display);
  if (!crtc) {
    ALOGE("Failed to find crtc for display %d", display);
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
  DrmCrtc *crtc = drm_->GetCrtcForDisplay(display);
  if (!crtc) {
    ALOGE("Failed to find crtc for display %d", display);
    return -ENODEV;
  }

  // Find a plane for the layer
  DrmPlane *plane = NULL;
  for (std::vector<DrmPlane *>::iterator iter = primary_planes_.begin();
       iter != primary_planes_.end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc)) {
      plane = *iter;
      primary_planes_.erase(iter);
      break;
    }
  }
  for (std::deque<DrmPlane *>::iterator iter = overlay_planes_.begin();
       !plane && iter != overlay_planes_.end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc)) {
      plane = *iter;
      overlay_planes_.erase(iter);
      break;
    }
  }
  if (!plane) {
    ALOGE("Failed to find plane for display %d", display);
    return -ENOENT;
  }
  return composition_map_[display]->AddLayer(layer, bo, crtc, plane);
}

int DrmComposition::AddDpmsMode(int display, uint32_t dpms_mode) {
  return composition_map_[display]->AddDpmsMode(dpms_mode);
}

std::unique_ptr<DrmDisplayComposition> DrmComposition::TakeDisplayComposition(
    int display) {
  return std::move(composition_map_[display]);
}
}
