// Copyright (c) 2017 Intel Corporation
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

#ifndef VK_SURFACE_H_
#define VK_SURFACE_H_

#include "nativesurface.h"
#include "vkcontext.h"
#include "vkshim.h"

namespace hwcomposer {

class OverlayBuffer;

class VKSurface : public NativeSurface {
 public:
  VKSurface() = default;
  ~VKSurface() override;
  VKSurface(uint32_t width, uint32_t height);

  bool MakeCurrent() override;

 private:
  bool InitializeGPUResources();
  struct vk_resource surface_resource_;
  VkFramebuffer surface_fb_;
  VKContext *context_;
};

}  // namespace hwcomposer

#endif  // VK_SURFACE_H_
