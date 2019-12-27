/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OS_ANDROID_DRM_Gralloc1BufferHandler_H_
#define OS_ANDROID_DRM_Gralloc1BufferHandler_H_
#include <cutils/native_handle.h>
#include <nativebufferhandler.h>

#include <hardware/gralloc1.h>

#include <i915_private_android_types.h>
//#include "gralloc_handle.h"
#include "vautils.h"

namespace android {


class Gralloc1BufferHandler : public NativeBufferHandler {
 public:
  explicit Gralloc1BufferHandler(uint32_t fd);
  ~Gralloc1BufferHandler() override;

  bool Init();

  bool CreateBuffer(uint32_t w, uint32_t h, int format, DRMHwcNativeHandle *handle,
                    uint32_t layer_type = 0,
                    bool *modifier_used = NULL, int64_t modifier = -1,
                    bool raw_pixel_buffer = false) const override;
  bool ReleaseBuffer(DRMHwcNativeHandle handle) const override;
  void DestroyHandle(DRMHwcNativeHandle handle) const override;
  bool ImportBuffer(DRMHwcNativeHandle handle) const override;
  void CopyHandle(DRMHwcNativeHandle source,
                  DRMHwcNativeHandle target) const override;
  uint32_t GetTotalPlanes(DRMHwcNativeHandle handle) const override;
  void *Map(DRMHwcNativeHandle handle, uint32_t x, uint32_t y, uint32_t width,
            uint32_t height, uint32_t *stride, void **map_data,
            size_t plane) const override;
  int32_t UnMap(DRMHwcNativeHandle handle, void *map_data) const override;

  uint32_t GetFd() const override {
    return fd_;
  }

  bool GetInterlace(DRMHwcNativeHandle handle) const override;

 private:
  uint32_t ConvertHalFormatToDrm(uint32_t hal_format);
  uint32_t fd_;
  const hw_module_t *gralloc_;
  hw_device_t *device_;
  GRALLOC1_PFN_RETAIN register_;
  GRALLOC1_PFN_RELEASE release_;
  GRALLOC1_PFN_GET_DIMENSIONS dimensions_;
  GRALLOC1_PFN_LOCK lock_;
  GRALLOC1_PFN_UNLOCK unlock_;
  GRALLOC1_PFN_CREATE_DESCRIPTOR create_descriptor_;
  GRALLOC1_PFN_DESTROY_DESCRIPTOR destroy_descriptor_;
  GRALLOC1_PFN_SET_CONSUMER_USAGE set_consumer_usage_;
  GRALLOC1_PFN_SET_DIMENSIONS set_dimensions_;
  GRALLOC1_PFN_SET_FORMAT set_format_;
  GRALLOC1_PFN_SET_PRODUCER_USAGE set_producer_usage_;
  GRALLOC1_PFN_ALLOCATE allocate_;
#ifdef USE_GRALLOC1
  GRALLOC1_PFN_SET_MODIFIER set_modifier_;
#endif
};

}  // namespace hwcomposer
#endif  // OS_ANDROID_Gralloc1BufferHandler_H_
