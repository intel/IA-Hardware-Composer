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

#include "displayplane.h"
#include "hwctrace.h"
#include "nativebufferhandler.h"
#include "overlaybuffermanager.h"

namespace hwcomposer {

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
    : native_handle_(0),
      buffer_handler_(NULL),
      width_(width),
      height_(height),
      framebuffer_format_(0),
      in_use_(false) {
}

NativeSurface::~NativeSurface() {
  // Ensure we close any framebuffers before
  // releasing buffer.
  if (layer_.GetBuffer())
    layer_.GetBuffer()->ReleaseFrameBuffer();

  if (buffer_handler_ && native_handle_) {
    buffer_handler_->DestroyBuffer(native_handle_);
  }
}

bool NativeSurface::Init(OverlayBufferManager *buffer_manager) {
  buffer_handler_ = buffer_manager->GetNativeBufferHandler();
  buffer_handler_->CreateBuffer(width_, height_, 0, &native_handle_);
  if (!native_handle_) {
    ETRACE("Failed to create buffer.");
    return false;
  }

  InitializeLayer(buffer_manager, native_handle_);

  return true;
}

bool NativeSurface::InitializeForOffScreenRendering(
    OverlayBufferManager *buffer_manager, HWCNativeHandle native_handle) {
  InitializeLayer(buffer_manager, native_handle);
  layer_.SetSourceCrop(HwcRect<float>(0, 0, width_, height_));
  layer_.SetDisplayFrame(HwcRect<int>(0, 0, width_, height_));

  return true;
}

void NativeSurface::SetNativeFence(int fd) {
  fd_.Reset(fd);
}

void NativeSurface::SetInUse(bool inuse) {
  in_use_ = inuse;
}

void NativeSurface::SetPlaneTarget(DisplayPlaneState &plane, uint32_t gpu_fd) {
  uint32_t format =
      plane.plane()->GetFormatForFrameBuffer(layer_.GetBuffer()->GetFormat());

  const HwcRect<int> &display_rect = plane.GetDisplayFrame();
  layer_.SetSourceCrop(HwcRect<float>(display_rect));
  layer_.SetDisplayFrame(HwcRect<int>(display_rect));
  width_ = display_rect.right - display_rect.left;
  height_ = display_rect.bottom - display_rect.top;
  plane.SetOverlayLayer(&layer_);
  SetInUse(true);

  if (framebuffer_format_ == format)
    return;

  framebuffer_format_ = format;
  layer_.GetBuffer()->SetRecommendedFormat(framebuffer_format_);
  layer_.GetBuffer()->CreateFrameBuffer(gpu_fd);
}

void NativeSurface::InitializeLayer(OverlayBufferManager *buffer_manager,
                                    HWCNativeHandle native_handle) {
  buffer_.reset(new OverlayBuffer());
  buffer_->InitializeFromNativeHandle(native_handle,
                                      buffer_manager->GetNativeBufferHandler());
  imported_buffer_.reset(new ImportedBuffer(buffer_.get(), buffer_manager, -1));
  width_ = buffer_->GetWidth();
  height_ = buffer_->GetHeight();
  layer_.SetNativeHandle(native_handle_);
  layer_.SetBlending(HWCBlending::kBlendingPremult);
  layer_.SetTransform(0);
  layer_.SetBuffer(imported_buffer_.get());
}

}  // namespace hwcomposer
