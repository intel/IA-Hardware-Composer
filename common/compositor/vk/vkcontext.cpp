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

#include "vkcontext.h"
#include "hwctrace.h"

namespace hwcomposer {

VKContext::~VKContext() {
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, render_pass_, NULL);
    vkDestroyPipelineCache(device_, pipeline_cache_, NULL);
    vkDestroySampler(device_, sampler_, NULL);
    vkFreeMemory(device_, uniform_buffer_mem_, NULL);
    vkDestroyBuffer(device_, uniform_buffer_, NULL);
  }
  vkDestroyDevice(device_, NULL);
  vkDestroyInstance(instance_, NULL);
}

uint32_t VKContext::GetMemoryTypeIndex(uint32_t mem_type_bits,
                                       uint32_t required_props) {
  VkPhysicalDeviceMemoryProperties device_mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &device_mem_props);

  for (uint32_t type_index = 0; type_index < 32;
       type_index++, mem_type_bits >>= 1) {
    if (mem_type_bits & 1) {
      if ((device_mem_props.memoryTypes[type_index].propertyFlags &
           required_props) == required_props) {
        return type_index;
      }
    }
  }
  return 32;
}

bool VKContext::Init() {
  if (initialized_)
    return false;

  VkResult res;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  const char *enabled_layers[] = {};
  const char *instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME};
  const char *device_extensions[] = {};

  VkInstanceCreateInfo instance_create = {};
  instance_create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create.pApplicationInfo = &app_info;
  instance_create.enabledLayerCount = ARRAY_SIZE(enabled_layers);
  instance_create.ppEnabledLayerNames = &enabled_layers[0];
  instance_create.enabledExtensionCount = ARRAY_SIZE(instance_extensions);
  instance_create.ppEnabledExtensionNames = &instance_extensions[0];

  res = vkCreateInstance(&instance_create, NULL, &instance_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateInstance failed (%d)\n", res);
    return false;
  }

  uint32_t count;
  res = vkEnumeratePhysicalDevices(instance_, &count, NULL);
  if (res != VK_SUCCESS) {
    ETRACE("vkEnumeratePhysicalDevices failed (%d)\n", res);
    return false;
  }
  if (count == 0) {
    ETRACE("No physical devices\n");
    return false;
  }

  VkPhysicalDevice phys_devs[count];
  res = vkEnumeratePhysicalDevices(instance_, &count, phys_devs);
  if (res != VK_SUCCESS) {
    ETRACE("vkEnumeratePhysicalDevices failed (%d)\n", res);
    return false;
  }

  physical_device_ = phys_devs[0];

  vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &count, NULL);
  if (count == 0) {
    ETRACE("No device queue family properties\n");
    return false;
  }

  VkQueueFamilyProperties props[count];
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &count, props);
  if (!(props[0].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
    ETRACE("Not a graphics queue\n");
    return false;
  }

  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create = {};
  queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create.queueCount = 1;
  queue_create.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo device_create = {};
  device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create.queueCreateInfoCount = 1;
  device_create.pQueueCreateInfos = &queue_create;
  device_create.enabledLayerCount = ARRAY_SIZE(enabled_layers);
  device_create.ppEnabledLayerNames = &enabled_layers[0];
  device_create.enabledExtensionCount = ARRAY_SIZE(device_extensions);
  device_create.ppEnabledExtensionNames = &device_extensions[0];

  res = vkCreateDevice(physical_device_, &device_create, NULL, &device_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateDevice failed (%d)\n", res);
    return false;
  }

  VkBufferCreateInfo buffer_create = {};
  buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create.size = 0x100 * 256;
  buffer_create.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  res = vkCreateBuffer(device_, &buffer_create, NULL, &uniform_buffer_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateBuffer failed (%d)\n", res);
    return false;
  }

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(device_, uniform_buffer_, &mem_requirements);

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

  res = vkAllocateMemory(device_, &mem_allocate, NULL, &uniform_buffer_mem_);
  if (res != VK_SUCCESS) {
    ETRACE("vkAllocateMemory failed (%d)\n", res);
    return false;
  }

  res = vkBindBufferMemory(device_, uniform_buffer_, uniform_buffer_mem_, 0);
  if (res != VK_SUCCESS) {
    ETRACE("vkBindBufferMemory failed (%d)\n", res);
    return false;
  }

  uint8_t *uniform_buffer_ptr;
  res =
      vkMapMemory(device_, uniform_buffer_mem_, 0, mem_allocate.allocationSize,
                  0, (void **)&uniform_buffer_ptr);
  if (res != VK_SUCCESS) {
    ETRACE("vkMapMemory failed (%d)\n", res);
    return false;
  }

  ring_buffer_ = RingBuffer(uniform_buffer_ptr, buffer_create.size);

  VkSamplerCreateInfo sampler_create = {};
  sampler_create.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create.magFilter = VK_FILTER_LINEAR;
  sampler_create.minFilter = VK_FILTER_LINEAR;
  sampler_create.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_create.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_create.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_create.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

  res = vkCreateSampler(device_, &sampler_create, NULL, &sampler_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateSampler failed (%d)\n", res);
    return false;
  }

  VkPipelineCacheCreateInfo pipeline_cache_create = {};
  pipeline_cache_create.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

  res = vkCreatePipelineCache(device_, &pipeline_cache_create, NULL,
                              &pipeline_cache_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreatePipelineCache failed (%d)\n", res);
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

  res = vkCreateRenderPass(device_, &pass_create, NULL, &render_pass_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateRenderPass failed (%d)\n", res);
    return false;
  }

  initialized_ = true;
  return true;
}

VKContext *global_context_;

}  // namespace hwcomposer
