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

#ifndef COMMON_COMPOSITOR_RENDERSTATE_H_
#define COMMON_COMPOSITOR_RENDERSTATE_H_

#include <vector>

#include <stdint.h>
#include <unistd.h>

#include "compositordefs.h"
#include "hwcdefs.h"

namespace hwcomposer {

struct OverlayLayer;
struct CompositionRegion;
class NativeGpuResource;
class NativeSurface;
class OverlayBuffer;

struct RenderState {
  struct LayerState {
    float crop_bounds_[4];
    float alpha_;
    float premult_;
    float texture_matrix_[4];
    uint32_t layer_index_;
    uint8_t *solid_color_array_;
    GpuResourceHandle handle_;
  };

  void ConstructState(std::vector<OverlayLayer> &layers,
                      const CompositionRegion &region,
                      uint32_t downscaling_factor, bool uses_display_up_scaling,
                      bool use_plane_transform);

  uint32_t x_;
  uint32_t y_;
  uint32_t width_;
  uint32_t height_;
  uint32_t scissor_x_;
  uint32_t scissor_y_;
  uint32_t scissor_width_;
  uint32_t scissor_height_;
  std::vector<LayerState> layer_state_;
};

struct MediaState {
  std::vector<OverlayLayer *> layers_;
  HWCColorMap colors_;
  HWCDeinterlaceProp deinterlace_;
  uint32_t scaling_mode_;
};

struct DrawState {
  ~DrawState() {
    for (int32_t fence : acquire_fences_) {
      close(fence);
    }
  }

  DrawState() = default;

  DrawState(DrawState &&other) {
    surface_ = other.surface_;
    destroy_surface_ = other.destroy_surface_;
    retire_fence_ = other.retire_fence_;

    states_.swap(other.states_);
    acquire_fences_.swap(other.acquire_fences_);
    media_state_ = std::move(other.media_state_);
  }

  std::vector<RenderState> states_;
  MediaState media_state_;
  NativeSurface *surface_;
  bool destroy_surface_ = false;
  int32_t retire_fence_ = -1;
  std::vector<int32_t> acquire_fences_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_RENDERSTATE_H_
