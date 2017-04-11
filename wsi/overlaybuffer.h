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
#ifndef WSI_OVERLAYBUFFER_H_
#define WSI_OVERLAYBUFFER_H_

#include <platformdefines.h>

#include <hwcbuffer.h>

#include "compositordefs.h"

namespace hwcomposer {

class NativeBufferHandler;

class OverlayBuffer {
 public:
  static OverlayBuffer* CreateOverlayBuffer();

  OverlayBuffer(OverlayBuffer&& rhs) = default;
  OverlayBuffer& operator=(OverlayBuffer&& other) = default;
  OverlayBuffer() = default;

  virtual ~OverlayBuffer() {
  }

  virtual void InitializeFromNativeHandle(
      HWCNativeHandle handle, NativeBufferHandler* buffer_handler) = 0;

  virtual uint32_t GetWidth() const = 0;

  virtual uint32_t GetHeight() const = 0;

  virtual uint32_t GetFormat() const = 0;

  virtual uint32_t GetUsage() const = 0;

  virtual uint32_t GetFb() const = 0;

  virtual struct vk_import ImportImage(VkDevice dev) = 0;
  virtual EGLImageKHR ImportImage(EGLDisplay egl_display) = 0;

  virtual bool CreateFrameBuffer(uint32_t gpu_fd) = 0;

  virtual void ReleaseFrameBuffer() = 0;

  virtual void SetRecommendedFormat(uint32_t format) = 0;

  virtual bool IsVideoBuffer() const = 0;

  virtual void Dump() = 0;
};

}  // namespace hwcomposer
#endif  // WSI_OVERLAYBUFFER_H_
