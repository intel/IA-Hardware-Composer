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

#ifndef ANDROID_DRM_PLATFORM_H_
#define ANDROID_DRM_PLATFORM_H_

#include "drmdisplaycomposition.h"
#include "drmhwcomposer.h"

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <map>
#include <vector>

namespace android {

class DrmDevice;

class Importer {
 public:
  virtual ~Importer() {
  }

  // Creates a platform-specific importer instance
  static Importer *CreateInstance(DrmDevice *drm);

  // Imports the buffer referred to by handle into bo.
  //
  // Note: This can be called from a different thread than ReleaseBuffer. The
  //       implementation is responsible for ensuring thread safety.
  virtual int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) = 0;

  // Releases the buffer object (ie: does the inverse of ImportBuffer)
  //
  // Note: This can be called from a different thread than ImportBuffer. The
  //       implementation is responsible for ensuring thread safety.
  virtual int ReleaseBuffer(hwc_drm_bo_t *bo) = 0;

  // Checks if importer can import the buffer.
  virtual bool CanImportBuffer(buffer_handle_t handle) = 0;
};

class Planner {
 public:
  class PlanStage {
   public:
    virtual ~PlanStage() {
    }

    virtual int ProvisionPlanes(std::vector<DrmCompositionPlane> *composition,
                                std::map<size_t, DrmHwcLayer *> &layers,
                                DrmCrtc *crtc,
                                std::vector<DrmPlane *> *planes) = 0;

   protected:
    // Removes and returns the next available plane from planes
    static DrmPlane *PopPlane(std::vector<DrmPlane *> *planes) {
      if (planes->empty())
        return NULL;
      DrmPlane *plane = planes->front();
      planes->erase(planes->begin());
      return plane;
    }

    static int ValidatePlane(DrmPlane *plane, DrmHwcLayer *layer);

    // Inserts the given layer:plane in the composition at the back
    static int Emplace(std::vector<DrmCompositionPlane> *composition,
                       std::vector<DrmPlane *> *planes,
                       DrmCompositionPlane::Type type, DrmCrtc *crtc,
                       std::pair<size_t, DrmHwcLayer *> layer) {
      DrmPlane *plane = PopPlane(planes);
      std::vector<DrmPlane *> unused_planes;
      int ret = -ENOENT;
      while (plane) {
        ret = ValidatePlane(plane, layer.second);
        if (!ret)
          break;
        if (!plane->zpos_property().is_immutable())
          unused_planes.push_back(plane);
        plane = PopPlane(planes);
      }

      if (!ret) {
        composition->emplace_back(type, plane, crtc, layer.first);
        planes->insert(planes->begin(), unused_planes.begin(),
                       unused_planes.end());
      }

      return ret;
    }
  };

  // Creates a planner instance with platform-specific planning stages
  static std::unique_ptr<Planner> CreateInstance(DrmDevice *drm);

  // Takes a stack of layers and provisions hardware planes for them. If the
  // entire stack can't fit in hardware, FIXME
  //
  // @layers: a map of index:layer of layers to composite
  // @primary_planes: a vector of primary planes available for this frame
  // @overlay_planes: a vector of overlay planes available for this frame
  //
  // Returns: A tuple with the status of the operation (0 for success) and
  //          a vector of the resulting plan (ie: layer->plane mapping).
  std::tuple<int, std::vector<DrmCompositionPlane>> ProvisionPlanes(
      std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
      std::vector<DrmPlane *> *primary_planes,
      std::vector<DrmPlane *> *overlay_planes);

  template <typename T, typename... A>
  void AddStage(A &&... args) {
    stages_.emplace_back(
        std::unique_ptr<PlanStage>(new T(std::forward(args)...)));
  }

 private:
  std::vector<DrmPlane *> GetUsablePlanes(
      DrmCrtc *crtc, std::vector<DrmPlane *> *primary_planes,
      std::vector<DrmPlane *> *overlay_planes);

  std::vector<std::unique_ptr<PlanStage>> stages_;
};

// This plan stage extracts all protected layers and places them on dedicated
// planes.
class PlanStageProtected : public Planner::PlanStage {
 public:
  int ProvisionPlanes(std::vector<DrmCompositionPlane> *composition,
                      std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
                      std::vector<DrmPlane *> *planes);
};

// This plan stage places as many layers on dedicated planes as possible (first
// come first serve), and then sticks the rest in a precomposition plane (if
// needed).
class PlanStageGreedy : public Planner::PlanStage {
 public:
  int ProvisionPlanes(std::vector<DrmCompositionPlane> *composition,
                      std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
                      std::vector<DrmPlane *> *planes);
};
}  // namespace android
#endif
