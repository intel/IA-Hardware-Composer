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

  bool ValidateLayers(std::vector<OverlayLayer> &layers,
                      std::vector<OverlayLayer *> &cursor_layers,
                      bool pending_modeset, bool disable_overlay,
                      DisplayPlaneStateList &composition,
                      bool request_video_effect);

  // This should be called only in case of a new cursor layer
  // being added and all other layers are same as previous
  // frame.
  bool ValidateCursorLayer(std::vector<OverlayLayer *> &cursor_layers,
                           DisplayPlaneStateList &composition);

  bool CheckPlaneFormat(uint32_t format);

  void SetOffScreenPlaneTarget(DisplayPlaneState &plane);

  void SetOffScreenCursorPlaneTarget(DisplayPlaneState &plane, uint32_t width,
                                     uint32_t height);

  void ReleaseFreeOffScreenTargets();

  void ReleaseAllOffScreenTargets();

  bool HasSurfaces() const {
    return !surfaces_.empty() || !cursor_surfaces_.empty();
  }

  uint32_t GetGpuFd() const {
    return gpu_fd_;
  }

  uint32_t GetHeight() const {
    return height_;
  }

 protected:
  DisplayPlaneState *GetLastUsedOverlay(DisplayPlaneStateList &composition);
  bool FallbacktoGPU(DisplayPlane *target_plane, OverlayLayer *layer,
                     const std::vector<OverlayPlane> &commit_planes) const;

  void ValidateFinalLayers(DisplayPlaneStateList &list,
                           std::vector<OverlayLayer> &layers);

  void ResetPlaneTarget(DisplayPlaneState &plane, OverlayPlane &overlay_plane);

  void EnsureOffScreenTarget(DisplayPlaneState &plane);

  void PreparePlaneForCursor(DisplayPlaneState *plane);

  DisplayPlaneHandler *plane_handler_;
  ResourceManager *resource_manager_;
  DisplayPlane *cursor_plane_;
  DisplayPlane *primary_plane_;
  std::vector<std::unique_ptr<NativeSurface>> surfaces_;
  std::vector<std::unique_ptr<DisplayPlane>> overlay_planes_;
  std::vector<std::unique_ptr<NativeSurface>> cursor_surfaces_;

  uint32_t width_;
  uint32_t height_;
  uint32_t gpu_fd_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANEMANAGER_H_
