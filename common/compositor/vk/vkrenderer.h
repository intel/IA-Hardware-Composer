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

#ifndef VK_RENDERER_H_
#define VK_RENDERER_H_

#include <memory>

#include "renderer.h"
#include "vkprogram.h"
#include "vkshim.h"

namespace hwcomposer {

class VKRenderer : public Renderer {
 public:
  VKRenderer() = default;
  ~VKRenderer();

  bool Init() override;
  bool Draw(const std::vector<RenderState> &commands, NativeSurface *surface,
            bool clear_surface) override;
  void InsertFence(int32_t kms_fence) override;
  void RestoreState() override;
  bool MakeCurrent() override;
  void SetExplicitSyncSupport(bool disable_explicit_sync) override;

 private:
  VKProgram *GetProgram(unsigned texture_count);
  uint32_t GetMemoryTypeIndex(uint32_t mem_type_bits, uint32_t required_props);
  VkBuffer UploadBuffer(size_t data_size, const uint8_t *data,
                        VkBufferUsageFlags usage);

  VkPhysicalDeviceProperties device_props_;
  VkPhysicalDeviceMemoryProperties device_mem_props_;
  VkDeviceMemory uniform_buffer_mem_;
  VkDescriptorPool desc_pool_;
  VkCommandPool cmd_pool_;
  VkQueue queue_;
  VkBuffer vert_buffer_;

  std::vector<std::unique_ptr<VKProgram>> programs_;
};

}  // namespace hwcomposer

#endif  // VK_RENDERER_H_
