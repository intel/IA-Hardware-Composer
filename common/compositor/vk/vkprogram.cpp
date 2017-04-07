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

#include "vkprogram.h"
#include "renderstate.h"

#include "hwctrace.h"

namespace hwcomposer {

static const uint8_t vkcomp_vert_spv[] = {
#include "vkcomp.vert.h"
};

static const uint8_t vkcomp_frag_spv[] = {
#include "vkcomp.frag.h"
};

VKProgram::VKProgram() {
  descriptor_set_layout_ = VK_NULL_HANDLE;
  pipeline_layout_ = VK_NULL_HANDLE;
  vertex_module_ = VK_NULL_HANDLE;
  fragment_module_ = VK_NULL_HANDLE;
  pipeline_ = VK_NULL_HANDLE;
  context_ = NULL;
}

VKProgram::~VKProgram() {
  if (!initialized_)
    return;

  VkDevice dev = context_->getDevice();
  vkDestroyDescriptorSetLayout(dev, descriptor_set_layout_, NULL);
  vkDestroyPipelineLayout(dev, pipeline_layout_, NULL);
  vkDestroyShaderModule(dev, vertex_module_, NULL);
  vkDestroyShaderModule(dev, fragment_module_, NULL);
  vkDestroyPipeline(dev, pipeline_, NULL);
}

bool VKProgram::Init(unsigned layer_index) {
  if (initialized_)
    return false;

  VkResult res;

  context_ = global_context_;
  VkDevice dev = context_->getDevice();
  VkPipelineCache pipeline_cache = context_->getPipelineCache();
  VkRenderPass render_pass = context_->getRenderPass();
  VkPhysicalDevice phys_dev = context_->getPhysicalDevice();

  VkPhysicalDeviceProperties device_props;
  vkGetPhysicalDeviceProperties(phys_dev, &device_props);
  ub_offset_align_ = device_props.limits.minUniformBufferOffsetAlignment;

  VkDescriptorSetLayoutBinding bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT,
       NULL},
      {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
       NULL},
      {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, layer_index,
       VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
  };

  VkDescriptorSetLayoutCreateInfo desc_create = {};
  desc_create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  desc_create.bindingCount = ARRAY_SIZE(bindings);
  desc_create.pBindings = &bindings[0];

  res = vkCreateDescriptorSetLayout(dev, &desc_create, NULL,
                                    &descriptor_set_layout_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateDescriptorSetLayout failed (%d)\n", res);
    return false;
  }

  VkPipelineLayoutCreateInfo pipeline_layout_create = {};
  pipeline_layout_create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create.setLayoutCount = 1;
  pipeline_layout_create.pSetLayouts = &descriptor_set_layout_;

  res = vkCreatePipelineLayout(dev, &pipeline_layout_create, NULL,
                               &pipeline_layout_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreatePipelineLayout failed (%d)\n", res);
    return false;
  }

  VkSpecializationMapEntry layer_count_spec = {};
  layer_count_spec.size = sizeof(uint32_t);

  VkShaderModuleCreateInfo module_create = {};
  module_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_create.codeSize = sizeof(vkcomp_vert_spv);
  module_create.pCode = (const uint32_t *)vkcomp_vert_spv;

  res = vkCreateShaderModule(dev, &module_create, NULL, &vertex_module_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateShaderModule failed (%d)\n", res);
    return false;
  }

  module_create.codeSize = sizeof(vkcomp_frag_spv);
  module_create.pCode = (const uint32_t *)vkcomp_frag_spv;

  res = vkCreateShaderModule(dev, &module_create, NULL, &fragment_module_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateShaderModule failed (%d)\n", res);
    return false;
  }

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkVertexInputBindingDescription vertex_input_binding = {};
  vertex_input_binding.binding = 0;
  vertex_input_binding.stride = sizeof(float) * 4;
  vertex_input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertex_input_attribs[2];
  vertex_input_attribs[0] = {};
  vertex_input_attribs[0].location = 0;
  vertex_input_attribs[0].binding = 0;
  vertex_input_attribs[0].format = VK_FORMAT_R32G32_SFLOAT;
  vertex_input_attribs[0].offset = 0;
  vertex_input_attribs[1] = {};
  vertex_input_attribs[1].location = 1;
  vertex_input_attribs[1].binding = 0;
  vertex_input_attribs[1].format = VK_FORMAT_R32G32_SFLOAT;
  vertex_input_attribs[1].offset = sizeof(float) * 2;

  VkPipelineVertexInputStateCreateInfo vertex_input = {};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &vertex_input_binding;
  vertex_input.vertexAttributeDescriptionCount =
      ARRAY_SIZE(vertex_input_attribs);
  vertex_input.pVertexAttributeDescriptions = &vertex_input_attribs[0];

  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterization = {};
  rasterization.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample = {};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blend_attach = {};
  blend_attach.colorWriteMask = 0xF;

  VkPipelineColorBlendStateCreateInfo blending = {};
  blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blending.attachmentCount = 1;
  blending.pAttachments = &blend_attach;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state = {};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = ARRAY_SIZE(dynamic_states);
  dynamic_state.pDynamicStates = &dynamic_states[0];

