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

#include "vkrenderer.h"
#include "vkprogram.h"

#include "hwctrace.h"
#include "nativesurface.h"
#include "renderstate.h"

namespace hwcomposer {

VKRenderer::~VKRenderer() {
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
    uint64_t object, size_t location, int32_t messageCode,
    const char *pLayerPrefix, const char *pMessage, void *pUserData) {
  (void)flags;
  (void)objectType;
  (void)object;
  (void)location;
  (void)messageCode;
  (void)pLayerPrefix;
  (void)pUserData;
  if (objectType == VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT)
    return VK_FALSE;
  if (objectType == VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT &&
      messageCode == 3 && strcmp(pLayerPrefix, "ParameterValidation") == 0 &&
      strcmp(pMessage,
             "vkCreateImage: value of pCreateInfo->pNext must be NULL") == 0)
    return VK_FALSE;
  ETRACE("vulkan(%d): %s\n", (int)objectType, pMessage);
  return VK_FALSE;
}

uint32_t VKRenderer::GetMemoryTypeIndex(uint32_t mem_type_bits,
                                        uint32_t required_props) {
  for (uint32_t type_index = 0; type_index < 32;
       type_index++, mem_type_bits >>= 1) {
    if (mem_type_bits & 1) {
      if ((device_mem_props_.memoryTypes[type_index].propertyFlags &
           required_props) == required_props) {
        return type_index;
      }
    }
  }
  return 32;
}

VkBuffer VKRenderer::UploadBuffer(size_t data_size, const uint8_t *data,
                                  VkBufferUsageFlags usage) {
  VkResult res;
  VkBufferCreateInfo buffer_create = {};
  buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create.size = data_size;
  buffer_create.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VkBuffer src_buffer;
  res = vkCreateBuffer(dev_, &buffer_create, NULL, &src_buffer);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateBuffer failed (%d)\n", res);
    return NULL;
  }

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(dev_, src_buffer, &mem_requirements);
  VkMemoryAllocateInfo mem_allocate = {};
  mem_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_allocate.allocationSize = mem_requirements.size;
  mem_allocate.memoryTypeIndex = GetMemoryTypeIndex(
      mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  if (mem_allocate.memoryTypeIndex >= 32) {
    ETRACE("Failed to find suitable staging device memory\n");
    return NULL;
  }

  VkDeviceMemory host_mem;
  res = vkAllocateMemory(dev_, &mem_allocate, NULL, &host_mem);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateMemory failed (%d)\n", res);
    return NULL;
  }

  res = vkBindBufferMemory(dev_, src_buffer, host_mem, 0);
  if (res != VK_SUCCESS) {
    ETRACE("vkBindBufferMemory failed (%d)\n", res);
    return NULL;
  }

  uint8_t *src_ptr;
  res = vkMapMemory(dev_, host_mem, 0, mem_allocate.allocationSize, 0,
                    (void **)&src_ptr);
  if (res != VK_SUCCESS) {
    ETRACE("vkMapMemory failed (%d)\n", res);
    return NULL;
  }

  memcpy(src_ptr, data, data_size);

  vkUnmapMemory(dev_, host_mem);

