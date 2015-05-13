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

#ifndef ANDROID_DRM_COMPOSITION_H_
#define ANDROID_DRM_COMPOSITION_H_

#include "compositor.h"
#include "drm_hwcomposer.h"
#include "drmplane.h"
#include "importer.h"

#include <deque>
#include <map>
#include <vector>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

typedef struct DrmCompositionLayer {
  DrmCompositionLayer();
  ~DrmCompositionLayer();

  hwc_layer_1_t layer;
  hwc_drm_bo_t bo;
  DrmCrtc *crtc;
  DrmPlane *plane;
} DrmCompositionLayer_t;

typedef std::multimap<int, DrmCompositionLayer> DrmCompositionLayerMap_t;
typedef std::pair<int, DrmCompositionLayer> DrmCompositionLayerPair_t;

class DrmComposition : public Composition {
 public:
  DrmComposition(DrmResources *drm, Importer *importer, uint64_t frame_no);
  ~DrmComposition();

  virtual int Init();

  virtual unsigned GetRemainingLayers(int display, unsigned num_needed) const;
  virtual int AddLayer(int display, hwc_layer_1_t *layer, hwc_drm_bo_t *bo);

  int FinishComposition();

  DrmCompositionLayerMap_t *GetCompositionMap();

 private:
  DrmResources *drm_;
  Importer *importer_;

  uint64_t frame_no_;

  int timeline_fd_;
  int timeline_;

  std::vector<DrmPlane *> primary_planes_;
  std::deque<DrmPlane *> overlay_planes_;
  DrmCompositionLayerMap_t composition_map_;
};
}

#endif  // ANDROID_DRM_COMPOSITION_H_
