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

#include "displayplanestate.h"

namespace hwcomposer {

DisplayPlaneState::DisplayPlaneState(DisplayPlane *plane, OverlayLayer *layer,
                                     uint32_t index)
    : plane_(plane), layer_(layer) {
  source_layers_.emplace_back(index);
  display_frame_ = layer->GetDisplayFrame();
  source_crop_ = layer->GetSourceCrop();
  if (layer->IsCursorLayer()) {
    type_ = PlaneType::kCursor;
    has_cursor_layer_ = true;
  }
}

void DisplayPlaneState::CopyState(DisplayPlaneState &state) {
  has_cursor_layer_ = state.has_cursor_layer_;
  type_ = state.type_;
  use_plane_scalar_ = state.use_plane_scalar_;
  plane_ = state.plane_;
  state_ = state.state_;
  source_crop_ = state.source_crop_;
  display_frame_ = state.display_frame_;
  apply_effects_ = state.apply_effects_;
  plane_->SetInUse(true);
  // We don't copy recycled_surface_ state as this
  // should be determined in DisplayQueue for every frame.
}

const HwcRect<int> &DisplayPlaneState::GetDisplayFrame() const {
  return display_frame_;
}

const HwcRect<float> &DisplayPlaneState::GetSourceCrop() const {
  return source_crop_;
}

void DisplayPlaneState::SetSourceCrop(const HwcRect<float> &crop) {
  source_crop_ = crop;
}

void DisplayPlaneState::ResetSourceRectToDisplayFrame() {
  source_crop_ = HwcRect<float>(display_frame_);
}

void DisplayPlaneState::AddLayer(const OverlayLayer *layer) {
  const HwcRect<int> &display_frame = layer->GetDisplayFrame();
  display_frame_.left = std::min(display_frame_.left, display_frame.left);
  display_frame_.top = std::min(display_frame_.top, display_frame.top);
  display_frame_.right = std::max(display_frame_.right, display_frame.right);
  display_frame_.bottom = std::max(display_frame_.bottom, display_frame.bottom);

  source_layers_.emplace_back(layer->GetZorder());

  state_ = State::kRender;
  has_cursor_layer_ = layer->IsCursorLayer();

  if (source_layers_.size() == 1 && has_cursor_layer_) {
    type_ = PlaneType::kCursor;
  } else {
    // TODO: Add checks for Video type once our
    // Media backend can support compositing more
    // than one layer together.
    type_ = PlaneType::kNormal;
    apply_effects_ = false;
  }

  if (!use_plane_scalar_)
    source_crop_ = HwcRect<float>(display_frame_);
}

// This API should be called only when Cursor layer is being
// added, is part of layers displayed by plane or is being
// removed in this frame. AddLayers should be used in all
// other cases.
void DisplayPlaneState::AddLayers(const std::vector<size_t> &source_layers,
                                  const std::vector<OverlayLayer> &layers,
                                  bool ignore_cursor_layer) {
  if (ignore_cursor_layer) {
    size_t lsize = layers.size();
    size_t size = source_layers.size();
    source_layers_.reserve(size);
    has_cursor_layer_ = false;
    bool initialized = false;
    for (const size_t &index : source_layers) {
      if (index >= lsize) {
        continue;
      }

      const OverlayLayer &layer = layers.at(index);
      const HwcRect<int> &df = layer.GetDisplayFrame();
      if (!initialized) {
        display_frame_ = df;
        initialized = true;
      } else {
        display_frame_.left = std::min(display_frame_.left, df.left);
        display_frame_.top = std::min(display_frame_.top, df.top);
        display_frame_.right = std::max(display_frame_.right, df.right);
        display_frame_.bottom = std::max(display_frame_.bottom, df.bottom);
      }

      source_layers_.emplace_back(index);
    }

    if (!use_plane_scalar_)
      source_crop_ = HwcRect<float>(display_frame_);
  } else {
    for (const int &index : source_layers) {
      source_layers_.emplace_back(index);
    }
  }
}

void DisplayPlaneState::UpdateDisplayFrame(const HwcRect<int> &display_frame) {
  display_frame_.left = std::min(display_frame_.left, display_frame.left);
  display_frame_.top = std::min(display_frame_.top, display_frame.top);
  display_frame_.right = std::max(display_frame_.right, display_frame.right);
  display_frame_.bottom = std::max(display_frame_.bottom, display_frame.bottom);

  if (!use_plane_scalar_)
    source_crop_ = HwcRect<float>(display_frame_);
}

void DisplayPlaneState::ForceGPURendering() {
  state_ = State::kRender;
}

void DisplayPlaneState::SetOverlayLayer(const OverlayLayer *layer) {
  layer_ = layer;
}

void DisplayPlaneState::ReUseOffScreenTarget() {
  recycled_surface_ = true;
}

bool DisplayPlaneState::SurfaceRecycled() const {
  return recycled_surface_;
}

const OverlayLayer *DisplayPlaneState::GetOverlayLayer() const {
  return layer_;
}

void DisplayPlaneState::SetOffScreenTarget(NativeSurface *target) {
  surfaces_.emplace(surfaces_.begin(), target);
}

NativeSurface *DisplayPlaneState::GetOffScreenTarget() const {
  if (surfaces_.size() == 0) {
    return NULL;
  }

  return surfaces_.at(0);
}

void DisplayPlaneState::TransferSurfaces(
    const std::vector<NativeSurface *> &surfaces, bool swap_front_buffer) {
  size_t size = surfaces.size();
  source_layers_.reserve(size);
  if (size < 3 || !swap_front_buffer) {
    for (uint32_t i = 0; i < size; i++) {
      surfaces_.emplace_back(surfaces.at(i));
    }
  } else {
    // Lets make sure front buffer is now back in the list.
    surfaces_.emplace_back(surfaces.at(1));
    surfaces_.emplace_back(surfaces.at(2));
    surfaces_.emplace_back(surfaces.at(0));
  }

  NativeSurface *surface = surfaces_.at(0);
  surface->SetInUse(true);
  SetOverlayLayer(surface->GetLayer());

  if (surfaces_.size() == 3) {
    surface_swapped_ = swap_front_buffer;
  } else {
    // We will be using an empty buffer, no need to
    // swap buffer in this case.
    surface_swapped_ = true;
  }
}

void DisplayPlaneState::SwapSurfaceIfNeeded() {
  if (surface_swapped_) {
    return;
  }

  std::vector<NativeSurface *> temp;
  temp.reserve(surfaces_.size());
  temp.emplace_back(surfaces_.at(1));
  temp.emplace_back(surfaces_.at(2));
  temp.emplace_back(surfaces_.at(0));
  temp.swap(surfaces_);
  surface_swapped_ = true;
  NativeSurface *surface = surfaces_.at(0);
  surface->SetInUse(true);
  SetOverlayLayer(surface->GetLayer());
}

const std::vector<NativeSurface *> &DisplayPlaneState::GetSurfaces() const {
  return surfaces_;
}

void DisplayPlaneState::ReleaseSurfaces() {
  for (NativeSurface *surface : surfaces_) {
    surface->SetInUse(false);
  }

  std::vector<NativeSurface *>().swap(surfaces_);
}

DisplayPlane *DisplayPlaneState::GetDisplayPlane() const {
  return plane_;
}

const std::vector<size_t> &DisplayPlaneState::GetSourceLayers() const {
  return source_layers_;
}

std::vector<CompositionRegion> &DisplayPlaneState::GetCompositionRegion() {
  return composition_region_;
}

const std::vector<CompositionRegion> &DisplayPlaneState::GetCompositionRegion()
    const {
  return composition_region_;
}

bool DisplayPlaneState::IsCursorPlane() const {
  return type_ == PlaneType::kCursor;
}

bool DisplayPlaneState::HasCursorLayer() const {
  return has_cursor_layer_;
}

bool DisplayPlaneState::IsVideoPlane() const {
  return type_ == PlaneType::kVideo;
}

void DisplayPlaneState::SetVideoPlane() {
  type_ = PlaneType::kVideo;
}

void DisplayPlaneState::UsePlaneScalar(bool enable) {
  use_plane_scalar_ = enable;
}

bool DisplayPlaneState::IsUsingPlaneScalar() const {
  return use_plane_scalar_;
}

void DisplayPlaneState::SetApplyEffects(bool apply_effects) {
  apply_effects_ = apply_effects;
  // Doesn't have any impact on planes which
  // are not meant for video purpose.
  if (type_ != PlaneType::kVideo) {
    apply_effects_ = false;
  }
}

bool DisplayPlaneState::ApplyEffects() const {
  return apply_effects_;
}

bool DisplayPlaneState::Scanout() const {
  if (recycled_surface_) {
    return true;
  }

  if (apply_effects_) {
    return false;
  }

  return state_ == State::kScanout;
}

bool DisplayPlaneState::NeedsOffScreenComposition() {
  if (state_ == State::kRender)
    return true;

  if (recycled_surface_) {
    return true;
  }

  if (apply_effects_) {
    return true;
  }

  return false;
}

}  // namespace hwcomposer
