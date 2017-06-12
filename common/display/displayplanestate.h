﻿/*
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

#ifndef COMMON_DISPLAY_DISPLAYPLANESTATE_H_
#define COMMON_DISPLAY_DISPLAYPLANESTATE_H_

#include <stdint.h>
#include <vector>

#include "overlaylayer.h"
#include "compositionregion.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class NativeSurface;
struct OverlayLayer;

typedef std::vector<DisplayPlaneState> DisplayPlaneStateList;

class DisplayPlaneState {
 public:
  enum class State : int32_t { kScanout, kRender };

  DisplayPlaneState() = default;
  DisplayPlaneState(DisplayPlaneState &&rhs) = default;
  DisplayPlaneState &operator=(DisplayPlaneState &&other) = default;
  DisplayPlaneState(DisplayPlane *plane, OverlayLayer *layer, uint32_t index)
      : plane_(plane), layer_(layer) {
    source_layers_.emplace_back(index);
    display_frame_ = layer->GetDisplayFrame();
  }

  explicit DisplayPlaneState(DisplayPlane *plane) : plane_(plane) {
  }

  State GetCompositionState() const {
    return state_;
  }

  const HwcRect<int> &GetDisplayFrame() const {
    return display_frame_;
  }

  void AddLayer(size_t index, const HwcRect<int> &display_frame) {
    display_frame_.left = std::min(display_frame_.left, display_frame.left);
    display_frame_.top = std::min(display_frame_.top, display_frame.top);
    display_frame_.right = std::max(display_frame_.right, display_frame.right);
    display_frame_.bottom =
        std::max(display_frame_.bottom, display_frame.bottom);

    source_layers_.emplace_back(index);
    state_ = State::kRender;
  }

  void AddLayers(const std::vector<size_t> &source_layers,
                 const HwcRect<int> &display_frame, State state) {
    for (const int &index : source_layers) {
      source_layers_.emplace_back(index);
    }
    display_frame_.left = display_frame.left;
    display_frame_.top = display_frame.top;
    display_frame_.right = display_frame.right;
    display_frame_.bottom = display_frame.bottom;
    state_ = state;
  }

  void ForceGPURendering() {
    state_ = State::kRender;
  }

  void SetOverlayLayer(const OverlayLayer *layer) {
    layer_ = layer;
  }

  const OverlayLayer *GetOverlayLayer() const {
    return layer_;
  }

  void SetOffScreenTarget(NativeSurface *target) {
    offscreen_target_ = target;
  }

  NativeSurface *GetOffScreenTarget() const {
    return offscreen_target_;
  }

  DisplayPlane *plane() const {
    return plane_;
  }

  const std::vector<size_t> &source_layers() const {
    return source_layers_;
  }

  std::vector<CompositionRegion> &GetCompositionRegion() {
    return composition_region_;
  }

  const std::vector<CompositionRegion> &GetCompositionRegion() const {
    return composition_region_;
  }

 private:
  State state_ = State::kScanout;
  DisplayPlane *plane_ = NULL;
  const OverlayLayer *layer_ = NULL;
  NativeSurface *offscreen_target_ = NULL;
  HwcRect<int> display_frame_;
  std::vector<size_t> source_layers_;
  std::vector<CompositionRegion> composition_region_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANESTATE_H_
