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

#ifndef RENDER_STATE_H_
#define RENDER_STATE_H_

#include <vector>

#include <stdint.h>

#include "compositordefs.h"

namespace hwcomposer {

struct OverlayLayer;
struct CompositionRegion;
class NativeGpuResource;

struct RenderState {
  struct LayerState {
    float crop_bounds_[4];
    float alpha_;
    float premult_;
    float texture_matrix_[4];
    GpuResourceHandle handle_;
  };

  void ConstructState(const std::vector<OverlayLayer> &layers,
                      const CompositionRegion &region,
                      const NativeGpuResource *resources);

  float x_;
  float y_;
  float width_;
  float height_;
  std::vector<LayerState> layer_state_;
};

}  // namespace hwcomposer
#endif  // GPU_COMMAND_H__
