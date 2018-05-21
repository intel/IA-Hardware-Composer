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

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "displayplanehandler.h"
#include "displayplanestate.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class FrameBufferManager;
class GpuDevice;
class ResourceManager;
struct OverlayLayer;

class DisplayPlaneManager {
 public:
  DisplayPlaneManager(DisplayPlaneHandler *plane_handler,
                      ResourceManager *resource_manager);

  virtual ~DisplayPlaneManager();

  bool Initialize(uint32_t width, uint32_t height,
                  FrameBufferManager *frame_buffer_manager);

  bool ValidateLayers(std::vector<OverlayLayer> &layers, int add_index,
                      bool disable_overlay, bool *commit_checked,
                      bool *re_validation_needed,
                      DisplayPlaneStateList &composition,
                      DisplayPlaneStateList &previous_composition,
                      std::vector<NativeSurface *> &mark_later);

  void MarkSurfacesForRecycling(DisplayPlaneState *plane,
                                std::vector<NativeSurface *> &mark_later,
                                bool recycle_resources,
                                bool reset_plane_surfaces = true);

  // API to force plane manager to release any free surfaces the next time
  // ReleaseFreeOffScreenTargets is called.
  void ReleasedSurfaces();

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

  void ReleaseFreeOffScreenTargets(bool forced = false);

  void ReleaseAllOffScreenTargets();

  bool HasSurfaces() const {
    return !surfaces_.empty();
  }

  uint32_t GetHeight() const {
    return height_;
  }

  uint32_t GetTotalOverlays() const {
    return total_overlays_;
  }

  void SetLastPlaneUsage(bool enable);

  // Transform to be applied to all planes associated
  // with pipe of this displayplanemanager.
  void SetDisplayTransform(uint32_t transform);

  // If we have two planes as follows:
  // Plane N: Having top and bottom layer and needs 3d rendering.
  // Plane N-1 covering the middle layer of screen.
  // In this case we should squash layers of N and N-1 planes into
  // one, otherwise we will scanout garbage with plane N.
  bool SquashPlanesAsNeeded(const std::vector<OverlayLayer> &layers,
                            DisplayPlaneStateList &composition,
                            std::vector<OverlayPlane> &commit_planes,
                            std::vector<NativeSurface *> &mark_later,
                            bool *validate_final_layers);

  // Returns true if we want to force target_layer to a separate plane than
  // adding it to
  // last_plane.
  bool ForceSeparatePlane(const std::vector<OverlayLayer> &layers,
                          const DisplayPlaneState &last_plane,
                          const OverlayLayer *target_layer);

 private:
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
                                 std::vector<OverlayPlane> &commit_planes);

  // Checks if we can benefit using display plane rotation.
  void ValidateForDisplayTransform(
      DisplayPlaneState &last_plane,
      const std::vector<OverlayPlane> &commit_planes);

  // Checks if we can benefit by downscaling layer of this plane.
  void ValidateForDownScaling(DisplayPlaneState &last_plane,
                              const std::vector<OverlayPlane> &commit_planes);

  void ForceGpuForAllLayers(std::vector<OverlayPlane> &commit_planes,
                            DisplayPlaneStateList &composition,
                            std::vector<OverlayLayer> &layers,
                            std::vector<NativeSurface *> &mark_later,
                            bool recycle_resources);

  // This should be called only in case of a new cursor layer
  // being added and all other layers are same as previous
  // frame.
  void ValidateCursorLayer(std::vector<OverlayLayer> &all_layers,
                           std::vector<OverlayPlane> &commit_planes,
                           std::vector<OverlayLayer *> &cursor_layers,
                           std::vector<NativeSurface *> &mark_later,
                           DisplayPlaneStateList &composition,
                           bool *validate_final_layers, bool *test_commit_done,
                           bool recycle_resources);

  bool CheckForDownScaling(DisplayPlaneStateList &composition,
                           std::vector<OverlayPlane> &commit_planes);

  void FinalizeValidation(DisplayPlaneStateList &composition,
                          std::vector<OverlayPlane> &commit_planes,
                          bool *render_layers, bool *re_validation_needed);

  DisplayPlaneHandler *plane_handler_;
  ResourceManager *resource_manager_;
  DisplayPlane *cursor_plane_;
  std::vector<std::unique_ptr<NativeSurface>> surfaces_;
  std::vector<std::unique_ptr<DisplayPlane>> overlay_planes_;

  uint32_t width_;
  uint32_t height_;
  uint32_t total_overlays_;
  uint32_t display_transform_;
  bool release_surfaces_;
#ifdef DISABLE_CURSOR_PLANE
  bool enable_last_plane_;
#endif
  FrameBufferManager *fb_manager_ = NULL;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANEMANAGER_H_