  struct pipeline_info pipeline_info;
  pipeline_info.layer_index = layer_index;
  pipeline_info.special = {};
  pipeline_info.special.mapEntryCount = 1;
  pipeline_info.special.pMapEntries = &layer_count_spec;
  pipeline_info.special.dataSize = sizeof(pipeline_info.layer_index);
  pipeline_info.special.pData = &pipeline_info.layer_index;

  pipeline_info.stages[0] = {};
  pipeline_info.stages[0].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  pipeline_info.stages[0].module = vertex_module_;
  pipeline_info.stages[0].pName = "main";
  pipeline_info.stages[0].pSpecializationInfo = &pipeline_info.special;

  pipeline_info.stages[1] = {};
  pipeline_info.stages[1].sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  pipeline_info.stages[1].module = fragment_module_;
  pipeline_info.stages[1].pName = "main";
  pipeline_info.stages[1].pSpecializationInfo = &pipeline_info.special;

  VkGraphicsPipelineCreateInfo pipeline_create = {};
  pipeline_create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create.stageCount = ARRAY_SIZE(pipeline_info.stages);
  pipeline_create.pStages = &pipeline_info.stages[0];
  pipeline_create.pVertexInputState = &vertex_input;
  pipeline_create.pInputAssemblyState = &input_assembly;
  pipeline_create.pViewportState = &viewport_state;
  pipeline_create.pRasterizationState = &rasterization;
  pipeline_create.pMultisampleState = &multisample;
  pipeline_create.pColorBlendState = &blending;
  pipeline_create.pDynamicState = &dynamic_state;
  pipeline_create.layout = pipeline_layout_;
  pipeline_create.renderPass = render_pass;

  res = vkCreateGraphicsPipelines(dev, pipeline_cache, 1, &pipeline_create,
                                  NULL, &pipeline_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateGraphicsPipelines failed (%d0\n", res);
    return false;
  }

  initialized_ = true;
  return true;
}

void VKProgram::UseProgram(const RenderState &state,
                           unsigned int viewport_width,
                           unsigned int viewport_height) {
  unsigned layer_count = state.layer_state_.size();
  VkBuffer uniform_buffer = context_->getUniformBuffer();
  RingBuffer *ring_buffer = context_->getRingBuffer();

  size_t vert_ub_size = 4 + 12 * layer_count;
  RingBuffer::Allocation vert_ub_alloc =
      ring_buffer->Allocate(vert_ub_size * sizeof(float), ub_offset_align_);
  if (!vert_ub_alloc) {
    ETRACE("Failed to allocate space for vert uniform buffer");
    return;
  }
  float *vert_ub = vert_ub_alloc.get<float>();

  vert_ub[0] = state.x_ / (float)viewport_width;
  vert_ub[1] = state.y_ / (float)viewport_height;
  vert_ub[2] = state.width_ / (float)viewport_width;
  vert_ub[3] = state.height_ / (float)viewport_height;
  vert_ub += 4;

  for (unsigned src_index = 0; src_index < layer_count; src_index++) {
    const RenderState::LayerState &src = state.layer_state_[src_index];

    vert_ub[0] = src.crop_bounds_[0];
    vert_ub[1] = src.crop_bounds_[1];
    vert_ub[2] = src.crop_bounds_[2] - src.crop_bounds_[0];
    vert_ub[3] = src.crop_bounds_[3] - src.crop_bounds_[1];
    vert_ub += 4;

    std::copy_n(src.texture_matrix_, 2, vert_ub);
    vert_ub += 4;  // 2 data + 2 padding
    std::copy_n(src.texture_matrix_ + 2, 2, vert_ub);
    vert_ub += 4;  // 2 data + 2 padding
  }

  size_t frag_ub_size = 4 * layer_count;
  RingBuffer::Allocation frag_ub_alloc =
      ring_buffer->Allocate(frag_ub_size * sizeof(float), ub_offset_align_);
  if (!frag_ub_alloc) {
    ETRACE("failed to allocate space for frag uniform buffer");
    return;
  }
  float *frag_ub = frag_ub_alloc.get<float>();

  for (unsigned src_index = 0; src_index < layer_count; src_index++) {
    frag_ub[0] = state.layer_state_[src_index].alpha_;
    frag_ub[1] = state.layer_state_[src_index].premult_;
    frag_ub += 4;  // 2 data + 2 padding
  }

  vert_buf_info_ = {};
  vert_buf_info_.buffer = uniform_buffer;
  vert_buf_info_.offset = vert_ub_alloc.offset();
  vert_buf_info_.range = vert_ub_size * sizeof(float);

  frag_buf_info_ = {};
  frag_buf_info_.buffer = uniform_buffer;
  frag_buf_info_.offset = frag_ub_alloc.offset();
  frag_buf_info_.range = frag_ub_size * sizeof(float);

  ub_allocs_.clear();
  ub_allocs_.emplace_back(std::move(vert_ub_alloc));
  ub_allocs_.emplace_back(std::move(frag_ub_alloc));
}

}  // namespace hwcomposer
