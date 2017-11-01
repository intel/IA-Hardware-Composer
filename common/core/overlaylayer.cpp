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

#include <nativebufferhandler.h>

namespace hwcomposer {

OverlayLayer::ImportedBuffer::~ImportedBuffer() {
  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }
}

OverlayLayer::ImportedBuffer::ImportedBuffer(OverlayBuffer* buffer,
                                             int32_t acquire_fence)
    : acquire_fence_(acquire_fence) {
  buffer_.reset(buffer);
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
  return imported_buffer_->buffer_.get();
}

void OverlayLayer::SetBuffer(NativeBufferHandler* buffer_handler,
                             HWCNativeHandle handle, int32_t acquire_fence) {
  OverlayBuffer* buffer = OverlayBuffer::CreateOverlayBuffer();
  buffer->InitializeFromNativeHandle(handle, buffer_handler);
  imported_buffer_.reset(new ImportedBuffer(buffer, acquire_fence));
}

void OverlayLayer::ResetBuffer() {
  imported_buffer_.reset(nullptr);
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
  int width = display_frame_width_ + display_frame_.left;
  int t_width = surface_damage_.left + display_frame_width_;
  int top = display_frame_height_ + display_frame_.top;
  int t_top = surface_damage_.top + display_frame_height_;
  // If surface Damage is not part of display frame, reset it to empty.
  if ((surface_damage_.left >= (width)) || (t_width <= display_frame_.left) ||
      (surface_damage_.top >= top) || (t_top <= display_frame_.top)) {
    surface_damage_ = HwcRect<int>(0, 0, 0, 0);
    return;
  }

  surface_damage_.left = std::max(surface_damage_.left, display_frame_.left);
  surface_damage_.right = std::min(surface_damage_.right, display_frame_.right);
  surface_damage_.top = std::min(surface_damage_.top, display_frame_.top);
  surface_damage_.bottom =
      std::min(surface_damage_.bottom, display_frame_.bottom);
}

void OverlayLayer::InitializeState(HwcLayer* layer,
                                   NativeBufferHandler* buffer_handler,
                                   OverlayLayer* previous_layer,
                                   uint32_t z_order, uint32_t layer_index,
                                   uint32_t max_width, uint32_t max_height,
                                   bool handle_constraints) {
  transform_ = layer->GetTransform();
  alpha_ = layer->GetAlpha();
  layer_index_ = layer_index;
  z_order_ = z_order;
  source_crop_width_ = layer->GetSourceCropWidth();
  source_crop_height_ = layer->GetSourceCropHeight();
  display_frame_width_ = display_frame_.right - display_frame_.left;
  display_frame_height_ = display_frame_.bottom - display_frame_.top;
  source_crop_ = layer->GetSourceCrop();
  blending_ = layer->GetBlending();
  SetBuffer(buffer_handler, layer->GetNativeHandle(), layer->GetAcquireFence());
  ValidateForOverlayUsage(max_width, max_height, handle_constraints);
  if (previous_layer) {
    ValidatePreviousFrameState(previous_layer, layer);
  }

  if (layer->HasContentAttributesChanged() ||
      layer->HasVisibleRegionChanged() || layer->HasLayerAttributesChanged() ||
      layer->HasSourcePositionChanged()) {
    state_ |= kClearSurface;
  }

  if (handle_constraints) {
    int32_t left_constraint = layer->GetLeftConstraint();
    int32_t right_constraint = layer->GetRightConstraint();
    if (left_constraint >= 0 && right_constraint >= 0) {
      if (display_frame_.right > right_constraint) {
        display_frame_.right = right_constraint;
      }

      if (display_frame_.left < left_constraint) {
        display_frame_.left = left_constraint;
      }

      if (display_frame_.right < right_constraint) {
        display_frame_.right =
            std::max(display_frame_.left, display_frame_.right);
      }

      if (display_frame_.left > left_constraint) {
        display_frame_.left =
            std::min(display_frame_.left, display_frame_.right);
      }

      if (left_constraint > 0) {
        display_frame_.left -= left_constraint;
        display_frame_.right = right_constraint - display_frame_.right;
      }

      display_frame_width_ = display_frame_.right - display_frame_.left;
      display_frame_height_ = display_frame_.bottom - display_frame_.top;
    }

    float lconstraint = (float)layer->GetLeftSourceConstraint();
    float rconstraint = (float)layer->GetRightSourceConstraint();
    if (lconstraint >= 0 && rconstraint >= 0) {
      if (source_crop_.right > rconstraint) {
        source_crop_.right = rconstraint;
      }

      if (source_crop_.left < lconstraint) {
        source_crop_.left = lconstraint;
      }

      if (source_crop_.right < rconstraint) {
        source_crop_.right = std::max(source_crop_.left, source_crop_.right);
      }

      if (source_crop_.left > lconstraint) {
        source_crop_.left = std::min(source_crop_.left, source_crop_.right);
      }

      source_crop_width_ = static_cast<int>(
          ceilf(source_crop_.right) - static_cast<int>(source_crop_.left));
      source_crop_height_ = static_cast<int>(
          ceilf(source_crop_.bottom) - static_cast<int>(source_crop_.top));
    }
  }

  // TODO: FIXME: We should be able to use surfacedamage
  // from HWCLayer here.
  surface_damage_ = display_frame_;
}

