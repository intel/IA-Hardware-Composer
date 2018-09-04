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

#ifndef OS_ALIOS_YALLOCBUFFERHANDLER_H_
#define OS_ALIOS_YallocBufferHandler_H_

#include <cutils/framebuffer.h>
#include <yalloc.h>

#include <nativebufferhandler.h>

namespace hwcomposer {

class GpuDevice;

class YallocBufferHandler : public NativeBufferHandler {
 public:
  explicit YallocBufferHandler(uint32_t fd);
  ~YallocBufferHandler() override;

  bool Init();

  bool CreateBuffer(uint32_t w, uint32_t h, int format, HWCNativeHandle *handle,
                    uint32_t layer_type = kLayerNormal,
                    bool *modifier_used = NULL, int64_t modifier = -1,
                    bool raw_pixel_buffer = false) const override;
  bool ReleaseBuffer(HWCNativeHandle handle) const override;
  void DestroyHandle(HWCNativeHandle handle) const override;
  void CopyHandle(HWCNativeHandle source,
                  HWCNativeHandle *target) const override;
  bool ImportBuffer(HWCNativeHandle handle) const override;
  uint32_t GetTotalPlanes(HWCNativeHandle handle) const override;
  void *Map(HWCNativeHandle handle, uint32_t x, uint32_t y, uint32_t width,
            uint32_t height, uint32_t *stride, void **map_data,
            size_t plane) const override;
  int32_t UnMap(HWCNativeHandle handle, void *map_data) const override;

  uint32_t GetFd() const override;
  bool GetInterlace(HWCNativeHandle handle) const override {
    return false;
  }

 private:
  uint32_t fd_;
  struct yalloc_device_t *device_;
};

}  // namespace hwcomposer
#endif  // OS_ALIOS_YALLOCBUFFERHANDLER_H_
