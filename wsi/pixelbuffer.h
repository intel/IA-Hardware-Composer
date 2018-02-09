/*
// Copyright (c) 2018 Intel Corporation
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

#ifndef WSI_PIXELBUFFER_H_
#define WSI_PIXELBUFFER_H_

#include <platformdefines.h>
#include <memory>
#include "compositordefs.h"
#include "hwcdefs.h"
#include <nativebufferhandler.h>

namespace hwcomposer {

class ResourceManager;

// Represent's raw pixel data.
class PixelBuffer {
 public:
  static PixelBuffer* CreatePixelBuffer();
  PixelBuffer();
  virtual ~PixelBuffer();

  // Map's buffer represented by prime_fd to CPU memory.
  virtual void* Map(uint32_t prime_fd, size_t size) = 0;

  // UnMap previously mapped buffer represented by prime_fd.
  virtual void Unmap(uint32_t prime_fd, void* addr, size_t size) = 0;

  // Creats buffer taking into consideration width, height and format.
  // It will try to update buffer wth addr in case we are able to map
  // and unmap the buffer. If NeedsTextureUpload() is true after this
  // call than caller is responsible for uploading the data to respective
  // texture.
  void Initialize(const NativeBufferHandler* buffer_handler, uint32_t width,
                  uint32_t height, uint32_t format, void* addr,
                  ResourceHandle& handle);

  // Returns true if this buffer cannot be mapped.
  bool NeedsTextureUpload() const {
    return needs_texture_upload_;
  }

  // Updates resource with pixel data addr.
  void Refresh(void* addr, const ResourceHandle& resource);

 private:
  bool needs_texture_upload_ = true;
};

}  // namespace hwcomposer
#endif