  buffer_create.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer dst_buffer;
  res = vkCreateBuffer(dev_, &buffer_create, NULL, &dst_buffer);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateBuffer failed (%d)\n", res);
    return NULL;
  }
  vkGetBufferMemoryRequirements(dev_, dst_buffer, &mem_requirements);
  mem_allocate.allocationSize = mem_requirements.size;
  mem_allocate.memoryTypeIndex = GetMemoryTypeIndex(
      mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (mem_allocate.memoryTypeIndex >= 32) {
    ETRACE("Failed to find suitable buffer device memory");
    return NULL;
  }

  VkDeviceMemory device_mem;
  res = vkAllocateMemory(dev_, &mem_allocate, NULL, &device_mem);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateMemory failed (%d)\n", res);
    return NULL;
  }
  res = vkBindBufferMemory(dev_, dst_buffer, device_mem, 0);
  if (res != VK_SUCCESS) {
    ETRACE("vkBindBufferMemory failed (%d)\n", res);
    return NULL;
  }

  VkCommandBuffer cmd_buffer;
  VkCommandBufferAllocateInfo cmd_buffer_allocate = {};
  cmd_buffer_allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_buffer_allocate.commandPool = cmd_pool_;
  cmd_buffer_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_buffer_allocate.commandBufferCount = 1;

  res = vkAllocateCommandBuffers(dev_, &cmd_buffer_allocate, &cmd_buffer);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateCommandBuffers failed (%d)\n", res);
    return NULL;
  }

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  res = vkBeginCommandBuffer(cmd_buffer, &begin_info);
  if (res != VK_SUCCESS) {
    ETRACE("vkBeginCommandBuffer failed (%d)\n", res);
    return NULL;
  }

  VkBufferCopy buffer_copy = {};
  buffer_copy.size = data_size;

  vkCmdCopyBuffer(cmd_buffer, src_buffer, dst_buffer, 1, &buffer_copy);

  res = vkEndCommandBuffer(cmd_buffer);
  if (res != VK_SUCCESS) {
    ETRACE("vkEndCommandBuffer failed (%d)\n", res);
    return NULL;
  }

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd_buffer;

  res = vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE);
  if (res != VK_SUCCESS) {
    ETRACE("%d: vkQueueSubmit failed (%d)\n", __LINE__, res);
    return NULL;
  }

  res = vkQueueWaitIdle(queue_);
  if (res != VK_SUCCESS) {
    ETRACE("vkQueueWaitIdle failed (%d)\n", res);
    return NULL;
  }

  vkFreeCommandBuffers(dev_, cmd_pool_, 1, &cmd_buffer);
  vkFreeMemory(dev_, host_mem, NULL);

  return dst_buffer;
}

