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

#ifndef DISPLAY_PLANE_MANAGER_H_
#define DISPLAY_PLANE_MANAGER_H_

#include <memory>
#include <map>
#include <vector>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <hwcbuffer.h>
#include <scopedfd.h>

#include "nativesync.h"

#include "displayplanestate.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class GpuDevice;
class NativeBufferHandler;
struct OverlayLayer;

class DisplayPlaneManager {
 public:
  DisplayPlaneManager(int gpu_fd, uint32_t pipe_id, uint32_t crtc_id);

  virtual ~DisplayPlaneManager();

  bool Initialize(NativeBufferHandler *buffer_handler, uint32_t width,
                  uint32_t height);

  bool BeginFrameUpdate(std::vector<OverlayLayer> &layers);

  std::tuple<bool, DisplayPlaneStateList> ValidateLayers(
      std::vector<OverlayLayer> &layers,
      const std::vector<OverlayLayer> &previous_layers,
      const DisplayPlaneStateList &previous_planes_state, bool pending_modeset);

  bool CommitFrame(DisplayPlaneStateList &planes,
                   drmModeAtomicReqPtr property_set, bool needs_modeset,
                   std::unique_ptr<NativeSync> &sync_object, ScopedFd &fence);

  void DisablePipe(drmModeAtomicReqPtr property_set);

  void EndFrameUpdate();

 protected:
  struct OverlayPlane {
   public:
    OverlayPlane(DisplayPlane *plane, OverlayLayer *layer)
        : plane(plane), layer(layer) {
    }
    DisplayPlane *plane;
    OverlayLayer *layer;
  };

  virtual std::unique_ptr<DisplayPlane> CreatePlane(uint32_t plane_id,
                                                    uint32_t possible_crtcs);
  virtual bool TestCommit(const std::vector<OverlayPlane> &commit_planes) const;

  bool FallbacktoGPU(DisplayPlane *target_plane, OverlayLayer *layer,
                     const std::vector<OverlayPlane> &commit_planes) const;

  void EnsureOffScreenTarget(DisplayPlaneState &plane);
  void ValidateFinalLayers(DisplayPlaneStateList &list,
                           std::vector<OverlayLayer> &layers);
  void ValidateCachedLayers(
      const DisplayPlaneStateList &previous_composition_planes,
      const std::vector<OverlayLayer> &previous_layers,
      std::vector<OverlayLayer> &layers, DisplayPlaneStateList &composition,
      bool *render_layers);

  NativeBufferHandler *buffer_handler_;
  std::vector<std::unique_ptr<NativeSurface>> surfaces_;
  std::vector<NativeSurface *> in_flight_surfaces_;
  std::unique_ptr<DisplayPlane> primary_plane_;
  std::unique_ptr<DisplayPlane> cursor_plane_;
  std::vector<std::unique_ptr<DisplayPlane>> overlay_planes_;
  std::unique_ptr<NativeSync> current_sync_;

  uint32_t width_;
  uint32_t height_;
  uint32_t crtc_id_;
  uint32_t pipe_;
  uint32_t gpu_fd_;
};

}  // namespace hwcomposer
#endif  // DISPLAY_PLANE_MANAGER_H_
