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
}

VKSurface::~VKSurface() {
}

bool VKSurface::InitializeGPUResources() {
  VkResult res;

  dst_image_ = layer_.GetBuffer()->ImportImage(dev_);
  if (dst_image_ == VK_NULL_HANDLE) {
    printf("Failed to make import image\n");
    return false;
  }

  VkImageSubresourceRange clear_range = {};
  clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  clear_range.levelCount = 1;
  clear_range.layerCount = 1;

  dst_barrier_before_clear_ = {};
  dst_barrier_before_clear_.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  dst_barrier_before_clear_.srcAccessMask = 0;
  dst_barrier_before_clear_.dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dst_barrier_before_clear_.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  dst_barrier_before_clear_.newLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  dst_barrier_before_clear_.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  dst_barrier_before_clear_.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  dst_barrier_before_clear_.image = dst_image_;
  dst_barrier_before_clear_.subresourceRange = clear_range;

  VkImageViewCreateInfo view_create = {};
  view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_create.image = dst_image_;
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

  res = vkCreateImageView(dev_, &view_create, NULL, &dst_image_view_);
  if (res != VK_SUCCESS) {
    printf("vkCreateImageView failed (%d)\n", res);
    return false;
  }

  VkFramebufferCreateInfo framebuffer_create = {};
  framebuffer_create.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_create.renderPass = render_pass_;
  framebuffer_create.attachmentCount = 1;
  framebuffer_create.pAttachments = &dst_image_view_;
  framebuffer_create.width = GetWidth();
  framebuffer_create.height = GetHeight();
  framebuffer_create.layers = 1;

  res = vkCreateFramebuffer(dev_, &framebuffer_create, NULL, &framebuffer_);
  if (res != VK_SUCCESS) {
    printf("vkCreateFramebuffer failed (%d)\n", res);
    return false;
  }

  return true;
}

bool VKSurface::MakeCurrent() {
  if (!framebuffer_ && !InitializeGPUResources()) {
    printf("Failed to initialize gpu resources.");
    return false;
  }

  return true;
}

}  // namespace hwcomposer