void OverlayLayer::InitializeFromHwcLayer(
    HwcLayer* layer, NativeBufferHandler* buffer_handler,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    uint32_t max_width, uint32_t max_height, bool handle_constraints) {
  display_frame_ = layer->GetDisplayFrame();
  InitializeState(layer, buffer_handler, previous_layer, z_order, layer_index,
                  max_width, max_height, handle_constraints);
}

#ifdef ENABLE_IMPLICIT_CLONE_MODE
void OverlayLayer::InitializeFromScaledHwcLayer(
    HwcLayer* layer, NativeBufferHandler* buffer_handler,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    const HwcRect<int>& display_frame, uint32_t max_width, uint32_t max_height,
    bool handle_constraints) {
  SetDisplayFrame(display_frame);
  InitializeState(layer, buffer_handler, previous_layer, z_order, layer_index,
                  max_width, max_height, handle_constraints);
}
#endif

void OverlayLayer::ValidatePreviousFrameState(OverlayLayer* rhs,
                                              HwcLayer* layer) {
  OverlayBuffer* buffer = imported_buffer_->buffer_.get();
  if (buffer->GetFormat() != rhs->imported_buffer_->buffer_->GetFormat())
    return;

  bool content_changed = false;
  bool rect_changed = layer->HasDisplayRectChanged();
  // We expect cursor plane to support alpha always.
  if (rhs->gpu_rendered_ || (type_ == kLayerCursor)) {
    content_changed = rect_changed || layer->HasContentAttributesChanged() ||
                      layer->HasLayerAttributesChanged() ||
                      layer->HasSourcePositionChanged();
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

    if (rect_changed || layer->HasLayerAttributesChanged())
      return;
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

void OverlayLayer::ValidateForOverlayUsage(int32_t max_width,
                                           int32_t max_height,
                                           bool handle_constraints) {
  const std::unique_ptr<OverlayBuffer>& buffer = imported_buffer_->buffer_;
  if (buffer->GetUsage() & kLayerCursor) {
    type_ = kLayerCursor;
  } else if (buffer->IsVideoBuffer()) {
    type_ = kLayerVideo;
  }

  if (handle_constraints)
    return;

  if (type_ == kLayerCursor) {
    display_frame_width_ = buffer->GetWidth();
    display_frame_height_ = buffer->GetHeight();
    display_frame_.right = display_frame_.left + display_frame_width_;
    display_frame_.bottom = display_frame_.top + display_frame_height_;
    if (display_frame_.bottom > max_height) {
      uint32_t delta = display_frame_.bottom - max_height;
      display_frame_height_ -= delta;
      display_frame_.bottom = max_height;
    }

    if (display_frame_.right > max_width) {
      uint32_t delta = display_frame_.right - max_width;
      display_frame_height_ -= delta;
      display_frame_.right = max_width;
    }

    source_crop_width_ = display_frame_width_;
    source_crop_height_ = display_frame_height_;
    source_crop_.left = 0.0;
    source_crop_.top = 0.0;
    source_crop_.right = static_cast<float>(display_frame_width_);
    source_crop_.bottom = static_cast<float>(display_frame_height_);
  } else {
    if (display_frame_.bottom > max_height) {
      uint32_t delta = display_frame_.bottom - max_height;
      display_frame_height_ -= delta;
      display_frame_.bottom = max_height;

      if (source_crop_.bottom > max_height) {
        uint32_t delta = source_crop_.bottom - max_height;
        source_crop_height_ -= delta;
        source_crop_.bottom = max_height;
      }
    }

    if (display_frame_.right > max_width) {
      uint32_t delta = display_frame_.right - max_width;
      display_frame_width_ -= delta;
      display_frame_.right = max_width;

      if (source_crop_.right > max_width) {
        uint32_t delta = source_crop_.right - max_width;
        source_crop_width_ -= delta;
        source_crop_.right = max_width;
      }
    }
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
  else if (transform_ & kRotate180)
    DUMPTRACE("Transform: kRotate180.");
  else if (transform_ & kRotate270)
    DUMPTRACE("Transform: kRotate270.");
  else
    DUMPTRACE("Transform: kRotate0.");

  DUMPTRACE("Alpha: %u", alpha_);

  DUMPTRACE("SourceWidth: %d", source_crop_width_);
  DUMPTRACE("SourceHeight: %d", source_crop_height_);
  DUMPTRACE("DstWidth: %d", display_frame_width_);
  DUMPTRACE("DstHeight: %d", display_frame_height_);
  DUMPTRACE("AquireFence: %d", imported_buffer_->acquire_fence_);

  imported_buffer_->buffer_->Dump();
}

}  // namespace hwcomposer
