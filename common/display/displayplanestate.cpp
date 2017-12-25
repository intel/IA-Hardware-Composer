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
                                     uint32_t index) {
  private_data_ = std::make_shared<DisplayPlanePrivateState>();
  private_data_->source_layers_.emplace_back(index);
  private_data_->display_frame_ = layer->GetDisplayFrame();
  private_data_->source_crop_ = layer->GetSourceCrop();
  if (layer->IsCursorLayer()) {
    private_data_->type_ = DisplayPlanePrivateState::PlaneType::kCursor;
    private_data_->has_cursor_layer_ = true;
  }

  plane->SetInUse(true);
  private_data_->plane_ = plane;
  private_data_->layer_ = layer;
}

void DisplayPlaneState::CopyState(DisplayPlaneState &state) {
  private_data_ = state.private_data_;
  // We don't copy recycled_surface_ state as this
  // should be determined in DisplayQueue for every frame.
}

const HwcRect<int> &DisplayPlaneState::GetDisplayFrame() const {
  return private_data_->display_frame_;
}

const HwcRect<float> &DisplayPlaneState::GetSourceCrop() const {
  return private_data_->source_crop_;
}

void DisplayPlaneState::SetSourceCrop(const HwcRect<float> &crop) {
  private_data_->source_crop_ = crop;
}

void DisplayPlaneState::ResetSourceRectToDisplayFrame() {
  private_data_->source_crop_ = HwcRect<float>(private_data_->display_frame_);
}

void DisplayPlaneState::AddLayer(const OverlayLayer *layer) {
  const HwcRect<int> &display_frame = layer->GetDisplayFrame();
  HwcRect<int> &target_display_frame = private_data_->display_frame_;
  target_display_frame.left =
      std::min(target_display_frame.left, display_frame.left);
  target_display_frame.top =
      std::min(target_display_frame.top, display_frame.top);
  target_display_frame.right =
      std::max(target_display_frame.right, display_frame.right);
  target_display_frame.bottom =
      std::max(target_display_frame.bottom, display_frame.bottom);

  private_data_->source_layers_.emplace_back(layer->GetZorder());

  private_data_->state_ = DisplayPlanePrivateState::State::kRender;
  private_data_->has_cursor_layer_ = layer->IsCursorLayer();

  if (private_data_->source_layers_.size() == 1 &&
      private_data_->has_cursor_layer_) {
    private_data_->type_ = DisplayPlanePrivateState::PlaneType::kCursor;
  } else {
    // TODO: Add checks for Video type once our
    // Media backend can support compositing more
    // than one layer together.
    private_data_->type_ = DisplayPlanePrivateState::PlaneType::kNormal;
    private_data_->apply_effects_ = false;
  }

  if (!private_data_->use_plane_scalar_)
    private_data_->source_crop_ = HwcRect<float>(private_data_->display_frame_);
}

