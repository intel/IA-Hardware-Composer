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

#include "displayplanestate.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class GpuDevice;
class NativeBufferHandler;
class OverlayBuffer;
struct OverlayLayer;
class PageFlipState;

class DisplayPlaneManager {
 public:
  DisplayPlaneManager(int gpu_fd, uint32_t pipe_id, uint32_t crtc_id);

  virtual ~DisplayPlaneManager();

  bool Initialize();

  bool BeginFrameUpdate(std::vector<OverlayLayer> &layers,
                        NativeBufferHandler *buffer_handler);
  std::tuple<bool, DisplayPlaneStateList> ValidateLayers(
      std::vector<OverlayLayer> &layers, bool pending_modeset);

  virtual bool CommitFrame(DisplayPlaneStateList &planes,
                           drmModeAtomicReqPtr property_set, bool needs_modeset,
                           PageFlipState *state, ScopedFd &fence);

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

  bool FallbacktoGPU(
      DisplayPlane *target_plane, OverlayLayer *layer,
      const std::vector<OverlayPlane> &commit_planes) const;

  OverlayBuffer *GetOverlayBuffer(const HwcBuffer &bo);

  std::vector<std::unique_ptr<DisplayPlane>> primary_planes_;
  std::vector<std::unique_ptr<DisplayPlane>> cursor_planes_;
  std::vector<std::unique_ptr<DisplayPlane>> overlay_planes_;
  std::vector<std::unique_ptr<OverlayBuffer>> overlay_buffers_;
  uint32_t crtc_id_;
  uint32_t pipe_;
  uint32_t gpu_fd_;
};

}  // namespace hwcomposer
#endif  // DISPLAY_PLANE_MANAGER_H_
