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
  source_crop_width_ =
      static_cast<int>(source_crop.right) - static_cast<int>(source_crop.left);
  source_crop_height_ =
      static_cast<int>(source_crop.bottom) - static_cast<int>(source_crop.top);
  source_crop_ = source_crop;
}

void OverlayLayer::SetDisplayFrame(const HwcRect<int>& display_frame) {
  display_frame_width_ = display_frame.right - display_frame.left;
  display_frame_height_ = display_frame.bottom - display_frame.top;
  display_frame_ = display_frame;
}

void OverlayLayer::InitializeFromHwcLayer(
    HwcLayer* layer, NativeBufferHandler* buffer_handler,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    const HwcRect<int>& display_frame, bool scaled) {
  transform_ = layer->GetTransform();
  rotation_ = layer->GetRotation();
  alpha_ = layer->GetAlpha();
  layer_index_ = layer_index;
  z_order_ = z_order;
  source_crop_width_ = layer->GetSourceCropWidth();
  source_crop_height_ = layer->GetSourceCropHeight();
  source_crop_ = layer->GetSourceCrop();
  if (scaled) {
    SetDisplayFrame(display_frame);
  } else {
    display_frame_width_ = layer->GetDisplayFrameWidth();
    display_frame_height_ = layer->GetDisplayFrameHeight();
    display_frame_ = display_frame;
  }
  blending_ = layer->GetBlending();
  SetBuffer(buffer_handler, layer->GetNativeHandle(), layer->GetAcquireFence());
  ValidateForOverlayUsage();
  if (previous_layer) {
    ValidatePreviousFrameState(previous_layer, layer);
  }

  if (transform_ == 0) {
    surface_damage_ = layer->GetSurfaceDamage();
  } else {
    // TODO: FIXME: We should be able to use surfacedamage
    // even when transform applied is not 0.
    surface_damage_ = display_frame_;
  }

  if (layer->HasContentAttributesChanged() ||
      layer->HasVisibleRegionChanged() || layer->HasLayerAttributesChanged() ||
      layer->HasSourcePositionChanged()) {
    state_ |= kClearSurface;
  }
}

void OverlayLayer::ValidatePreviousFrameState(OverlayLayer* rhs,
                                              HwcLayer* layer) {
  OverlayBuffer* buffer = imported_buffer_->buffer_.get();
  if (buffer->GetFormat() != rhs->imported_buffer_->buffer_->GetFormat())
    return;

  bool content_changed = false;
  bool rect_changed = layer->HasDisplayRectChanged();
  // We expect cursor plane to support alpha always.
  if (rhs->gpu_rendered_ || (cursor_layer_)) {
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

void OverlayLayer::ValidateForOverlayUsage() {
  const std::unique_ptr<OverlayBuffer>& buffer = imported_buffer_->buffer_;
  prefer_separate_plane_ = buffer->IsVideoBuffer();
  cursor_layer_ = buffer->GetUsage() & kLayerCursor;
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
