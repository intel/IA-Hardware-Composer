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
#ifndef OVERLAY_BUFFER_H_
#define OVERLAY_BUFFER_H_

#include <platformdefines.h>

#include <hwcbuffer.h>

#include "compositordefs.h"

namespace hwcomposer {

class NativeBufferHandler;

class OverlayBuffer {
 public:
  OverlayBuffer() = default;
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

  bool IsCompatible(const HwcBuffer& bo) const;

  void IncrementRefCount() {
    ref_count_++;
  }

  void DecreaseRefCount() {
    if (ref_count_ > 0)
      ref_count_--;
  }

  int RefCount() const {
    return ref_count_;
  }

  void SetInUse(bool in_use) {
    in_use_ = in_use;
  }

  bool InUse() const {
    return in_use_;
  }

  void SetRecommendedFormat(uint32_t format);

  void Dump();

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
  uint32_t ref_count_ = 1;
  uint32_t gpu_fd_;
  bool reset_framebuffer_ = true;
  bool in_use_ = false;
};

}  // namespace hwcomposer
#endif  // OVERLAY_BUFFER_H_
