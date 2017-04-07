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

#ifndef VK_PROGRAM_H_
#define VK_PROGRAM_H_

#include <algorithm>
#include <vector>
#include "vkcontext.h"
#include "vkshim.h"

namespace hwcomposer {

struct RenderState;

struct pipeline_info {
  uint32_t layer_index;
  VkSpecializationInfo special;
  VkPipelineShaderStageCreateInfo stages[2];
};

class VKProgram {
 public:
  VKProgram();
  VKProgram(const VKProgram& rhs) = delete;
  VKProgram& operator=(const VKProgram& rhs) = delete;

  ~VKProgram();

  bool Init(unsigned layer_index);
  void UseProgram(const RenderState& cmd, unsigned int viewport_width,
                  unsigned int viewport_height);

  VkDescriptorSetLayout getDescLayout() {
    return descriptor_set_layout_;
  }
  VkPipelineLayout getPipeLayout() {
    return pipeline_layout_;
  }
  VkPipeline getPipeline() {
    return pipeline_;
  }
  VkDescriptorBufferInfo getVertUBInfo() {
    return vert_buf_info_;
  }
  VkDescriptorBufferInfo getFragUBInfo() {
    return frag_buf_info_;
  }

 private:
  bool initialized_ = false;
  size_t ub_offset_align_;
  VkDescriptorSetLayout descriptor_set_layout_;
  VkPipelineLayout pipeline_layout_;
  VkShaderModule vertex_module_;
  VkShaderModule fragment_module_;
  VkPipeline pipeline_;
  VkDescriptorBufferInfo vert_buf_info_;
  VkDescriptorBufferInfo frag_buf_info_;
  std::vector<RingBuffer::Allocation> ub_allocs_;
  VKContext* context_;
};

}  // namespace hwcomposer

#endif  // VK_PROGRAM_H_
