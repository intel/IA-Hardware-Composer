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

#include "overlaylayer.h"

#include <cmath>

#include <drm_mode.h>
#include <hwctrace.h>

#include "hwcutils.h"

#include "resourcemanager.h"

namespace hwcomposer {

OverlayLayer::ImportedBuffer::~ImportedBuffer() {
  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }
}

OverlayLayer::ImportedBuffer::ImportedBuffer(
    std::shared_ptr<OverlayBuffer>& buffer, int32_t acquire_fence)
    : acquire_fence_(acquire_fence) {
  buffer_ = buffer;
}

void OverlayLayer::SetAcquireFence(int32_t acquire_fence) {
  // Release any existing fence.
  if (imported_buffer_->acquire_fence_ > 0) {
    close(imported_buffer_->acquire_fence_);
  }

  imported_buffer_->acquire_fence_ = acquire_fence;
}

int32_t OverlayLayer::GetAcquireFence() const {
  return imported_buffer_->acquire_fence_;
}

int32_t OverlayLayer::ReleaseAcquireFence() const {
  int32_t fence = imported_buffer_->acquire_fence_;
  imported_buffer_->acquire_fence_ = -1;
  return fence;
}

OverlayBuffer* OverlayLayer::GetBuffer() const {
  if (imported_buffer_->buffer_.get() == NULL)
    ETRACE("hwc layer get NullBuffer");
  return imported_buffer_->buffer_.get();
}

void OverlayLayer::SetBuffer(HWCNativeHandle handle, int32_t acquire_fence,
                             ResourceManager* resource_manager,
                             bool register_buffer) {
  std::shared_ptr<OverlayBuffer> buffer(NULL);

  if (resource_manager) {
    buffer = resource_manager->FindCachedBuffer(GETNATIVEBUFFER(handle));
  }

  if (buffer == NULL) {
    buffer = OverlayBuffer::CreateOverlayBuffer();
    buffer->InitializeFromNativeHandle(handle, resource_manager,
                                       register_buffer);
    if (register_buffer) {
      resource_manager->RegisterBuffer(GETNATIVEBUFFER(handle), buffer);
    }
  }
  imported_buffer_.reset(new ImportedBuffer(buffer, acquire_fence));
}

void OverlayLayer::SetBlending(HWCBlending blending) {
  blending_ = blending;
}

void OverlayLayer::SetSourceCrop(const HwcRect<float>& source_crop) {
  source_crop_width_ = static_cast<int>(ceilf(source_crop.right) -
                                        static_cast<int>(source_crop.left));
  source_crop_height_ = static_cast<int>(ceilf(source_crop.bottom) -
                                         static_cast<int>(source_crop.top));
  source_crop_ = source_crop;
}

void OverlayLayer::SetDisplayFrame(const HwcRect<int>& display_frame) {
  display_frame_width_ = display_frame.right - display_frame.left;
  display_frame_height_ = display_frame.bottom - display_frame.top;
  display_frame_ = display_frame;
  surface_damage_ = display_frame;
}

void OverlayLayer::ValidateTransform(uint32_t transform,
                                     uint32_t display_transform) {
  if (transform & kTransform90) {
    if (transform & kReflectX) {
      plane_transform_ |= kReflectX;
    }

    if (transform & kReflectY) {
      plane_transform_ |= kReflectY;
    }

    switch (display_transform) {
      case kRotate90:
        plane_transform_ |= kTransform180;
        break;
      case kRotate180:
        plane_transform_ |= kTransform270;
        break;
      case kRotateNone:
        plane_transform_ |= kTransform90;
        break;
      default:
        break;
    }
  } else if (transform & kTransform180) {
    switch (display_transform) {
      case kRotate90:
        plane_transform_ |= kTransform270;
        break;
      case kRotate270:
        plane_transform_ |= kTransform90;
        break;
      case kRotateNone:
        plane_transform_ |= kTransform180;
        break;
      default:
        break;
    }
  } else if (transform & kTransform270) {
    switch (display_transform) {
      case kRotate270:
        plane_transform_ |= kTransform180;
        break;
      case kRotate180:
        plane_transform_ |= kTransform90;
        break;
      case kRotateNone:
        plane_transform_ |= kTransform270;
        break;
      default:
        break;
    }
  } else {
    if (display_transform == kRotate90) {
      if (transform & kReflectX) {
        plane_transform_ |= kReflectX;
      }

      if (transform & kReflectY) {
        plane_transform_ |= kReflectY;
      }

      plane_transform_ |= kTransform90;
    } else {
      switch (display_transform) {
        case kRotate270:
          plane_transform_ |= kTransform270;
          break;
        case kRotate180:
          plane_transform_ |= kReflectY;
          break;
        default:
          break;
      }
    }
  }
}

