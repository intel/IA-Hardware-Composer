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

#ifndef PUBLIC_NATIVEBUFFERHANDLER_H_
#define PUBLIC_NATIVEBUFFERHANDLER_H_

#include <stdint.h>

#include <platformdefines.h>
#include <hwcdefs.h>

namespace hwcomposer {

class NativeBufferHandler {
 public:
  static NativeBufferHandler *CreateInstance(uint32_t fd);

  virtual ~NativeBufferHandler() {
  }

  virtual bool CreateBuffer(uint32_t w, uint32_t h, int format,
                            HWCNativeHandle *handle = NULL,
                            uint32_t layer_type = kLayerNormal) const = 0;

  virtual bool CanReleaseGemHandles(HWCNativeHandle handle) const = 0;

  virtual bool ReleaseBuffer(HWCNativeHandle handle) const = 0;

  virtual void DestroyHandle(HWCNativeHandle handle) const = 0;

  virtual bool ImportBuffer(HWCNativeHandle handle) const = 0;

  virtual void CopyHandle(HWCNativeHandle source,
                          HWCNativeHandle *target) const = 0;

  virtual uint32_t GetTotalPlanes(HWCNativeHandle handle) const = 0;

  virtual void *Map(HWCNativeHandle handle, uint32_t x, uint32_t y,
                    uint32_t width, uint32_t height, uint32_t *stride,
                    void **map_data, size_t plane) const = 0;

  virtual int32_t UnMap(HWCNativeHandle handle, void *map_data) const = 0;

  virtual uint32_t GetFd() const = 0;
};

}  // namespace hwcomposer
#endif  // PUBLIC_NATIVEBUFFERHANDLER_H_
