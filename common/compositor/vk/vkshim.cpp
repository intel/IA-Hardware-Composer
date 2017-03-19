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

#include "vkshim.h"
#include <gbm.h>
#include <stdio.h>

namespace hwcomposer {

VkDevice dev_;
VkInstance inst_;
VkRenderPass render_pass_;
VkPipelineCache pipeline_cache_;
VkBuffer uniform_buffer_;
VkSampler sampler_;
VkImage dst_image_;
VkImageView dst_image_view_;
std::vector<VkImage> src_images_;
std::vector<VkImageView> src_image_views_;
std::vector<VkDescriptorImageInfo> src_image_infos_;
RingBuffer ring_buffer_;
std::vector<RingBuffer::Allocation> ub_allocs_;
size_t ub_offset_align_;
std::vector<VkImageMemoryBarrier> src_barrier_before_clear_;
VkImageMemoryBarrier dst_barrier_before_clear_;
VkFramebuffer framebuffer_;

VkFormat DrmToVkFormat(int drm_format) {
  switch (drm_format) {
    case DRM_FORMAT_BGR888:
      return VK_FORMAT_R8G8B8_UNORM;
    case DRM_FORMAT_ARGB8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case DRM_FORMAT_XBGR8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_ABGR8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_BGR565:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

VkFormat GbmToVkFormat(int gbm_format) {
  switch (gbm_format) {
    case GBM_FORMAT_XRGB8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    default:
      printf("%s: gbm_format %d unhandled\n", __func__, gbm_format);
      return VK_FORMAT_UNDEFINED;
  }
}

RingBuffer::Allocation RingBuffer::Allocate(size_t size, size_t alignment) {
  if (size > buffer_size_)
    return Allocation();

  size_t base = write_offset_;
  size_t padding = ((alignment - (base & (alignment - 1))) & (alignment - 1));
  size_t jump = padding + size;
  if (base + padding + size > buffer_size_) {
    base = 0;
    padding = 0;
    jump = buffer_size_ - write_offset_ + size;
  }

  if (IsSpanInUse(base, size)) {
    return Allocation();
  }

  jump_queue_.emplace_back(base + padding, jump);

  write_offset_ += jump;
  write_offset_ %= buffer_size_;

  return Allocation(this, buffer_ + base + padding);
}

void RingBuffer::Free(uint8_t *ptr) {
  size_t base = ptr - buffer_;
  bool found = false;
  for (jump_entry_t &entry : jump_queue_) {
    if (entry.base == base) {
      entry.free = true;
      found = true;
      break;
    }
  }

  auto it = jump_queue_.begin();
  for (; it != jump_queue_.end(); ++it) {
    if (!it->free)
      break;
    read_offset_ += it->jump;
    read_offset_ %= buffer_size_;
  }

  jump_queue_.erase(jump_queue_.begin(), it);

  if (jump_queue_.empty()) {
    write_offset_ = 0;
    read_offset_ = 0;
  }

#ifndef NDEBUG
  if (!jump_queue_.empty()) {
    auto next = jump_queue_.front();
  }
#endif
}

bool RingBuffer::IsSpanInUse(size_t first, size_t size) {
  if (jump_queue_.empty())
    return false;

  size_t last = first + size - 1;

  size_t use_first = read_offset_;
  size_t use_last = (write_offset_ == 0 ? buffer_size_ : write_offset_) - 1;
  if (use_last < use_first)
    use_last += buffer_size_;

  if (first <= use_first && last >= use_first)
    return true;

  if (first <= use_last && last >= use_last)
    return true;

  if (use_first <= first && use_last >= first)
    return true;

  if (use_first <= last && use_last >= last)
    return true;

  return false;
}

}  // namespace hwcomposer

