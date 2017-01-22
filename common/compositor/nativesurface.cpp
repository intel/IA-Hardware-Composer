/*
// Copyright (c) 2016 Intel Corporation
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

#include "nativesurface.h"

#include "hwctrace.h"
#include "nativebufferhandler.h"

namespace hwcomposer {

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
    : native_handle_(0),
      buffer_handler_(NULL),
      width_(width),
      height_(height),
      ref_count_(0),
      in_flight_(false) {
}

NativeSurface::~NativeSurface() {
  // Ensure we close any framebuffers before
  // releasing buffer.
  overlay_buffer_.reset(nullptr);
  if (buffer_handler_ && native_handle_) {
    buffer_handler_->DestroyBuffer(native_handle_);
  }
}

bool NativeSurface::Init(NativeBufferHandler *buffer_handler, uint32_t gpu_fd) {
  buffer_handler_ = buffer_handler;
  buffer_handler_->CreateBuffer(width_, height_, 0, &native_handle_);
  if (!native_handle_) {
    ETRACE("Failed to create buffer.");
    return false;
  }

  overlay_buffer_.reset(new hwcomposer::OverlayBuffer());
  overlay_buffer_->InitializeFromNativeHandle(native_handle_, buffer_handler_);
  overlay_buffer_->CreateFrameBuffer(gpu_fd);

  if (!InitializeGPUResources()) {
    ETRACE("Failed to initialize gpu resources.");
    return false;
  }

  ref_count_ = 0;
  width_ = overlay_buffer_->GetWidth();
  height_ = overlay_buffer_->GetHeight();

  return true;
}

bool NativeSurface::InitializeForOffScreenRendering(
    NativeBufferHandler *buffer_handler, HWCNativeHandle native_handle) {
  overlay_buffer_.reset(new hwcomposer::OverlayBuffer());
  overlay_buffer_->InitializeFromNativeHandle(native_handle, buffer_handler);

  if (!InitializeGPUResources()) {
    ETRACE("Failed to initialize gpu resources.");
    return false;
  }

  width_ = overlay_buffer_->GetWidth();
  height_ = overlay_buffer_->GetHeight();

  return true;
}

void NativeSurface::SetNativeFence(int fd) {
  fd_.Reset(fd);
}

void NativeSurface::SetInUse(bool inuse) {
  if (inuse) {
    ref_count_ = 3;
  } else if (ref_count_) {
    ref_count_--;
  }

  in_flight_ = false;
}

void NativeSurface::SetInFlightSurface() {
  in_flight_ = true;
}

}  // namespace hwcomposer
