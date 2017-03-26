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
#ifndef COMMON_CORE_OVERLAYBUFFER_H_
#define COMMON_CORE_OVERLAYBUFFER_H_

#include <platformdefines.h>

#include <hwcbuffer.h>

#include "compositordefs.h"

namespace hwcomposer {

class NativeBufferHandler;

class OverlayBuffer {
 public:
  OverlayBuffer(OverlayBuffer&& rhs) = default;
  OverlayBuffer& operator=(OverlayBuffer&& other) = default;

  ~OverlayBuffer();

  void Initialize(const HwcBuffer& bo);

  void InitializeFromNativeHandle(HWCNativeHandle handle,
                                  NativeBufferHandler* buffer_handler);

  uint32_t GetWidth() const {
    return width_;
  }

  uint32_t GetHeight() const {
    return height_;
  }

  uint32_t GetFormat() const {
    return format_;
  }

  uint32_t GetStride() const {
    return pitches_[0];
  }

  uint32_t GetUsage() const {
    return usage_;
  }

  uint32_t GetFb() const {
    return fb_id_;
  }

  GpuImage ImportImage(GpuDisplay egl_display);

  bool CreateFrameBuffer(uint32_t gpu_fd);

  void ReleaseFrameBuffer();

  void SetRecommendedFormat(uint32_t format);

  void Dump();

 protected:
  OverlayBuffer() = default;
  friend class OverlayBufferManager;

 private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t format_ = 0;
  uint32_t pitches_[4];
  uint32_t offsets_[4];
  uint32_t gem_handles_[4];
  uint32_t fb_id_ = 0;
  uint32_t prime_fd_ = 0;
  uint32_t usage_ = 0;
  uint32_t gpu_fd_ = 0;
  bool is_yuv_ = false;
  HWCNativeHandle handle_ = 0;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_OVERLAYBUFFER_H_
