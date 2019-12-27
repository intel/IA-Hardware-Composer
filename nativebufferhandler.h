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

#ifndef PUBLIC_DMRNATIVEBUFFERHANDLER_H_
#define PUBLIC_DMRNATIVEBUFFERHANDLER_H_

#include <cutils/native_handle.h>
#include <stdint.h>

//#include <hwcdefs.h>
//#include <platformdefines.h>
#include "vautils.h"

namespace android {

class NativeBufferHandler {
 public:
  static NativeBufferHandler *CreateInstance(uint32_t fd);

  virtual ~NativeBufferHandler() {
  }

  virtual bool CreateBuffer(uint32_t w, uint32_t h, int format,
                            DRMHwcNativeHandle *handle = NULL,
                            uint32_t layer_type = 0,
                            bool *modifier_used = NULL, int64_t modifier = -1,
                            bool raw_pixel_buffer = false) const = 0;

  virtual bool ReleaseBuffer(DRMHwcNativeHandle handle) const = 0;

  virtual void DestroyHandle(DRMHwcNativeHandle handle) const = 0;

  virtual bool ImportBuffer(DRMHwcNativeHandle handle) const = 0;

  virtual void CopyHandle(DRMHwcNativeHandle source,
                          DRMHwcNativeHandle target) const = 0;

  virtual uint32_t GetTotalPlanes(DRMHwcNativeHandle handle) const = 0;

  virtual void *Map(DRMHwcNativeHandle handle, uint32_t x, uint32_t y,
                    uint32_t width, uint32_t height, uint32_t *stride,
                    void **map_data, size_t plane) const = 0;

  virtual int32_t UnMap(DRMHwcNativeHandle handle, void *map_data) const = 0;

  virtual uint32_t GetFd() const = 0;
  virtual bool GetInterlace(DRMHwcNativeHandle handle) const = 0;
};

}  // namespace hwcomposer
#endif  // PUBLIC_NATIVEBUFFERHANDLER_H_
