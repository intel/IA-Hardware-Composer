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

namespace hwcomposer {

bool OverlayLayer::operator!=(const OverlayLayer& rhs) const {
  if (buffer_->GetFormat() != rhs.buffer_->GetFormat())
    return true;

  // We expect cursor plane to support alpha always.
  if (!(buffer_->GetUsage() & kLayerCursor)) {
    if (alpha_ != rhs.alpha_)
      return true;
  }

  if (blending_ != rhs.blending_)
    return true;

  // We check only for rotation and not transfor as this
  // valueis arrived from transform_
  if (rotation_ != rhs.rotation_)
    return true;

  if (display_frame_width_ != rhs.display_frame_width_)
    return true;

  if (display_frame_height_ != rhs.display_frame_height_)
    return true;

  if (source_crop_width_ != rhs.source_crop_width_)
    return true;

  if (source_crop_height_ != rhs.source_crop_height_)
    return true;

  return false;
}

int OverlayLayer::GetReleaseFence() {
  if (!sync_object_) {
    sync_object_.reset(new NativeSync());
    if (!sync_object_->Init()) {
      ETRACE("Failed to create sync object.");
      return -1;
    }
  }

  return sync_object_->CreateNextTimelineFence();
}

void OverlayLayer::SetReleaseFenceState(NativeSync::State state) {
  sync_object_->SetState(state);
}

void OverlayLayer::ReleaseFenceIfReady() {
  if (!sync_object_)
    return;

  if (sync_object_->GetState() == NativeSync::State::kReady) {
    sync_object_.reset(nullptr);
  }
}

void OverlayLayer::ReleaseSyncOwnershipAsNeeded(
    std::unique_ptr<NativeSync>& fence) {
  if (sync_object_ &&
      sync_object_->GetState() == NativeSync::State::kSignalOnPageFlipEvent) {
    fence.reset(sync_object_.release());
  }
}

void OverlayLayer::SetIndex(uint32_t index) {
  index_ = index;
}

void OverlayLayer::SetNativeHandle(HWCNativeHandle handle) {
  sf_handle_ = handle;
}

void OverlayLayer::SetTransform(int32_t transform) {
  transform_ = transform;
  rotation_ = 0;
  if (transform & kReflectX)
    rotation_ |= 1 << DRM_REFLECT_X;
  if (transform & kReflectY)
    rotation_ |= 1 << DRM_REFLECT_Y;
  if (transform & kRotate90)
    rotation_ |= 1 << DRM_ROTATE_90;
  else if (transform & kRotate180)
    rotation_ |= 1 << DRM_ROTATE_180;
  else if (transform & kRotate270)
    rotation_ |= 1 << DRM_ROTATE_270;
  else
    rotation_ |= 1 << DRM_ROTATE_0;
}

void OverlayLayer::SetAlpha(uint8_t alpha) {
  alpha_ = alpha;
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
  DUMPTRACE("AquireFence: %d", acquire_fence_.get());

  buffer_->Dump();
}

}  // namespace hwcomposer
