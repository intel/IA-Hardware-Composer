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

#ifndef ANDROID_DRM_DISPLAY_COMPOSITION_H_
#define ANDROID_DRM_DISPLAY_COMPOSITION_H_

#include "drm_hwcomposer.h"
#include "drmplane.h"
#include "importer.h"

#include <vector>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

enum DrmCompositionType {
  DRM_COMPOSITION_TYPE_EMPTY,
  DRM_COMPOSITION_TYPE_FRAME,
  DRM_COMPOSITION_TYPE_DPMS,
};

typedef struct DrmCompositionLayer {
  DrmCompositionLayer();
  ~DrmCompositionLayer();

  hwc_layer_1_t layer;
  hwc_drm_bo_t bo;
  DrmCrtc *crtc;
  DrmPlane *plane;
} DrmCompositionLayer_t;
typedef std::vector<DrmCompositionLayer_t> DrmCompositionLayerVector_t;

class DrmDisplayComposition {
 public:
  DrmDisplayComposition();
  ~DrmDisplayComposition();

  int Init(DrmResources *drm, Importer *importer);

  DrmCompositionType type() const;

  int AddLayer(hwc_layer_1_t *layer, hwc_drm_bo_t *bo, DrmCrtc *crtc,
               DrmPlane *plane);
  int AddDpmsMode(uint32_t dpms_mode);

  int FinishComposition();

  DrmCompositionLayerVector_t *GetCompositionLayers();
  uint32_t dpms_mode() const;

 private:
  DrmDisplayComposition(const DrmDisplayComposition &) = delete;

  bool validate_composition_type(DrmCompositionType desired);

  DrmResources *drm_;
  Importer *importer_;

  DrmCompositionType type_;

  int timeline_fd_;
  int timeline_;

  DrmCompositionLayerVector_t layers_;
  uint32_t dpms_mode_;
};
}

#endif  // ANDROID_DRM_DISPLAY_COMPOSITION_H_
