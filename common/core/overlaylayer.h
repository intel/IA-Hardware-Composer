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

#ifndef OVERLAY_LAYER_H_
#define OVERLAY_LAYER_H_

#include <hwcdefs.h>
#include <platformdefines.h>

#include <nativefence.h>

#include "overlaybuffer.h"

namespace hwcomposer {

class NativeBufferHandler;
class OverlayBuffer;

struct OverlayLayer {
  bool operator!=(const OverlayLayer& rhs) const;
  void SetReleaseFence(int fd) {
    release_fence_.Reset(fd);
  }

  int GetReleaseFence() const {
    return release_fence_.get();
  }

  void SetAcquireFence(int fd) {
    acquire_fence_.Reset(fd);
  }

  int GetAcquireFence() const {
    return acquire_fence_.get();
  }

  void SetIndex(uint32_t index);

  uint32_t GetIndex() const {
    return index_;
  }

  void SetNativeHandle(HWCNativeHandle handle);

  HWCNativeHandle GetNativeHandle() const {
    return sf_handle_;
  }

  void SetTransform(int32_t sf_transform);

  uint32_t GetTransform() const {
    return transform_;
  }

  void SetAlpha(uint8_t alpha);

  uint8_t GetAlpha() const {
    return alpha_;
  }

  void SetBlending(HWCBlending blending);

  HWCBlending GetBlending() const {
    return blending_;
  }

  uint32_t GetRotation() const {
    return rotation_;
  }

  OverlayBuffer* GetBuffer() const {
    return buffer_.get();
  }

  void SetBuffer(OverlayBuffer* buffer) {
    buffer_.reset(buffer);
  }

  void SetSourceCrop(const HwcRect<float>& source_crop);
  const HwcRect<float>& GetSourceCrop() const {
    return source_crop_;
  }

  void SetDisplayFrame(const HwcRect<int>& display_frame);
  const HwcRect<int>& GetDisplayFrame() const {
    return display_frame_;
  }

  uint32_t GetSourceCropWidth() const {
    return source_crop_width_;
  }
  uint32_t GetSourceCropHeight() const {
    return source_crop_height_;
  }
  uint32_t GetDisplayFrameWidth() const {
    return display_frame_width_;
  }
  uint32_t GetDisplayFrameHeight() const {
    return display_frame_height_;
  }

  void Dump();

 private:
  uint32_t transform_;
  uint32_t rotation_;
  uint32_t index_;
  uint32_t source_crop_width_;
  uint32_t source_crop_height_;
  uint32_t display_frame_width_;
  uint32_t display_frame_height_;
  uint8_t alpha_ = 0xff;
  HwcRect<float> source_crop_;
  HwcRect<int> display_frame_;
  NativeFence release_fence_;
  ScopedFd acquire_fence_;
  HWCBlending blending_ = HWCBlending::kBlendingNone;
  HWCNativeHandle sf_handle_ = 0;
  std::unique_ptr<OverlayBuffer> buffer_;
};

}  // namespace hardware
#endif  // OVERLAY_LAYER_H_