void OverlayLayer::UpdateSurfaceDamage(HwcLayer* /*layer*/) {
  if (!gpu_rendered_) {
    surface_damage_ = display_frame_;
    return;
  }

  if ((state_ & kClearSurface) || (state_ & kDimensionsChanged) ||
      (transform_ != kIdentity)) {
    surface_damage_ = display_frame_;
    return;
  }

  // TODO: FIXME: We should be able to use surfacedamage
  // from HWCLayer here.
  surface_damage_ = display_frame_;
}

void OverlayLayer::InitializeState(HwcLayer* layer,
                                   ResourceManager* resource_manager,
                                   OverlayLayer* previous_layer,
                                   uint32_t z_order, uint32_t layer_index,
                                   uint32_t max_height, HWCRotation rotation,
                                   bool handle_constraints) {
  transform_ = layer->GetTransform();
  if (rotation != kRotateNone) {
    ValidateTransform(layer->GetTransform(), rotation);
    // Remove this in case we enable support in future
    // to apply display rotation at pipe level.
    transform_ = plane_transform_;
  } else {
    plane_transform_ = transform_;
  }

  alpha_ = layer->GetAlpha();
  layer_index_ = layer_index;
  z_order_ = z_order;
  source_crop_width_ = layer->GetSourceCropWidth();
  source_crop_height_ = layer->GetSourceCropHeight();
  source_crop_ = layer->GetSourceCrop();
  blending_ = layer->GetBlending();
  SetBuffer(layer->GetNativeHandle(), layer->GetAcquireFence(),
            resource_manager, true);
  ValidateForOverlayUsage();
  if (previous_layer) {
    ValidatePreviousFrameState(previous_layer, layer);
  }

  if (layer->HasContentAttributesChanged() ||
      layer->HasLayerAttributesChanged() || !layer->IsValidated()) {
    state_ |= kClearSurface;
    state_ |= kLayerContentChanged;
  }

  if (!handle_constraints) {
    UpdateSurfaceDamage(layer);
    return;
  }

  int32_t left_constraint = layer->GetLeftConstraint();
  int32_t right_constraint = layer->GetRightConstraint();
  int32_t left_source_constraint = layer->GetLeftSourceConstraint();
  int32_t right_source_constraint = layer->GetRightSourceConstraint();
  int32_t display_frame_left = display_frame_.left;
  uint32_t frame_width = display_frame_.right - display_frame_.left;
  uint32_t source_width =
      static_cast<int>(source_crop_.right - source_crop_.left);
  uint32_t frame_offset_left = 0;
  uint32_t frame_offset_right = frame_width;
  if (left_constraint >= 0 && right_constraint >= 0) {
    if (display_frame_.left > right_source_constraint) {
      state_ |= kInvisible;
      return;
    }

    if (display_frame_.right < left_source_constraint) {
      state_ |= kInvisible;
      return;
    }

    if (display_frame_.left < left_source_constraint) {
      frame_offset_left = left_source_constraint - display_frame_left;
      display_frame_.left = left_source_constraint;
    }

    if (display_frame_.right > right_source_constraint) {
      frame_offset_right = right_source_constraint - display_frame_left;
      display_frame_.right = right_source_constraint;
    }

    display_frame_.left =
        (display_frame_.left - left_source_constraint) + left_constraint;
    display_frame_.right =
        (display_frame_.right - left_source_constraint) + left_constraint;

    display_frame_.bottom =
        std::min(max_height, static_cast<uint32_t>(display_frame_.bottom));
    display_frame_width_ = display_frame_.right - display_frame_.left;
    display_frame_height_ = display_frame_.bottom - display_frame_.top;

    UpdateSurfaceDamage(layer);
    if (gpu_rendered_) {
      // If viewport and layer doesn't interact we can avoid re-rendering
      // this layer.
      if (AnalyseOverlap(surface_damage_, display_frame_) != kOutside) {
        surface_damage_.left =
            std::max(surface_damage_.left, display_frame_.left);
        surface_damage_.right =
            std::min(surface_damage_.right, display_frame_.right);
        surface_damage_.top = std::max(surface_damage_.top, display_frame_.top);
        surface_damage_.bottom =
            std::min(surface_damage_.bottom, display_frame_.bottom);
      } else {
        surface_damage_ = HwcRect<int>(0, 0, 0, 0);
      }
    }

    // split the source in proportion of frame rect offset for sub displays as:
    // 1. the original source size may be different with the original frame
    // rect,
    //    we need get proportional content of source.
    // 2. the UI content may cross the sub displays of Mosaic or Logical mode

    source_crop_.left = static_cast<float>(source_width) *
                        (static_cast<float>(frame_offset_left) /
                         static_cast<float>(frame_width));
    source_crop_.right = static_cast<float>(source_width) *
                         (static_cast<float>(frame_offset_right) /
                          static_cast<float>(frame_width));
    source_crop_width_ = static_cast<int>(ceilf(source_crop_.right) -
                                          static_cast<int>(source_crop_.left));
    source_crop_height_ = static_cast<int>(ceilf(source_crop_.bottom) -
                                           static_cast<int>(source_crop_.top));
  }
}

