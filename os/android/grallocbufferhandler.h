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

#ifndef OS_ANDROID_GRALLOCBUFFERHANDLER_H_
#define OS_ANDROID_GRALLOCBUFFERHANDLER_H_

#include <nativebufferhandler.h>

#include <hardware/gralloc.h>

namespace hwcomposer {

class GpuDevice;

class GrallocBufferHandler : public NativeBufferHandler {
 public:
  explicit GrallocBufferHandler(uint32_t fd);
  ~GrallocBufferHandler() override;

  bool Init();

  bool CreateBuffer(uint32_t w, uint32_t h, int format,
                    HWCNativeHandle *handle) override;
  bool ReleaseBuffer(HWCNativeHandle handle) override;
  void DestroyHandle(HWCNativeHandle handle) override;
  bool ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) override;
  void CopyHandle(HWCNativeHandle source, HWCNativeHandle *target) override;
  uint32_t GetTotalPlanes(HWCNativeHandle handle) override;
  void *Map(HWCNativeHandle handle, uint32_t x, uint32_t y, uint32_t width,
            uint32_t height, uint32_t *stride, void **map_data,
            size_t plane) override;
  void UnMap(HWCNativeHandle handle, void *map_data) override;

 private:
  uint32_t ConvertHalFormatToDrm(uint32_t hal_format);
  uint32_t fd_;
  const gralloc_module_t *gralloc_;
};

}  // namespace hwcomposer
#endif  // OS_ANDROID_GRALLOCBUFFERHANDLER_H_
