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

#ifndef NATIVE_BUFFER_HANDLER_H_
#define NATIVE_BUFFER_HANDLER_H_

#include <stdint.h>

#include <hwcbuffer.h>
#include <platformdefines.h>

namespace hwcomposer {

class NativeBufferHandler {
 public:
  static NativeBufferHandler *CreateInstance(uint32_t fd);

  virtual ~NativeBufferHandler() {
  }

  virtual bool CreateBuffer(uint32_t w, uint32_t h, int format,
                            HWCNativeHandle *handle) = 0;

  virtual bool DestroyBuffer(HWCNativeHandle handle) = 0;

  virtual bool ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) = 0;
};

}  // namespace hardware
#endif  // NATIVE_BUFFER_HANDLER_H_
