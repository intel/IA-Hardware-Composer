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
#ifndef WSI_DRMBUFFER_H_
#define WSI_DRMBUFFER_H_

#include <platformdefines.h>

#include <hwcbuffer.h>

#include "overlaybuffer.h"

namespace hwcomposer {

class NativeBufferHandler;

class DrmBuffer : public OverlayBuffer {
 public:
  DrmBuffer(DrmBuffer&& rhs) = default;
  DrmBuffer& operator=(DrmBuffer&& other) = default;

  DrmBuffer() = default;

  ~DrmBuffer() override;

  void Initialize(const HwcBuffer& bo);

  void InitializeFromNativeHandle(HWCNativeHandle handle,
                                  NativeBufferHandler* buffer_handler) override;

  uint32_t GetWidth() const override {
    return width_;
  }

  uint32_t GetHeight() const override {
    return height_;
  }

  uint32_t GetFormat() const override {
    return format_;
  }

  uint32_t GetUsage() const override {
    return usage_;
  }

  uint32_t GetFb() const override {
    return fb_id_;
  }

  struct vk_import ImportImage(VkDevice dev) override;
  EGLImageKHR ImportImage(EGLDisplay egl_display) override;

  bool CreateFrameBuffer(uint32_t gpu_fd) override;

  void ReleaseFrameBuffer() override;

  void SetRecommendedFormat(uint32_t format) override;

  bool IsVideoBuffer() const override {
    return is_yuv_;
  }

  void Dump() override;

 private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t format_ = 0;
  uint32_t frame_buffer_format_ = 0;
  uint32_t pitches_[4];
  uint32_t offsets_[4];
  uint32_t gem_handles_[4];
  uint32_t fb_id_ = 0;
  uint32_t prime_fd_ = 0;
  uint32_t usage_ = 0;
  uint32_t gpu_fd_ = 0;
  bool is_yuv_ = false;
  HWCNativeHandle handle_ = 0;
  NativeBufferHandler* buffer_handler_ = 0;
};

}  // namespace hwcomposer
#endif  // WSI_DRMBUFFER_H_
