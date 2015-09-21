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
#include "drmcrtc.h"
#include "drmplane.h"
#include "glworker.h"
#include "importer.h"

#include <vector>

#include <hardware/gralloc.h>
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

  hwc_layer_1_t layer;
  hwc_drm_bo_t bo;
  DrmCrtc *crtc;
  DrmPlane *plane;
  native_handle_t *handle;
} DrmCompositionLayer_t;
typedef std::vector<DrmCompositionLayer_t> DrmCompositionLayerVector_t;

class DrmDisplayComposition {
 public:
  DrmDisplayComposition();
  ~DrmDisplayComposition();

  int Init(DrmResources *drm, DrmCrtc *crtc, Importer *importer,
           uint64_t frame_no);

  DrmCompositionType type() const;

  int SetLayers(hwc_layer_1_t *layers, size_t num_layers, size_t *layer_indices,
                std::vector<DrmPlane *> *primary_planes,
                std::vector<DrmPlane *> *overlay_planes);
  int AddPlaneDisable(DrmPlane *plane);
  int SetDpmsMode(uint32_t dpms_mode);

  void RemoveNoPlaneLayers();
  int SignalPreCompositionDone();
  int FinishComposition();

  DrmCompositionLayerVector_t *GetCompositionLayers();
  int pre_composition_layer_index() const;
  uint32_t dpms_mode() const;

  uint64_t frame_no() const;

  Importer *importer() const;

 private:
  DrmDisplayComposition(const DrmDisplayComposition &) = delete;

  bool validate_composition_type(DrmCompositionType desired);

  int CreateNextTimelineFence();
  int IncreaseTimelineToPoint(int point);

  DrmResources *drm_;
  DrmCrtc *crtc_;
  Importer *importer_;
  const gralloc_module_t *gralloc_;
  EGLDisplay egl_display_;

  DrmCompositionType type_;

  int timeline_fd_;
  int timeline_;
  int timeline_current_;
  int timeline_pre_comp_done_;

  DrmCompositionLayerVector_t layers_;
  int pre_composition_layer_index_;
  uint32_t dpms_mode_;

  uint64_t frame_no_;
};
}

#endif  // ANDROID_DRM_DISPLAY_COMPOSITION_H_
