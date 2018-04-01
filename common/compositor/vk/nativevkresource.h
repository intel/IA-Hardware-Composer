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

#ifndef NATIVE_VK_RESOURCE_H_
#define NATIVE_VK_RESOURCE_H_

#include "nativegpuresource.h"
#include "vkshim.h"

namespace hwcomposer {

struct OverlayLayer;

class NativeVKResource : public NativeGpuResource {
 public:
  NativeVKResource() = default;
  ~NativeVKResource() override;

  bool PrepareResources(const std::vector<OverlayBuffer*>& buffers) override;
  GpuResourceHandle GetResourceHandle(uint32_t layer_index) const override;
  void ReleaseGPUResources(
      const std::vector<ResourceHandle>& handles) override {
  }

 private:
  void Reset();
  std::vector<struct vk_resource> layer_textures_;
  std::vector<VkDeviceMemory> src_image_memory_;
};

}  // namespace hwcomposer

#endif  // NATIVE_VK_RESOURCE_H_
