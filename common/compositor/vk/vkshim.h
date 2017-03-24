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

#ifndef VKSHIM_H_
#define VKSHIM_H_

#include <drm/drm_fourcc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_intel.h>
#include <vector>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

namespace hwcomposer {

VkFormat DrmToVkFormat(int drm_format);
VkFormat GbmToVkFormat(int gbm_format);

// Generic ring buffer that always allocates contiguous regions. Chunks can be
// freed in any order, but the in flight pointer (read_offset_) only advances
// when the oldest in flight chunk is free. It's optimized. for the case where
// the oldest chunks are the first to be freed, but this is not strictly
// required. Returned Allocation objects automatically free themselves when they
// go out of scope.
class RingBuffer {
 public:
  class Allocation {
   public:
    Allocation() = default;
    Allocation(RingBuffer *parent, uint8_t *ptr) {
      parent_ = parent;
      ptr_ = ptr;
    }
    Allocation(Allocation &&rhs) noexcept {
      parent_ = rhs.parent_;
      ptr_ = rhs.ptr_;
      rhs.parent_ = nullptr;
      rhs.ptr_ = nullptr;
    }

    ~Allocation() {
      CheckedFree();
    }

    Allocation &operator=(Allocation &&rhs) noexcept {
      if (&rhs == this)
        return *this;
      CheckedFree();

      parent_ = rhs.parent_;
      ptr_ = rhs.ptr_;
      rhs.parent_ = nullptr;
      rhs.ptr_ = nullptr;

      return *this;
    }

    explicit operator bool() const {
      return ptr_ != nullptr && parent_ != nullptr;
    }

    uint8_t *ptr() {
      return ptr_;
    }

    template <typename T>
    T *get() {
      return (T *)ptr_;
    }

    size_t offset() const {
      return ptr_ - parent_->buffer_;
    }

   private:
    RingBuffer *parent_ = nullptr;
    uint8_t *ptr_ = nullptr;

    void CheckedFree() {
      if (parent_ && ptr_)
        parent_->Free(ptr_);
    }
  };

  RingBuffer() = default;
  RingBuffer(uint8_t *buffer, size_t buffer_size)
      : buffer_(buffer), buffer_size_(buffer_size) {
  }

  explicit operator bool() const {
    return buffer_ != nullptr;
  }

  uint8_t *get() {
    return buffer_;
  }

  // If succesful, the returned allocation's offset within the ring buffer will
  // be aligned, but the allocation's pointer may not be. alignment must be a
  // power of two.
  Allocation Allocate(size_t size, size_t alignment);

 private:
  uint8_t *buffer_ = nullptr;
  size_t buffer_size_ = 0;
  size_t read_offset_ = 0;
  size_t write_offset_ = 0;
  struct jump_entry_t {
    size_t base;
    size_t jump;
    bool free = false;
    jump_entry_t(size_t b, size_t j) : base(b), jump(j) {
    }
  };
  std::vector<jump_entry_t> jump_queue_;

  bool IsSpanInUse(size_t first, size_t size);
  void Free(uint8_t *ptr);
};

extern VkDevice dev_;
extern VkInstance inst_;
extern VkRenderPass render_pass_;
extern VkPipelineCache pipeline_cache_;
extern VkBuffer uniform_buffer_;
extern VkSampler sampler_;
extern std::vector<VkImage> src_images_;
extern std::vector<VkImageView> src_image_views_;
extern std::vector<VkDescriptorImageInfo> src_image_infos_;
extern RingBuffer ring_buffer_;
extern std::vector<RingBuffer::Allocation> ub_allocs_;
extern size_t ub_offset_align_;
extern std::vector<VkImageMemoryBarrier> src_barrier_before_clear_;
extern VkImageMemoryBarrier dst_barrier_before_clear_;
extern VkFramebuffer framebuffer_;

}  // namespace hwcomposer

#endif  // VKSHIM_H_