void DisplayPlaneState::ResetLayers(const std::vector<OverlayLayer> &layers) {
  const std::vector<size_t> &current_layers = private_data_->source_layers_;
  size_t lsize = layers.size();
  size_t size = current_layers.size();
  std::vector<size_t> source_layers;
  source_layers.reserve(size);
  bool had_cursor = private_data_->has_cursor_layer_;
  private_data_->has_cursor_layer_ = false;
  bool initialized = false;
  HwcRect<int> target_display_frame;
  bool use_scalar = false;
  bool has_video = false;
  for (const size_t &index : current_layers) {
    if (index >= lsize) {
      break;
    }

    const OverlayLayer &layer = layers.at(index);
    bool is_cursor = layer.IsCursorLayer();
    if (!had_cursor && is_cursor) {
      continue;
    }

    if (is_cursor) {
      private_data_->has_cursor_layer_ = true;
    } else if (!has_video) {
      has_video = layer.IsVideoLayer();
    }

    if (!use_scalar) {
      use_scalar = layer.IsUsingPlaneScalar();
    }

    const HwcRect<int> &df = layer.GetDisplayFrame();
    if (!initialized) {
      target_display_frame = df;
      initialized = true;
    } else {
      target_display_frame.left = std::min(target_display_frame.left, df.left);
      target_display_frame.top = std::min(target_display_frame.top, df.top);
      target_display_frame.right =
          std::max(target_display_frame.right, df.right);
      target_display_frame.bottom =
          std::max(target_display_frame.bottom, df.bottom);
    }

    source_layers.emplace_back(layer.GetZorder());
  }

  // If this plane contains cursor layer, this should be the top
  // most plane.
  if (had_cursor && !private_data_->has_cursor_layer_) {
    const OverlayLayer &layer = layers.at(lsize - 1);
    if (layer.IsCursorLayer()) {
      const HwcRect<int> &df = layer.GetDisplayFrame();
      if (source_layers.empty()) {
        target_display_frame = df;
      } else {
        target_display_frame.left =
            std::min(target_display_frame.left, df.left);
        target_display_frame.top = std::min(target_display_frame.top, df.top);
        target_display_frame.right =
            std::max(target_display_frame.right, df.right);
        target_display_frame.bottom =
            std::max(target_display_frame.bottom, df.bottom);
      }
      source_layers.emplace_back(layer.GetZorder());
      private_data_->has_cursor_layer_ = true;
    }
  }

  private_data_->source_layers_.swap(source_layers);
  private_data_->display_frame_ = target_display_frame;

  if (use_scalar) {
    // In case we previously relied on display scalar, we leave that
    // state untouched as it needs to be handled during
    // ValidateForDisplayScaling in planemanager.
    private_data_->use_plane_scalar_ = true;
  }

  if (!private_data_->use_plane_scalar_)
    private_data_->source_crop_ = HwcRect<float>(target_display_frame);

  if (private_data_->source_layers_.size() == 1) {
    if (private_data_->has_cursor_layer_) {
      private_data_->type_ = DisplayPlanePrivateState::PlaneType::kCursor;
    } else if (has_video) {
      private_data_->type_ = DisplayPlanePrivateState::PlaneType::kVideo;
    } else {
      private_data_->type_ = DisplayPlanePrivateState::PlaneType::kNormal;
    }
  } else {
    private_data_->type_ = DisplayPlanePrivateState::PlaneType::kNormal;
  }

  std::vector<CompositionRegion>().swap(private_data_->composition_region_);
}

void DisplayPlaneState::UpdateDisplayFrame(const HwcRect<int> &display_frame) {
  HwcRect<int> &target_display_frame = private_data_->display_frame_;
  target_display_frame.left =
      std::min(target_display_frame.left, display_frame.left);
  target_display_frame.top =
      std::min(target_display_frame.top, display_frame.top);
  target_display_frame.right =
      std::max(target_display_frame.right, display_frame.right);
  target_display_frame.bottom =
      std::max(target_display_frame.bottom, display_frame.bottom);

  if (!private_data_->use_plane_scalar_)
    private_data_->source_crop_ = HwcRect<float>(target_display_frame);
}

void DisplayPlaneState::ForceGPURendering() {
  private_data_->state_ = DisplayPlanePrivateState::State::kRender;
}

void DisplayPlaneState::SetOverlayLayer(const OverlayLayer *layer) {
  private_data_->layer_ = layer;
}

void DisplayPlaneState::ReUseOffScreenTarget() {
  recycled_surface_ = true;
}

bool DisplayPlaneState::SurfaceRecycled() const {
  return recycled_surface_;
}

const OverlayLayer *DisplayPlaneState::GetOverlayLayer() const {
  return private_data_->layer_;
}

void DisplayPlaneState::SetOffScreenTarget(NativeSurface *target) {
  private_data_->layer_ = target->GetLayer();
  target->ResetDisplayFrame(private_data_->display_frame_);
  target->ResetSourceCrop(private_data_->source_crop_);
  private_data_->surfaces_.emplace(private_data_->surfaces_.begin(), target);
  target->GetLayer()->UsePlaneScalar(private_data_->use_plane_scalar_);
  recycled_surface_ = false;
}

NativeSurface *DisplayPlaneState::GetOffScreenTarget() const {
  if (private_data_->surfaces_.size() == 0) {
    return NULL;
  }

  return private_data_->surfaces_.at(0);
}

void DisplayPlaneState::SwapSurface() {
  surface_swapped_ = false;
  SwapSurfaceIfNeeded();
}