void OverlayLayer::InitializeFromHwcLayer(
    HwcLayer* layer, ResourceManager* resource_manager,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    uint32_t max_height, HWCRotation rotation, bool handle_constraints) {
  display_frame_width_ = layer->GetDisplayFrameWidth();
  display_frame_height_ = layer->GetDisplayFrameHeight();
  display_frame_ = layer->GetDisplayFrame();
  InitializeState(layer, resource_manager, previous_layer, z_order, layer_index,
                  max_height, rotation, handle_constraints);
}

void OverlayLayer::InitializeFromScaledHwcLayer(
    HwcLayer* layer, ResourceManager* resource_manager,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    const HwcRect<int>& display_frame, uint32_t max_height,
    HWCRotation rotation, bool handle_constraints) {
  SetDisplayFrame(display_frame);
  InitializeState(layer, resource_manager, previous_layer, z_order, layer_index,
                  max_height, rotation, handle_constraints);
}

void OverlayLayer::ValidatePreviousFrameState(OverlayLayer* rhs,
                                              HwcLayer* layer) {
  OverlayBuffer* buffer = imported_buffer_->buffer_.get();
  if (buffer->GetFormat() != rhs->imported_buffer_->buffer_->GetFormat())
    return;

  bool content_changed = false;
  bool rect_changed = layer->HasDisplayRectChanged();
  // We expect cursor plane to support alpha always.
  if (rhs->gpu_rendered_ || (type_ == kLayerCursor)) {
    content_changed = rect_changed || layer->HasSourceRectChanged();
  } else {
    // If previous layer was opaque and we have alpha now,
    // let's mark this layer for re-validation. Plane
    // supporting XRGB format might not necessarily support
    // transparent planes. We assume plane supporting
    // ARGB will support XRGB.
    if ((rhs->alpha_ == 0xff) && (alpha_ != rhs->alpha_))
      return;

    if (blending_ != rhs->blending_)
      return;

    if (rect_changed || layer->HasLayerAttributesChanged()) {
      if (layer->IsValidated()) {
        return;
      }

      if (rhs->transform_ != transform_) {
        return;
      }

      if ((rhs->display_frame_.left != display_frame_.left) ||
          (rhs->display_frame_.right != display_frame_.right) ||
          (rhs->display_frame_.top != display_frame_.top) ||
          (rhs->display_frame_.bottom != display_frame_.bottom)) {
        return;
      }
    }

    if (layer->HasSourceRectChanged()) {
      // If the overall width and height hasn't changed, it
      // shouldn't impact the plane composition results.
      if ((source_crop_width_ != rhs->source_crop_width_) ||
          (source_crop_height_ != rhs->source_crop_height_)) {
        return;
      }
    }
  }

  state_ &= ~kLayerAttributesChanged;
  gpu_rendered_ = rhs->gpu_rendered_;

  if (!rect_changed) {
    state_ &= ~kDimensionsChanged;
  }

  if (!layer->HasVisibleRegionChanged() &&
      !layer->HasSurfaceDamageRegionChanged() &&
      !layer->HasLayerContentChanged() && !content_changed) {
    state_ &= ~kLayerContentChanged;
  }
}

void OverlayLayer::ValidateForOverlayUsage() {
  const std::shared_ptr<OverlayBuffer>& buffer = imported_buffer_->buffer_;
  if (buffer->GetUsage() & kLayerCursor) {
    type_ = kLayerCursor;
  } else if (buffer->IsVideoBuffer()) {
    type_ = kLayerVideo;
  }
}

void OverlayLayer::Dump() {
  DUMPTRACE("OverlayLayer Information Starts. -------------");
  switch (blending_) {
    case HWCBlending::kBlendingNone:
      DUMPTRACE("Blending: kBlendingNone.");
      break;
    case HWCBlending::kBlendingPremult:
      DUMPTRACE("Blending: kBlendingPremult.");
      break;
    case HWCBlending::kBlendingCoverage:
      DUMPTRACE("Blending: kBlendingCoverage.");
      break;
    default:
      break;
  }

  if (transform_ & kReflectX)
    DUMPTRACE("Transform: kReflectX.");
  if (transform_ & kReflectY)
    DUMPTRACE("Transform: kReflectY.");
  if (transform_ & kReflectY)
    DUMPTRACE("Transform: kReflectY.");
  else if (transform_ & kTransform180)
    DUMPTRACE("Transform: kTransform180.");
  else if (transform_ & kTransform270)
    DUMPTRACE("Transform: kTransform270.");
  else
    DUMPTRACE("Transform: kTransform0.");

  DUMPTRACE("Alpha: %u", alpha_);

  DUMPTRACE("SourceWidth: %d", source_crop_width_);
  DUMPTRACE("SourceHeight: %d", source_crop_height_);
  DUMPTRACE("DstWidth: %d", display_frame_width_);
  DUMPTRACE("DstHeight: %d", display_frame_height_);
  DUMPTRACE("AquireFence: %d", imported_buffer_->acquire_fence_);

  imported_buffer_->buffer_->Dump();
}

}  // namespace hwcomposer
