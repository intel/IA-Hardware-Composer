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
#include "hwctrace.h"

namespace hwcomposer {

DisplayPlaneState::DisplayPlaneState(DisplayPlane *plane, OverlayLayer *layer,
                                     uint32_t index, uint32_t plane_transform) {
  private_data_ = std::make_shared<DisplayPlanePrivateState>();
  private_data_->source_layers_.emplace_back(index);
  private_data_->display_frame_ = layer->GetDisplayFrame();
  private_data_->rect_updated_ = true;
  private_data_->source_crop_ = layer->GetSourceCrop();
  if (layer->IsCursorLayer()) {
    private_data_->type_ = DisplayPlanePrivateState::PlaneType::kCursor;
    private_data_->has_cursor_layer_ = true;
  }

  plane->SetInUse(true);
  private_data_->plane_ = plane;
  private_data_->layer_ = layer;
  private_data_->plane_transform_ = plane_transform;
  if (!private_data_->plane_->IsSupportedTransform(plane_transform)) {
    private_data_->rotation_type_ =
        DisplayPlaneState::RotationType::kGPURotation;
    private_data_->unsupported_siplay_rotation_ = true;
  } else {
    private_data_->rotation_type_ = RotationType::kDisplayRotation;
  }
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

void DisplayPlaneState::AddLayer(const OverlayLayer *layer) {
  const HwcRect<int> &display_frame = layer->GetDisplayFrame();
  HwcRect<int> target_display_frame = private_data_->display_frame_;
  target_display_frame.left =
      std::min(target_display_frame.left, display_frame.left);
  target_display_frame.top =
      std::min(target_display_frame.top, display_frame.top);
  target_display_frame.right =
      std::max(target_display_frame.right, display_frame.right);
  target_display_frame.bottom =
      std::max(target_display_frame.bottom, display_frame.bottom);

  HwcRect<float> target_source_crop = private_data_->source_crop_;
  const HwcRect<float> &source_crop = layer->GetSourceCrop();
  target_source_crop.left = std::min(target_source_crop.left, source_crop.left);
  target_source_crop.top = std::min(target_source_crop.top, source_crop.top);
  target_source_crop.right =
      std::max(target_source_crop.right, source_crop.right);
  target_source_crop.bottom =
      std::max(target_source_crop.bottom, source_crop.bottom);

  private_data_->source_layers_.emplace_back(layer->GetZorder());

  private_data_->state_ = DisplayPlanePrivateState::State::kRender;

  // If layers are less than 2, we need to enforce rect checks as
  // we shouldn't have done them yet (i.e. Previous state could have
  // been direct scanout.)
  bool rect_updated = true;
  if (private_data_->source_layers_.size() > 2 &&
      (private_data_->display_frame_ == target_display_frame) &&
      ((private_data_->source_crop_ == target_source_crop))) {
    rect_updated = false;
  } else {
    private_data_->display_frame_ = target_display_frame;
    private_data_->source_crop_ = target_source_crop;
  }

  if (!private_data_->rect_updated_)
    private_data_->rect_updated_ = rect_updated;

  if (!private_data_->has_cursor_layer_)
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

  // Reset Validation state.
  if (re_validate_layer_ & ReValidationType::kScanout)
    re_validate_layer_ &= ~ReValidationType::kScanout;

  refresh_needed_ = true;
}

void DisplayPlaneState::ResetLayers(const std::vector<OverlayLayer> &layers,
                                    size_t remove_index) {
  const std::vector<size_t> &current_layers = private_data_->source_layers_;
  std::vector<size_t> source_layers;
  bool had_cursor = private_data_->has_cursor_layer_;
  private_data_->has_cursor_layer_ = false;
  bool initialized = false;
  HwcRect<int> target_display_frame;
  HwcRect<float> target_source_crop;
  bool has_video = false;
  for (const size_t &index : current_layers) {
    if (index >= remove_index) {
#ifdef SURFACE_TRACING
      ISURFACETRACE("Reset breaks index: %d remove_index %d \n", index,
                    remove_index);
#endif
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

    const HwcRect<int> &df = layer.GetDisplayFrame();
    const HwcRect<float> &source_crop = layer.GetSourceCrop();
    if (!initialized) {
      target_display_frame = df;
      target_source_crop = source_crop;
      initialized = true;
    } else {
      target_display_frame.left = std::min(target_display_frame.left, df.left);
      target_display_frame.top = std::min(target_display_frame.top, df.top);
      target_display_frame.right =
          std::max(target_display_frame.right, df.right);
      target_display_frame.bottom =
          std::max(target_display_frame.bottom, df.bottom);

      target_source_crop.left =
          std::min(target_source_crop.left, source_crop.left);
      target_source_crop.top =
          std::min(target_source_crop.top, source_crop.top);
      target_source_crop.right =
          std::max(target_source_crop.right, source_crop.right);
      target_source_crop.bottom =
          std::max(target_source_crop.bottom, source_crop.bottom);
    }
#ifdef SURFACE_TRACING
    ISURFACETRACE("Reset adds index: %d \n", layer.GetZorder());
#endif
    source_layers.emplace_back(layer.GetZorder());
  }

  if (source_layers.empty()) {
    private_data_->source_layers_.swap(source_layers);
    return;
  }

#ifdef SURFACE_TRACING
  ISURFACETRACE(
      "Reset called has_video: %d Source Layers Size: %d Previous Source "
      "Layers Size: %d Has Cursor: %d Total Layers Size: %d \n",
      has_video, source_layers.size(), current_layers.size(),
      private_data_->has_cursor_layer_, layers.size());
#endif

  private_data_->source_layers_.swap(source_layers);
  bool rect_updated = true;
  if ((private_data_->display_frame_ == target_display_frame) &&
      ((private_data_->source_crop_ == target_source_crop))) {
    rect_updated = false;
  } else {
    private_data_->display_frame_ = target_display_frame;
    private_data_->source_crop_ = target_source_crop;
  }

  if (!private_data_->rect_updated_)
    private_data_->rect_updated_ = rect_updated;

  if (private_data_->source_layers_.size() == 1) {
    if (private_data_->has_cursor_layer_) {
      private_data_->type_ = DisplayPlanePrivateState::PlaneType::kCursor;
    } else if (has_video) {
      private_data_->type_ = DisplayPlanePrivateState::PlaneType::kVideo;
    } else {
      private_data_->type_ = DisplayPlanePrivateState::PlaneType::kNormal;
    }

    if (!has_video)
      re_validate_layer_ |= ReValidationType::kScanout;
  } else {
    private_data_->type_ = DisplayPlanePrivateState::PlaneType::kNormal;
  }

  std::vector<CompositionRegion>().swap(private_data_->composition_region_);
  refresh_needed_ = true;
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
}

void DisplayPlaneState::UpdateSourceCrop(const HwcRect<float> &source_crop) {
  HwcRect<float> &target_source_crop = private_data_->source_crop_;
  target_source_crop.left = std::min(target_source_crop.left, source_crop.left);
  target_source_crop.top = std::min(target_source_crop.top, source_crop.top);
  target_source_crop.right =
      std::max(target_source_crop.right, source_crop.right);
  target_source_crop.bottom =
      std::max(target_source_crop.bottom, source_crop.bottom);
}

void DisplayPlaneState::ForceGPURendering() {
  private_data_->state_ = DisplayPlanePrivateState::State::kRender;
}

void DisplayPlaneState::DisableGPURendering() {
  private_data_->state_ = DisplayPlanePrivateState::State::kScanout;
}

void DisplayPlaneState::SetOverlayLayer(const OverlayLayer *layer) {
  private_data_->layer_ = layer;
}

void DisplayPlaneState::ReUseOffScreenTarget() {
  if (surface_swapped_) {
    ETRACE(
        "Surface has been swapped and being re-used as offscreen target. \n");
  }
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
  uint32_t rotation = private_data_->plane_transform_;
  if (private_data_->rotation_type_ != RotationType::kDisplayRotation)
    rotation = kIdentity;

  target->SetTransform(rotation);

  target->ResetDisplayFrame(private_data_->display_frame_);
  if (private_data_->use_plane_scalar_) {
    target->ResetSourceCrop(private_data_->source_crop_);
  } else {
    target->ResetSourceCrop(HwcRect<float>(private_data_->display_frame_));
  }
  private_data_->surfaces_.emplace(private_data_->surfaces_.begin(), target);
  recycled_surface_ = false;
  refresh_needed_ = true;
  if (private_data_->surfaces_.size() == 1) {
    refresh_needed_ = false;
  }

  surface_swapped_ = true;
}

NativeSurface *DisplayPlaneState::GetOffScreenTarget() const {
  if (private_data_->surfaces_.size() == 0) {
    return NULL;
  }

  return private_data_->surfaces_.at(0);
}

void DisplayPlaneState::SwapSurfaceIfNeeded() {
  if (surface_swapped_) {
    if (recycled_surface_) {
      ETRACE(
          "Surface has been swapped and being re-used as offscreen target. \n");
    }
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

void DisplayPlaneState::ReleaseSurfaces() {
  std::vector<NativeSurface *>().swap(private_data_->surfaces_);
}

void DisplayPlaneState::RefreshSurfaces(NativeSurface::ClearType clear_surface,
                                        bool force) {
  if (!refresh_needed_ && !private_data_->rect_updated_ && !force) {
    return;
  }

  const HwcRect<int> &target_display_frame = private_data_->display_frame_;
  const HwcRect<float> &target_src_rect = private_data_->source_crop_;
  for (NativeSurface *surface : private_data_->surfaces_) {
    surface->ResetDisplayFrame(target_display_frame);
    if (private_data_->use_plane_scalar_) {
      surface->ResetSourceCrop(target_src_rect);
    } else {
      surface->ResetSourceCrop(HwcRect<float>(target_display_frame));
    }

    bool clear = surface->ClearSurface();
    bool partial_clear = surface->IsPartialClear();

    if (!clear && !partial_clear) {
      surface->SetClearSurface(clear_surface);
    } else if (!clear && clear_surface == NativeSurface::kPartialClear) {
      surface->SetClearSurface(clear_surface);
    } else if (partial_clear && clear_surface == NativeSurface::kFullClear) {
      surface->SetClearSurface(NativeSurface::kFullClear);
    }

    if (surface->ClearSurface()) {
      if (private_data_->use_plane_scalar_) {
        surface->UpdateSurfaceDamage(target_src_rect, true);
      } else {
        surface->UpdateSurfaceDamage(target_display_frame, true);
      }
    }
  }

  refresh_needed_ = false;
  recycled_surface_ = false;
  if (private_data_->rect_updated_) {
    ValidateReValidation();
  }
}

void DisplayPlaneState::UpdateDamage(const HwcRect<int> &surface_damage,
                                     bool forced) {
  for (NativeSurface *surface : private_data_->surfaces_) {
    surface->UpdateSurfaceDamage(surface_damage, forced);
  }
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
  if (!private_data_->composition_region_.empty())
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

void DisplayPlaneState::UsePlaneScalar(bool enable, bool force_refresh) {
  if (private_data_->use_plane_scalar_ != enable) {
    private_data_->use_plane_scalar_ = enable;
    if (force_refresh) {
      RefreshSurfaces(NativeSurface::kFullClear, true);
    } else {
      const HwcRect<int> &target_display_frame = private_data_->display_frame_;
      const HwcRect<float> &target_src_rect = private_data_->source_crop_;
      for (NativeSurface *surface : private_data_->surfaces_) {
        surface->ResetDisplayFrame(target_display_frame);
        if (private_data_->use_plane_scalar_) {
          surface->ResetSourceCrop(target_src_rect);
        } else {
          surface->ResetSourceCrop(HwcRect<float>(target_display_frame));
        }

        if (surface->ClearSurface()) {
          if (private_data_->use_plane_scalar_) {
            surface->UpdateSurfaceDamage(target_src_rect, true);
          } else {
            surface->UpdateSurfaceDamage(target_display_frame, true);
          }
        }
      }
    }
  }
}

bool DisplayPlaneState::IsUsingPlaneScalar() const {
  return private_data_->use_plane_scalar_;
}

void DisplayPlaneState::SetApplyEffects(bool apply_effects) {
  if (private_data_->apply_effects_ != apply_effects) {
    private_data_->apply_effects_ = apply_effects;
    recycled_surface_ = false;
    // Doesn't have any impact on planes which
    // are not meant for video.
    if (apply_effects &&
        private_data_->type_ != DisplayPlanePrivateState::PlaneType::kVideo) {
      private_data_->apply_effects_ = false;
    }
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

bool DisplayPlaneState::NeedsOffScreenComposition() const {
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

uint32_t DisplayPlaneState::RevalidationType() const {
  return re_validate_layer_;
}

void DisplayPlaneState::RevalidationDone(uint32_t validation_done) {
  if (validation_done & ReValidationType::kScanout) {
    re_validate_layer_ &= ~ReValidationType::kScanout;
  }

  if (validation_done & ReValidationType::kScalar) {
    re_validate_layer_ &= ~ReValidationType::kScalar;
  }

  if (validation_done & ReValidationType::kRotation) {
    re_validate_layer_ &= ~ReValidationType::kRotation;
  }
}

bool DisplayPlaneState::CanSquash() const {
  if (private_data_->state_ == DisplayPlanePrivateState::State::kScanout)
    return false;

  if (private_data_->type_ == DisplayPlanePrivateState::PlaneType::kVideo)
    return false;

  return true;
}

void DisplayPlaneState::ValidateReValidation() {
  if (!private_data_->rect_updated_)
    return;

  if (private_data_->plane_transform_ != kIdentity &&
      !private_data_->unsupported_siplay_rotation_) {
    re_validate_layer_ |= ReValidationType::kRotation;
  }

  if (private_data_->source_layers_.size() == 1 &&
      !(private_data_->type_ == DisplayPlanePrivateState::PlaneType::kVideo)) {
    re_validate_layer_ |= ReValidationType::kScanout;
  } else {
    bool use_scalar = CanUseDisplayUpScaling();
    if (private_data_->use_plane_scalar_ != use_scalar) {
      re_validate_layer_ |= ReValidationType::kScalar;
    }
  }

  private_data_->rect_updated_ = false;
}

bool DisplayPlaneState::CanUseDisplayUpScaling() const {
  // TODO: Handle case where all layers to be compoisted have same scaling
  // ratio.
  // We cannot use plane scaling for Layers with different scaling ratio.
  if (private_data_->source_layers_.size() > 1) {
    return false;
  }

  if (!private_data_->rect_updated_) {
    return private_data_->can_use_display_scalar_;
  }

  const HwcRect<int> &target_display_frame = private_data_->display_frame_;
  const HwcRect<float> &target_src_rect = private_data_->source_crop_;

  uint32_t display_frame_width =
      target_display_frame.right - target_display_frame.left;
  uint32_t display_frame_height =
      target_display_frame.bottom - target_display_frame.top;
  uint32_t source_crop_width = static_cast<uint32_t>(
      ceilf(target_src_rect.right - target_src_rect.left));
  uint32_t source_crop_height = static_cast<uint32_t>(
      ceilf(target_src_rect.bottom - target_src_rect.top));
  // Source and Display frame width, height are same and scaling is not needed.
  if ((display_frame_width == source_crop_width) &&
      (display_frame_height == source_crop_height)) {
    private_data_->can_use_display_scalar_ = false;
  }

  // Display frame width, height is lesser than Source. Let's downscale
  // it with our compositor backend.
  if ((display_frame_width < source_crop_width) &&
      (display_frame_height < source_crop_height)) {
    private_data_->can_use_display_scalar_ = false;
  }

  // Display frame height is less. If the cost of upscaling width is less
  // than downscaling height, than return.
  if ((display_frame_width > source_crop_width) &&
      (display_frame_height < source_crop_height)) {
    uint32_t width_cost =
        (display_frame_width - source_crop_width) * display_frame_height;
    uint32_t height_cost =
        (source_crop_height - display_frame_height) * display_frame_width;
    if (height_cost > width_cost) {
      private_data_->can_use_display_scalar_ = false;
    }
  }

  // Display frame width is less. If the cost of upscaling height is less
  // than downscaling width, than return.
  if ((display_frame_width < source_crop_width) &&
      (display_frame_height > source_crop_height)) {
    uint32_t width_cost =
        (source_crop_width - display_frame_width) * display_frame_height;
    uint32_t height_cost =
        (display_frame_height - source_crop_height) * display_frame_width;
    if (width_cost > height_cost) {
      private_data_->can_use_display_scalar_ = false;
    }
  }

  private_data_->can_use_display_scalar_ = true;

  return private_data_->can_use_display_scalar_;
}

void DisplayPlaneState::SetRotationType(RotationType type, bool refresh) {
  if (private_data_->rotation_type_ != type) {
    private_data_->rotation_type_ = type;
    if (refresh) {
      RefreshSurfaces(NativeSurface::kFullClear, true);
    }

    uint32_t rotation = private_data_->plane_transform_;
    if (type != RotationType::kDisplayRotation)
      rotation = kIdentity;

    for (NativeSurface *surface : private_data_->surfaces_) {
      surface->SetTransform(rotation);
    }
  }
}

DisplayPlaneState::RotationType DisplayPlaneState::GetRotationType() const {
  return private_data_->rotation_type_;
}

void DisplayPlaneState::PlaneRectUpdated() {
  private_data_->rect_updated_ = true;
}

}  // namespace hwcomposer