void DisplayPlaneState::SwapSurfaceIfNeeded() {
  if (surface_swapped_) {
    return;
  }

  size_t size = private_data_->surfaces_.size();

  if (size == 3) {
    std::vector<NativeSurface *> temp;
    temp.reserve(size);
    // Lets make sure front buffer is now back in the list.
    temp.emplace_back(private_data_->surfaces_.at(1));
    temp.emplace_back(private_data_->surfaces_.at(2));
    temp.emplace_back(private_data_->surfaces_.at(0));
    private_data_->surfaces_.swap(temp);
  }

  surface_swapped_ = true;
  recycled_surface_ = false;
  NativeSurface *surface = private_data_->surfaces_.at(0);
  surface->SetInUse(true);
  private_data_->layer_ = surface->GetLayer();
}

const std::vector<NativeSurface *> &DisplayPlaneState::GetSurfaces() const {
  return private_data_->surfaces_;
}

void DisplayPlaneState::ReleaseSurfaces(bool only_release) {
  if (!only_release) {
    for (NativeSurface *surface : private_data_->surfaces_) {
      surface->SetInUse(false);
    }
  }

  std::vector<NativeSurface *>().swap(private_data_->surfaces_);
}

void DisplayPlaneState::RefreshSurfaces(bool clear_surface) {
  const HwcRect<int> &target_display_frame = private_data_->display_frame_;
  const HwcRect<float> &target_src_rect = private_data_->source_crop_;
  bool use_scalar = private_data_->use_plane_scalar_;
  for (NativeSurface *surface : private_data_->surfaces_) {
    surface->ResetDisplayFrame(target_display_frame);
    surface->ResetSourceCrop(target_src_rect);
    surface->UpdateSurfaceDamage(target_src_rect, target_src_rect);
    if (!surface->ClearSurface())
      surface->SetClearSurface(clear_surface);
    surface->GetLayer()->UsePlaneScalar(use_scalar);
  }

  std::vector<CompositionRegion>().swap(private_data_->composition_region_);
}

DisplayPlane *DisplayPlaneState::GetDisplayPlane() const {
  return private_data_->plane_;
}

const std::vector<size_t> &DisplayPlaneState::GetSourceLayers() const {
  return private_data_->source_layers_;
}

std::vector<CompositionRegion> &DisplayPlaneState::GetCompositionRegion() {
  return private_data_->composition_region_;
}

void DisplayPlaneState::ResetCompositionRegion() {
  std::vector<CompositionRegion>().swap(private_data_->composition_region_);
}

bool DisplayPlaneState::IsCursorPlane() const {
  return private_data_->type_ == DisplayPlanePrivateState::PlaneType::kCursor;
}

bool DisplayPlaneState::HasCursorLayer() const {
  return private_data_->has_cursor_layer_;
}

bool DisplayPlaneState::IsVideoPlane() const {
  return private_data_->type_ == DisplayPlanePrivateState::PlaneType::kVideo;
}

void DisplayPlaneState::SetVideoPlane() {
  private_data_->type_ = DisplayPlanePrivateState::PlaneType::kVideo;
}

void DisplayPlaneState::UsePlaneScalar(bool enable) {
  private_data_->use_plane_scalar_ = enable;
}

bool DisplayPlaneState::IsUsingPlaneScalar() const {
  return private_data_->use_plane_scalar_;
}

void DisplayPlaneState::SetApplyEffects(bool apply_effects) {
  private_data_->apply_effects_ = apply_effects;
  // Doesn't have any impact on planes which
  // are not meant for video purpose.
  if (private_data_->type_ != DisplayPlanePrivateState::PlaneType::kVideo) {
    private_data_->apply_effects_ = false;
  }
}

bool DisplayPlaneState::ApplyEffects() const {
  return private_data_->apply_effects_;
}

bool DisplayPlaneState::Scanout() const {
  if (recycled_surface_) {
    return true;
  }

  if (private_data_->apply_effects_) {
    return false;
  }

  return private_data_->state_ == DisplayPlanePrivateState::State::kScanout;
}

bool DisplayPlaneState::NeedsOffScreenComposition() {
  if (private_data_->state_ == DisplayPlanePrivateState::State::kRender)
    return true;

  if (recycled_surface_) {
    return true;
  }

  if (private_data_->apply_effects_) {
    return true;
  }

  return false;
}

}  // namespace hwcomposer
