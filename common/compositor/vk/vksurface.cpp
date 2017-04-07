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

#include "vksurface.h"
#include "overlaybuffer.h"

namespace hwcomposer {

VKSurface::VKSurface(uint32_t width, uint32_t height)
    : NativeSurface(width, height) {
  surface_resource_ = {};
  surface_fb_ = VK_NULL_HANDLE;
  context_ = NULL;
}

VKSurface::~VKSurface() {
  if (!context_)
    return;

  VkDevice dev = context_->getDevice();
  vkDestroyFramebuffer(dev, surface_fb_, NULL);
  vkDestroyImageView(dev, surface_resource_.image_view, NULL);
  vkDestroyImage(dev, surface_resource_.image, NULL);
  vkFreeMemory(dev, surface_resource_.image_memory, NULL);
}

bool VKSurface::InitializeGPUResources() {
  VkResult res;

  context_ = global_context_;
  VkDevice dev = context_->getDevice();
  VkRenderPass render_pass = context_->getRenderPass();

  struct vk_import import = layer_.GetBuffer()->ImportImage(dev);
  if (import.res != VK_SUCCESS) {
    ETRACE("Failed to make import image (%d)\n", import.res);
    return false;
  }

  surface_resource_.image_memory = import.memory;
  surface_resource_.image = import.image;

  VkImageViewCreateInfo view_create = {};
  view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_create.image = surface_resource_.image;
  view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_create.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_create.components = {};
  view_create.components.r = VK_COMPONENT_SWIZZLE_R;
  view_create.components.g = VK_COMPONENT_SWIZZLE_G;
  view_create.components.b = VK_COMPONENT_SWIZZLE_B;
  view_create.components.a = VK_COMPONENT_SWIZZLE_A;
  view_create.subresourceRange = {};
  view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_create.subresourceRange.levelCount = 1;
  view_create.subresourceRange.layerCount = 1;

  res =
      vkCreateImageView(dev, &view_create, NULL, &surface_resource_.image_view);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateImageView failed (%d)\n", res);
    return false;
  }

  VkFramebufferCreateInfo framebuffer_create = {};
  framebuffer_create.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_create.renderPass = render_pass;
  framebuffer_create.attachmentCount = 1;
  framebuffer_create.pAttachments = &surface_resource_.image_view;
  framebuffer_create.width = GetWidth();
  framebuffer_create.height = GetHeight();
  framebuffer_create.layers = 1;

  res = vkCreateFramebuffer(dev, &framebuffer_create, NULL, &surface_fb_);
  if (res != VK_SUCCESS) {
    ETRACE("vkCreateFramebuffer failed (%d)\n", res);
    return false;
  }

  return true;
}

bool VKSurface::MakeCurrent() {
  if (surface_fb_ == VK_NULL_HANDLE && !InitializeGPUResources()) {
    ETRACE("Failed to initialize gpu resources.");
    return false;
  }

  context_->setSurface(&surface_resource_, surface_fb_);

  return true;
}

}  // namespace hwcomposer
