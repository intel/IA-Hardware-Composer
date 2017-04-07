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

#include "nativevkresource.h"

#include "hwctrace.h"
#include "overlaybuffer.h"
#include "overlaylayer.h"

namespace hwcomposer {

bool NativeVKResource::PrepareResources(
    const std::vector<OverlayBuffer*>& buffers) {
  VkResult res;

  Reset();
  context_ = global_context_;
  VkDevice dev = context_->getDevice();

  for (auto& buffer : buffers) {
    struct vk_import import = buffer->ImportImage(dev);
    if (import.res != VK_SUCCESS) {
      ETRACE("Failed to make import image (%d)\n", import.res);
      return false;
    }

    VkImageViewCreateInfo view_create = {};
    view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create.image = import.image;
    view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create.format = NativeToVkFormat(buffer->GetFormat());
    view_create.components = {};
    view_create.components.r = VK_COMPONENT_SWIZZLE_R;
    view_create.components.g = VK_COMPONENT_SWIZZLE_G;
    view_create.components.b = VK_COMPONENT_SWIZZLE_B;
    view_create.components.a = VK_COMPONENT_SWIZZLE_A;
    view_create.subresourceRange = {};
    view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create.subresourceRange.levelCount = 1;
    view_create.subresourceRange.layerCount = 1;

    VkImageView image_view;
    res = vkCreateImageView(dev, &view_create, NULL, &image_view);
    if (res != VK_SUCCESS) {
      ETRACE("vkCreateImageView failed (%d)\n", res);
      return false;
    }

    struct vk_resource resource;
    resource.image = import.image;
    resource.image_view = image_view;
    resource.image_memory = import.memory;
    layer_textures_.emplace_back(resource);
  }
  return true;
}

NativeVKResource::~NativeVKResource() {
  Reset();
}

void NativeVKResource::Reset() {
  if (!context_)
    return;

  VkDevice dev = context_->getDevice();

  for (auto& layer : layer_textures_) {
    vkDestroyImage(dev, layer.image, NULL);
    vkDestroyImageView(dev, layer.image_view, NULL);
    vkFreeMemory(dev, layer.image_memory, NULL);
  }
  layer_textures_.clear();

  context_ = NULL;
}

GpuResourceHandle NativeVKResource::GetResourceHandle(
    uint32_t layer_index) const {
  if (layer_textures_.size() < layer_index) {
    struct vk_resource res = {};
    return res;
  }

  return layer_textures_.at(layer_index);
}

}  // namespace hwcomposer