bool VKRenderer::Init() {
  VkResult res;

  const char *enabled_layers[] = {};

  const char *instance_extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
  };

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  VkInstanceCreateInfo instance_create = {};
  instance_create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create.pApplicationInfo = &app_info;
  instance_create.enabledLayerCount = ARRAY_SIZE(enabled_layers);
  instance_create.ppEnabledLayerNames = &enabled_layers[0];
  instance_create.enabledExtensionCount = ARRAY_SIZE(instance_extensions);
  instance_create.ppEnabledExtensionNames = &instance_extensions[0];

  res = vkCreateInstance(&instance_create, NULL, &inst_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateInstance failed (%d)\n", res);
    return false;
  }

  VkDebugReportCallbackCreateInfoEXT debug_create = {};
  debug_create.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
  debug_create.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                       VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                       VK_DEBUG_REPORT_ERROR_BIT_EXT |
                       VK_DEBUG_REPORT_DEBUG_BIT_EXT;
  debug_create.pfnCallback = &VulkanDebugReportCallback;
  VkDebugReportCallbackEXT callback;
  PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
      (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
          inst_, "vkCreateDebugReportCallbackEXT");
  if (vkCreateDebugReportCallbackEXT) {
    vkCreateDebugReportCallbackEXT(inst_, &debug_create, NULL, &callback);
  } else {
    ETRACE("Failed to create vulkan debug callback\n");
  }

  uint32_t count;
  res = vkEnumeratePhysicalDevices(inst_, &count, NULL);
  if (res != VK_SUCCESS) {
    ETRACE("vkEnumeratePhysicalDevices failed (%d)\n", res);
    return false;
  }
  if (count == 0) {
    ETRACE("No physical devices\n");
    return false;
  }

  VkPhysicalDevice phys_devs[count];
  res = vkEnumeratePhysicalDevices(inst_, &count, phys_devs);
  if (res != VK_SUCCESS) {
    ETRACE("vkEnumeratePhysicalDevices failed (%d)\n", res);
    return false;
  }

  VkPhysicalDevice phys_dev = phys_devs[0];

  vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &count, NULL);
  if (count == 0) {
    ETRACE("No device queue family properties\n");
    return false;
  }

  VkQueueFamilyProperties props[count];
  vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &count, props);
  if (!(props[0].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
    ETRACE("Not a graphics queue\n");
    return false;
  }

  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create = {};
  queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create.queueCount = 1;
  queue_create.pQueuePriorities = &queue_priority;

  const char *device_extensions[] = {};

  VkDeviceCreateInfo device_create = {};
  device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create.queueCreateInfoCount = 1;
  device_create.pQueueCreateInfos = &queue_create;
  device_create.enabledLayerCount = ARRAY_SIZE(enabled_layers);
  device_create.ppEnabledLayerNames = &enabled_layers[0];
  device_create.enabledExtensionCount = ARRAY_SIZE(device_extensions);
  device_create.ppEnabledExtensionNames = &device_extensions[0];

  res = vkCreateDevice(phys_dev, &device_create, NULL, &dev_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateDevice failed (%d)\n", res);
    return false;
  }

  vkGetPhysicalDeviceProperties(phys_dev, &device_props_);
  vkGetPhysicalDeviceMemoryProperties(phys_dev, &device_mem_props_);

  ub_offset_align_ = device_props_.limits.minUniformBufferOffsetAlignment;

  vkGetDeviceQueue(dev_, 0, 0, &queue_);

  VkCommandPoolCreateInfo pool_create = {};
  pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

  res = vkCreateCommandPool(dev_, &pool_create, NULL, &cmd_pool_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateCommandPool failed (%d)\n", res);
    return false;
  }

  // clang-format off
  const float verts[] = {0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 2.0f, 0.0f, 2.0f,
                         2.0f, 0.0f, 2.0f, 0.0f};
  // clang-format on

  vert_buffer_ = UploadBuffer(sizeof(verts), (const uint8_t *)verts,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  if (vert_buffer_ == NULL) {
    ETRACE("UploadBuffer failed\n");
    return false;
  }

  VkBufferCreateInfo buffer_create = {};
  buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create.size = 0x100 * 256;
  buffer_create.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  res = vkCreateBuffer(dev_, &buffer_create, NULL, &uniform_buffer_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateBuffer failed (%d)\n", res);
    return false;
  }

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(dev_, uniform_buffer_, &mem_requirements);

  VkMemoryAllocateInfo mem_allocate = {};
  mem_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_allocate.allocationSize = mem_requirements.size;
  mem_allocate.memoryTypeIndex = GetMemoryTypeIndex(
      mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  if (mem_allocate.memoryTypeIndex >= 32) {
    ETRACE("Failed to find suitable uniform buffer device memory\n");
    return false;
  }

  res = vkAllocateMemory(dev_, &mem_allocate, NULL, &uniform_buffer_mem_);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateMemory failed (%d)\n", res);
    return false;
  }

  res = vkBindBufferMemory(dev_, uniform_buffer_, uniform_buffer_mem_, 0);
  if (res != VK_SUCCESS) {
    ETRACE("vkBindBufferMemory failed (%d)\n", res);
    return false;
  }

  uint8_t *uniform_buffer_ptr;
  res = vkMapMemory(dev_, uniform_buffer_mem_, 0, mem_allocate.allocationSize,
                    0, (void **)&uniform_buffer_ptr);
  if (res != VK_SUCCESS) {
    ETRACE("vkMapMemory failed (%d)\n", res);
    return false;
  }

  ring_buffer_ = RingBuffer(uniform_buffer_ptr, buffer_create.size);

  VkDescriptorPoolSize pool_sizes[2];
  pool_sizes[0] = {};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 256;
  pool_sizes[1] = {};
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = 256;

  VkDescriptorPoolCreateInfo desc_pool_create = {};
  desc_pool_create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  desc_pool_create.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  desc_pool_create.maxSets = 256;
  desc_pool_create.poolSizeCount = ARRAY_SIZE(pool_sizes);
  desc_pool_create.pPoolSizes = &pool_sizes[0];

  res = vkCreateDescriptorPool(dev_, &desc_pool_create, NULL, &desc_pool_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateDescriptorPool failed (%d)\n", res);
    return false;
  }

  VkSamplerCreateInfo sampler_create = {};
  sampler_create.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create.magFilter = VK_FILTER_LINEAR;
  sampler_create.minFilter = VK_FILTER_LINEAR;
  sampler_create.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_create.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_create.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_create.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

  res = vkCreateSampler(dev_, &sampler_create, NULL, &sampler_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateSampler failed (%d)\n", res);
    return false;
  }

  VkAttachmentDescription attach_desc = {};
  attach_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  attach_desc.samples = VK_SAMPLE_COUNT_1_BIT;
  attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attach_desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attach_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attach = {};
  color_attach.attachment = 0;
  color_attach.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass_desc = {};
  subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &color_attach;

  VkSubpassDependency subpass_deps = {};
  subpass_deps.srcSubpass = 0;
  subpass_deps.dstSubpass = VK_SUBPASS_EXTERNAL;
  subpass_deps.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_deps.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  subpass_deps.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_deps.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  subpass_deps.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo pass_create = {};
  pass_create.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass_create.attachmentCount = 1;
  pass_create.pAttachments = &attach_desc;
  pass_create.subpassCount = 1;
  pass_create.pSubpasses = &subpass_desc;
  pass_create.dependencyCount = 1;
  pass_create.pDependencies = &subpass_deps;

  res = vkCreateRenderPass(dev_, &pass_create, NULL, &render_pass_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateRenderPass failed (%d)\n", res);
    return false;
  }

  VkPipelineCacheCreateInfo pipeline_cache_create = {};
  pipeline_cache_create.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

  res = vkCreatePipelineCache(dev_, &pipeline_cache_create, NULL,
                              &pipeline_cache_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreatePipelineCache failed (%d)\n", res);
    return false;
  }

  return true;
}

bool VKRenderer::Draw(const std::vector<RenderState> &render_states,
                      NativeSurface *surface) {
  VkResult res;
  uint32_t frame_width = surface->GetWidth();
  uint32_t frame_height = surface->GetHeight();
  // vk renderer should not support protected
  surface->GetLayer()->SetProtected(false);
  surface->MakeCurrent();

  src_image_infos_.clear();
  ub_allocs_.clear();
  std::vector<VkDescriptorSetLayout> desc_layouts;
  std::vector<VkDescriptorSet> desc_sets(render_states.size());
  std::vector<VkDescriptorBufferInfo> ub_infos;
  for (const RenderState &state : render_states) {
    unsigned size = state.layer_state_.size();
    if (size == 0)
      break;

    VKProgram *program = GetProgram(size);
    if (!program)
      continue;

    desc_layouts.emplace_back(program->getDescLayout());

    program->UseProgram(state, frame_width, frame_height);

    ub_infos.emplace_back(program->getVertUBInfo());
    ub_infos.emplace_back(program->getFragUBInfo());
  }

  VkDescriptorSetAllocateInfo alloc_desc_set = {};
  alloc_desc_set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_desc_set.descriptorPool = desc_pool_;
  alloc_desc_set.descriptorSetCount = (uint32_t)desc_layouts.size();
  alloc_desc_set.pSetLayouts = desc_layouts.data();

  res = vkAllocateDescriptorSets(dev_, &alloc_desc_set, desc_sets.data());
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateDescriptorSets failed (%d)\n", res);
    return false;
  }

  std::vector<VkWriteDescriptorSet> write_desc_sets;
  size_t src_image_infos_offset = 0;
  for (size_t cmd_index = 0; cmd_index < render_states.size(); cmd_index++) {
    const RenderState &state = render_states[cmd_index];
    size_t layer_count = state.layer_state_.size();
    VkDescriptorSet desc_set = desc_sets[cmd_index];

    VkWriteDescriptorSet write_desc_set = {};
    write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_desc_set.dstSet = desc_set;
    write_desc_set.dstBinding = 0;
    write_desc_set.descriptorCount = 1;
    write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_desc_set.pBufferInfo = &ub_infos[cmd_index * 2 + 0];
    write_desc_sets.emplace_back(write_desc_set);

    write_desc_set = {};
    write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_desc_set.dstSet = desc_set;
    write_desc_set.dstBinding = 1;
    write_desc_set.descriptorCount = 1;
    write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_desc_set.pBufferInfo = &ub_infos[cmd_index * 2 + 1];
    write_desc_sets.emplace_back(write_desc_set);

    write_desc_set = {};
    write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_desc_set.dstSet = desc_set;
    write_desc_set.dstBinding = 2;
    write_desc_set.descriptorCount = (uint32_t)layer_count;
    write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_desc_set.pImageInfo = &src_image_infos_[src_image_infos_offset];
    write_desc_sets.emplace_back(write_desc_set);

    src_image_infos_offset += layer_count;
  }

  vkUpdateDescriptorSets(dev_, write_desc_sets.size(), write_desc_sets.data(),
                         0, NULL);

  VkCommandBufferAllocateInfo cmd_buffer_alloc = {};
  cmd_buffer_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_buffer_alloc.commandPool = cmd_pool_;
  cmd_buffer_alloc.commandBufferCount = 1;

  VkCommandBuffer cmd_buffer;
  res = vkAllocateCommandBuffers(dev_, &cmd_buffer_alloc, &cmd_buffer);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateCommandBuffer failed (%d)\n", res);
    return false;
  }

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  res = vkBeginCommandBuffer(cmd_buffer, &begin_info);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateCommandBuffer failed (%d)\n", res);
    return false;
  }

  std::vector<VkImageMemoryBarrier> barrier_before_clear;
  barrier_before_clear.emplace_back(dst_barrier_before_clear_);
  barrier_before_clear.insert(barrier_before_clear.end(),
                              src_barrier_before_clear_.begin(),
                              src_barrier_before_clear_.end());

  vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL,
                       barrier_before_clear.size(),
                       barrier_before_clear.data());

  VkClearValue clear_value[1];
  clear_value[0] = {};
  clear_value[0].color.float32[0] = 0.0f;
  clear_value[0].color.float32[1] = 0.0f;
  clear_value[0].color.float32[2] = 0.0f;
  clear_value[0].color.float32[3] = 0.0f;

  VkExtent2D extent = {};
  extent.width = frame_width;
  extent.height = frame_height;

  VkRect2D rect = {};
  rect.extent = extent;

  VkRenderPassBeginInfo pass_begin = {};
  pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  pass_begin.renderPass = render_pass_;
  pass_begin.framebuffer = framebuffer_;
  pass_begin.renderArea = rect;
  pass_begin.clearValueCount = 1;
  pass_begin.pClearValues = &clear_value[0];

  vkCmdBeginRenderPass(cmd_buffer, &pass_begin, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {};
  viewport.width = (float)frame_width;
  viewport.height = (float)frame_height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

  VkDeviceSize zero_offset = 0;
  vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vert_buffer_, &zero_offset);

  size_t last_layer_count = 0;
  for (size_t cmd_index = 0; cmd_index < render_states.size(); cmd_index++) {
    const RenderState &state = render_states[cmd_index];
    size_t layer_count = state.layer_state_.size();
    VkDescriptorSet desc_set = desc_sets[cmd_index];

    VkRect2D scissor = {};
    scissor.offset = {
        .x = (int32_t)state.x_, .y = (int32_t)state.y_,
    };
    scissor.extent = {
        .width = (uint32_t)state.width_, .height = (uint32_t)state.height_,
    };

    VKProgram *program = GetProgram(layer_count);
    VkPipeline pipeline = program->getPipeline();
    VkPipelineLayout pipeline_layout = program->getPipeLayout();

    if (last_layer_count != layer_count) {
      vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      last_layer_count = layer_count;
    }

    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1, &desc_set, 0, NULL);

    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
  }

  vkCmdEndRenderPass(cmd_buffer);

  res = vkEndCommandBuffer(cmd_buffer);
  if (res != VK_SUCCESS) {
    ETRACE("vkEndCommandBuffer failed (%d)\n", res);
    return false;
  }

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd_buffer;

  res = vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE);
  if (res != VK_SUCCESS) {
    ETRACE("%d: vkQueueSubmit failed (%d)\n", __LINE__, res);
    return false;
  }

  res = vkQueueWaitIdle(queue_);
  if (res != VK_SUCCESS) {
    ETRACE("vkQueueWaitIdle failed (%d)\n", res);
    return false;
  }

  vkFreeCommandBuffers(dev_, cmd_pool_, 1, &cmd_buffer);

  res = vkFreeDescriptorSets(dev_, desc_pool_, desc_sets.size(),
                             desc_sets.data());
  if (res != VK_SUCCESS) {
    ETRACE("vkFreeDescriptorSets failed (%d)\n", res);
    return false;
  }

  return true;
}

void VKRenderer::InsertFence(int32_t kms_fence) {
}

void VKRenderer::SetExplicitSyncSupport(bool disable_explicit_sync) {
}

VKProgram *VKRenderer::GetProgram(unsigned texture_count) {
  if (programs_.size() >= texture_count) {
    VKProgram *program = programs_[texture_count - 1].get();
    if (program != 0)
      return program;
  }

  std::unique_ptr<VKProgram> program(new VKProgram());
  if (program->Init(texture_count)) {
    if (programs_.size() < texture_count)
      programs_.resize(texture_count);

    programs_[texture_count - 1] = std::move(program);
    return programs_[texture_count - 1].get();
  }

  return 0;
}

}  // namespace hwcomposer
