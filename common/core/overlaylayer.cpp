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

#include <hwctrace.h>

#include "overlaybuffer.h"

namespace hwcomposer {

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
  source_crop_width_ = (int)source_crop.right - (int)source_crop.left;
  source_crop_height_ = (int)source_crop.bottom - (int)source_crop.top;
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
  DUMPTRACE("ReleaseFence: %d", release_fence_.get());

  buffer_->Dump();
}

}  // namespace hwcomposer
