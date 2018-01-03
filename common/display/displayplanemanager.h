/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef COMMON_DISPLAY_DISPLAYPLANEMANAGER_H_
#define COMMON_DISPLAY_DISPLAYPLANEMANAGER_H_

#include <memory>
#include <map>
#include <vector>
#include <tuple>

#include "displayplanestate.h"
#include "displayplanehandler.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class GpuDevice;
class ResourceManager;
struct OverlayLayer;

class DisplayPlaneManager {
 public:
  DisplayPlaneManager(int gpu_fd, DisplayPlaneHandler *plane_handler,
                      ResourceManager *resource_manager);

  virtual ~DisplayPlaneManager();

  bool Initialize(uint32_t width, uint32_t height);

  bool ValidateLayers(std::vector<OverlayLayer> &layers, int add_index,
                      bool disable_overlay, bool *commit_checked,
                      DisplayPlaneStateList &composition,
                      DisplayPlaneStateList &previous_composition,
                      std::vector<NativeSurface *> &mark_later);

  void MarkSurfacesForRecycling(DisplayPlaneState *plane,
                                std::vector<NativeSurface *> &mark_later,
                                bool recycle_resources);

  // This can be used to quickly check if the new DisplayPlaneStateList
  // can be succefully commited before doing a full re-validation.
  bool ReValidatePlanes(DisplayPlaneStateList &list,
                        std::vector<OverlayLayer> &layers,
                        std::vector<NativeSurface *> &mark_later,
                        bool *request_full_validation,
                        bool needs_revalidation_checks,
                        bool re_validate_commit);

  bool CheckPlaneFormat(uint32_t format);

  void SetOffScreenPlaneTarget(DisplayPlaneState &plane);

  void ReleaseFreeOffScreenTargets();

  void ReleaseAllOffScreenTargets();

  bool HasSurfaces() const {
    return !surfaces_.empty();
  }

  uint32_t GetGpuFd() const {
    return gpu_fd_;
  }

  uint32_t GetHeight() const {
    return height_;
  }

 private:
  struct LayerResultCache {
    uint32_t last_transform_ = 0;
    uint32_t last_failed_transform_ = 0;
    DisplayPlane *plane_;
  };

  DisplayPlaneState *GetLastUsedOverlay(DisplayPlaneStateList &composition);
  bool FallbacktoGPU(DisplayPlane *target_plane, OverlayLayer *layer,
                     const std::vector<OverlayPlane> &commit_planes) const;

  void ValidateFinalLayers(std::vector<OverlayPlane> &commit_planes,
                           DisplayPlaneStateList &list,
                           std::vector<OverlayLayer> &layers,
                           std::vector<NativeSurface *> &mark_later,
                           bool recycle_resources);

  void ResetPlaneTarget(DisplayPlaneState &plane, OverlayPlane &overlay_plane);

  void EnsureOffScreenTarget(DisplayPlaneState &plane);

  void PreparePlaneForCursor(DisplayPlaneState *plane,
                             std::vector<NativeSurface *> &mark_later,
                             bool *validate_final_layers, bool reset_buffer,
                             bool recycle_resources);

  void ValidateForDisplayScaling(DisplayPlaneState &last_plane,
                                 std::vector<OverlayPlane> &commit_planes,
                                 OverlayLayer *current_layer,
                                 bool ignore_format = false);

  void ForceGpuForAllLayers(std::vector<OverlayPlane> &commit_planes,
                            DisplayPlaneStateList &composition,
                            std::vector<OverlayLayer> &layers,
                            std::vector<NativeSurface *> &mark_later,
                            bool recycle_resources);

  // This should be called only in case of a new cursor layer
  // being added and all other layers are same as previous
  // frame.
  void ValidateCursorLayer(std::vector<OverlayPlane> &commit_planes,
                           std::vector<OverlayLayer *> &cursor_layers,
                           std::vector<NativeSurface *> &mark_later,
                           DisplayPlaneStateList &composition,
                           bool *validate_final_layers, bool *test_commit_done,
                           bool recycle_resources);

  void SwapSurfaceIfNeeded(DisplayPlaneState *plane);

  DisplayPlaneHandler *plane_handler_;
  ResourceManager *resource_manager_;
  DisplayPlane *cursor_plane_;
  std::vector<std::unique_ptr<NativeSurface>> surfaces_;
  std::vector<std::unique_ptr<DisplayPlane>> overlay_planes_;
  std::vector<LayerResultCache> results_cache_;

  uint32_t width_;
  uint32_t height_;
  uint32_t gpu_fd_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANEMANAGER_H_
