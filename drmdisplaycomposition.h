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

#include <sstream>
#include <vector>

#include <hardware/gralloc.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

struct SquashState;

enum DrmCompositionType {
  DRM_COMPOSITION_TYPE_EMPTY,
  DRM_COMPOSITION_TYPE_FRAME,
  DRM_COMPOSITION_TYPE_DPMS,
  DRM_COMPOSITION_TYPE_MODESET,
};

struct DrmCompositionRegion {
  DrmHwcRect<int> frame;
  std::vector<size_t> source_layers;
};

struct DrmCompositionPlane {
  const static size_t kSourceNone = SIZE_MAX;
  const static size_t kSourcePreComp = kSourceNone - 1;
  const static size_t kSourceSquash = kSourcePreComp - 1;
  const static size_t kSourceLayerMax = kSourceSquash - 1;
  DrmPlane *plane;
  DrmCrtc *crtc;
  size_t source_layer;
};

class DrmDisplayComposition {
 public:
  DrmDisplayComposition() = default;
  DrmDisplayComposition(const DrmDisplayComposition &) = delete;
  ~DrmDisplayComposition();

  int Init(DrmResources *drm, DrmCrtc *crtc, Importer *importer,
           uint64_t frame_no);

  int SetLayers(DrmHwcLayer *layers, size_t num_layers, bool geometry_changed);
  int AddPlaneDisable(DrmPlane *plane);
  int SetDpmsMode(uint32_t dpms_mode);
  int SetDisplayMode(const DrmMode &display_mode);

  int Plan(SquashState *squash, std::vector<DrmPlane *> *primary_planes,
           std::vector<DrmPlane *> *overlay_planes);

  int CreateNextTimelineFence();
  int SignalSquashDone() {
    return IncreaseTimelineToPoint(timeline_squash_done_);
  }
  int SignalPreCompDone() {
    return IncreaseTimelineToPoint(timeline_pre_comp_done_);
  }
  int SignalCompositionDone() {
    return IncreaseTimelineToPoint(timeline_);
  }

  std::vector<DrmHwcLayer> &layers() {
    return layers_;
  }

  std::vector<DrmCompositionRegion> &squash_regions() {
    return squash_regions_;
  }

  std::vector<DrmCompositionRegion> &pre_comp_regions() {
    return pre_comp_regions_;
  }

  std::vector<DrmCompositionPlane> &composition_planes() {
    return composition_planes_;
  }

  uint64_t frame_no() const {
    return frame_no_;
  }

  DrmCompositionType type() const {
    return type_;
  }

  uint32_t dpms_mode() const {
    return dpms_mode_;
  }

  const DrmMode &display_mode() const {
    return display_mode_;
  }

  DrmCrtc *crtc() const {
    return crtc_;
  }

  Importer *importer() const {
    return importer_;
  }

  void Dump(std::ostringstream *out) const;

 private:
  bool validate_composition_type(DrmCompositionType desired);

  int IncreaseTimelineToPoint(int point);

  int CreateAndAssignReleaseFences();

  DrmResources *drm_ = NULL;
  DrmCrtc *crtc_ = NULL;
  Importer *importer_ = NULL;

  DrmCompositionType type_ = DRM_COMPOSITION_TYPE_EMPTY;
  uint32_t dpms_mode_ = DRM_MODE_DPMS_ON;
  DrmMode display_mode_;

  int timeline_fd_ = -1;
  int timeline_ = 0;
  int timeline_current_ = 0;
  int timeline_squash_done_ = 0;
  int timeline_pre_comp_done_ = 0;

  bool geometry_changed_;
  std::vector<DrmHwcLayer> layers_;
  std::vector<DrmCompositionRegion> squash_regions_;
  std::vector<DrmCompositionRegion> pre_comp_regions_;
  std::vector<DrmCompositionPlane> composition_planes_;

  uint64_t frame_no_ = 0;
};
}

#endif  // ANDROID_DRM_DISPLAY_COMPOSITION_H_
