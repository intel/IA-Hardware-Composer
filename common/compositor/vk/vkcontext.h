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

#ifndef VK_CONTEXT_H_
#define VK_CONTEXT_H_

#include <memory>
#include "vkshim.h"

namespace hwcomposer {

class VKContext {
 public:
  VKContext() = default;
  ~VKContext();

  bool Init();

  VkInstance getInstance() {
    return instance_;
  }

  VkPhysicalDevice getPhysicalDevice() {
    return physical_device_;
  }

  VkDevice getDevice() {
    return device_;
  }

  VkBuffer getUniformBuffer() {
    return uniform_buffer_;
  }

  VkSampler getSampler() {
    return sampler_;
  }

  VkPipelineCache getPipelineCache() {
    return pipeline_cache_;
  }

  VkRenderPass getRenderPass() {
    return render_pass_;
  }

  void setSurface(struct vk_resource *resource, VkFramebuffer fb) {
    surface_resource_ = resource;
    framebuffer_ = fb;
  }

  VkFramebuffer getFramebuffer() {
    return framebuffer_;
  }

  struct vk_resource *getSurfaceResource() {
    return surface_resource_;
  }

  RingBuffer *getRingBuffer() {
    return &ring_buffer_;
  }

 private:
  uint32_t GetMemoryTypeIndex(uint32_t mem_type_bits, uint32_t required_props);
  bool initialized_ = false;
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkBuffer uniform_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory uniform_buffer_mem_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  struct vk_resource *surface_resource_ = NULL;
  VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
  RingBuffer ring_buffer_;
};

extern VKContext *global_context_;

}  // namespace hwcomposer

#endif  // VK_CONTEXT_H_
