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

#ifndef WSI_DRM_PIXELBUFFER_H_
#define WSI_DRM_PIXELBUFFER_H_

#include "pixelbuffer.h"

namespace hwcomposer {

class ResourceManager;

class DrmPixelBuffer : public PixelBuffer {
 public:
  DrmPixelBuffer();
  ~DrmPixelBuffer() override;

  void* Map(uint32_t prime_fd, size_t size) override;

  void Unmap(uint32_t prime_fd, void* addr, size_t size) override;
};

}  // namespace hwcomposer
#endif
