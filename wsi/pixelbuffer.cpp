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
                             uint32_t width, uint32_t height, uint32_t stride, uint32_t format,
                             void *addr, ResourceHandle &resource, bool is_cursor_buffer) {
  int layer_type =  is_cursor_buffer ? kLayerCursor : kLayerNormal;
  uint8_t* byteaddr = (uint8_t*) addr;

  if (!buffer_handler->CreateBuffer(width, height, format, &resource.handle_, layer_type)) {
    ETRACE("PixelBuffer: CreateBuffer failed");
    return;
  }

  HWCNativeHandle &handle = resource.handle_;
  if (!buffer_handler->ImportBuffer(handle)) {
    ETRACE("PixelBuffer: ImportBuffer failed");
    return;
  }

  if (handle->meta_data_.prime_fds_[0] <= 0) {
    ETRACE("PixelBuffer: prime_fd_ is invalid.");
    return;
  }

  size_t size = handle->meta_data_.height_ * handle->meta_data_.pitches_[0];
  uint8_t *ptr = (uint8_t *)Map(handle->meta_data_.prime_fds_[0], size);
  if (!ptr) {
    return;
  }

  for (uint32_t i = 0; i < height; i++)
    memcpy(ptr + i * handle->meta_data_.pitches_[0],
           byteaddr + i * stride,
           stride);

  Unmap(handle->meta_data_.prime_fds_[0], ptr, size);
  needs_texture_upload_ = false;

  orig_width_ = width;
  orig_height_ = height;
  orig_stride_ = stride;
}

void PixelBuffer::Initializetemp(const NativeBufferHandler *buffer_handler,
                                 uint32_t width, uint32_t height,
                                 uint32_t stride, uint32_t format, void *addr,
                                 bool is_cursor_buffer) {
  int layer_type = is_cursor_buffer ? kLayerCursor : kLayerNormal;
  uint8_t *byteaddr = (uint8_t *)addr;

  if (!buffer_handler->CreateBuffer(width, height, format, &handle_,
                                    layer_type)) {
    ETRACE("PixelBuffer: CreateBuffer failed");
    return;
  }

  if (!buffer_handler->ImportBuffer(handle_)) {
    ETRACE("PixelBuffer: ImportBuffer failed");
    return;
  }

  if (handle_->meta_data_.prime_fds_[0] <= 0) {
    ETRACE("PixelBuffer: prime_fd_ is invalid.");
    return;
  }

  size_t size = handle_->meta_data_.height_ * handle_->meta_data_.pitches_[0];
  uint8_t *ptr = (uint8_t *)Map(handle_->meta_data_.prime_fds_[0], size);
  if (!ptr) {
    return;
  }

  for (uint32_t i = 0; i < height; i++)
    memcpy(ptr + i * handle_->meta_data_.pitches_[0], byteaddr + i * stride,
           stride);

  Unmap(handle_->meta_data_.prime_fds_[0], ptr, size);
  needs_texture_upload_ = false;

  orig_width_ = width;
  orig_height_ = height;
  orig_stride_ = stride;
}

void PixelBuffer::Refresh(void *addr, const ResourceHandle &resource) {
  needs_texture_upload_ = true;
  const HWCNativeHandle &handle = resource.handle_;
  size_t size = handle->meta_data_.height_ * handle->meta_data_.pitches_[0];
  uint8_t *ptr = (uint8_t*) Map(handle->meta_data_.prime_fds_[0], size);
  if (!ptr) {
    return;
  }

  uint8_t* byteaddr = (uint8_t*) addr;
  for (int i = 0; i < orig_height_; i++)
	memcpy(ptr + i * handle->meta_data_.pitches_[0],
         byteaddr + i * orig_stride_,
         orig_stride_);

  Unmap(handle->meta_data_.prime_fds_[0], ptr, size);
  needs_texture_upload_ = false;
}
};
