/*
// Copyright (c) 2018 Intel Corporation
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
*/

#include "pixelbuffer.h"

#include "resourcemanager.h"

namespace hwcomposer {

PixelBuffer::PixelBuffer() {
}

PixelBuffer::~PixelBuffer() {
}

void PixelBuffer::Initialize(const NativeBufferHandler *buffer_handler,
                             uint32_t width, uint32_t height, uint32_t format,
                             void *addr, ResourceHandle &resource) {
  if (!buffer_handler->CreateBuffer(width, height, format, &resource.handle_)) {
    ETRACE("PixelBuffer: CreateBuffer failed");
    return;
  }

  HWCNativeHandle &handle = resource.handle_;
  if (!buffer_handler->ImportBuffer(handle)) {
    ETRACE("PixelBuffer: ImportBuffer failed");
    return;
  }

  if (handle->meta_data_.prime_fd_ <= 0) {
    ETRACE("PixelBuffer: prime_fd_ is invalid.");
    return;
  }

  size_t size = handle->meta_data_.height_ * handle->meta_data_.pitches_[0];
  void *ptr = Map(handle->meta_data_.prime_fd_, size);
  if (!ptr) {
    return;
  }

  memcpy(ptr, addr, size);
  Unmap(handle->meta_data_.prime_fd_, ptr, size);
  needs_texture_upload_ = false;
}

void PixelBuffer::Refresh(void *addr, const ResourceHandle &resource) {
  needs_texture_upload_ = true;
  const HWCNativeHandle &handle = resource.handle_;
  size_t size = handle->meta_data_.height_ * handle->meta_data_.pitches_[0];
  void *ptr = Map(handle->meta_data_.prime_fd_, size);
  if (!ptr) {
    return;
  }

  memcpy(ptr, addr, size);
  Unmap(handle->meta_data_.prime_fd_, ptr, size);
  needs_texture_upload_ = false;
}
};
