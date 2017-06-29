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
    surface_damage_ = display_frame_;
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
    surface_damage_ = display_frame_;

    source_layers_.emplace_back(index);
    state_ = State::kRender;
  }

  void AddSurfaceDamage(const HwcRect<int> &surface_damage) {
    surface_damage_.left = std::min(surface_damage_.left, surface_damage.left);
    surface_damage_.top = std::min(surface_damage_.top, surface_damage.top);
    surface_damage_.right =
        std::max(surface_damage_.right, surface_damage.right);
    surface_damage_.bottom =
        std::max(surface_damage_.bottom, surface_damage.bottom);
  }

  const HwcRect<int> &GetSurfaceDamage() const {
    return surface_damage_;
  }

  void ResetSurfaceDamage() {
    surface_damage_ = HwcRect<int>(0, 0, 0, 0);
  }

  void AddLayers(const std::vector<size_t> &source_layers,
                 const HwcRect<int> &display_frame,
                 const HwcRect<int> &surface_damage, State state) {
    for (const int &index : source_layers) {
      source_layers_.emplace_back(index);
    }
    display_frame_.left = display_frame.left;
    display_frame_.top = display_frame.top;
    display_frame_.right = display_frame.right;
    display_frame_.bottom = display_frame.bottom;

    surface_damage_.left = surface_damage.left;
    surface_damage_.top = surface_damage.top;
    surface_damage_.right = surface_damage.right;
    surface_damage_.bottom = surface_damage.bottom;
    state_ = state;
  }

  void ForceGPURendering() {
    state_ = State::kRender;
  }

  void SetOverlayLayer(const OverlayLayer *layer) {
    layer_ = layer;
  }

  void ReUseOffScreenTarget() {
    state_ = State::kScanout;
    recycled_surface_ = true;
  }

  bool SurfaceRecycled() const {
    return recycled_surface_;
  }

  bool ReleaseSurfaces() const {
    return surfaces_in_transition_ ||
           !(recycled_surface_ || transferred_surfaces_);
  }

  const OverlayLayer *GetOverlayLayer() const {
    return layer_;
  }

  void SetOffScreenTarget(NativeSurface *target) {
    surfaces_.emplace(surfaces_.begin(), target);
  }

  NativeSurface *GetOffScreenTarget() const {
    if (surfaces_.size() == 0)
      return NULL;

    return surfaces_.at(0);
  }

  void TransferSurfaces(DisplayPlaneState &plane_state,
                        bool swap_front_buffer) {
    uint32_t starting_index = 1;
    if (surfaces_.size() < 3 || !swap_front_buffer) {
      starting_index = 0;
    }

    size_t size = surfaces_.size();
    // Lets make sure front buffer is now back in the list.
    for (uint32_t i = starting_index; i < size; i++) {
      plane_state.surfaces_.emplace_back(surfaces_.at(i));
    }

    if (starting_index == 1)
      plane_state.surfaces_.emplace_back(surfaces_.at(0));

    transferred_surfaces_ = true;
    plane_state.surfaces_in_transition_ = true;
  }

  void SurfaceTransitionComplete() {
    surfaces_in_transition_ = false;
    transferred_surfaces_ = false;
  }

  const std::vector<NativeSurface *> &GetSurfaces() const {
    return surfaces_;
  }

  std::vector<NativeSurface *> &GetSurfaces() {
    return surfaces_;
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
  HwcRect<int> display_frame_;
  HwcRect<int> surface_damage_;
  std::vector<size_t> source_layers_;
  std::vector<CompositionRegion> composition_region_;
  bool recycled_surface_ = false;
  bool transferred_surfaces_ = false;
  bool surfaces_in_transition_ = false;
  std::vector<NativeSurface *> surfaces_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANESTATE_H_
