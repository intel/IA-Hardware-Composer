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
#include "displayplanestate.h"
#include "hwctrace.h"
#include "nativebufferhandler.h"

namespace hwcomposer {

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
    : native_handle_(0),
      buffer_handler_(NULL),
      width_(width),
      height_(height),
      in_use_(false),
      clear_surface_(true),
      framebuffer_format_(0) {
}

NativeSurface::~NativeSurface() {
  // Ensure we close any framebuffers before
  // releasing buffer.
  layer_.ResetBuffer();

  if (native_handle_) {
    buffer_handler_->DestroyHandle(native_handle_);
  }
}

bool NativeSurface::Init(NativeBufferHandler *buffer_handler, uint32_t format,
                         bool cursor_layer) {
  buffer_handler_ = buffer_handler;
  uint32_t usage = hwcomposer::kLayerNormal;
  if (cursor_layer) {
    usage = hwcomposer::kLayerCursor;
  }

  buffer_handler_->CreateBuffer(width_, height_, format, &native_handle_,
                                usage);
  if (!native_handle_) {
    ETRACE("NativeSurface: Failed to create buffer.");
    return false;
  }

  InitializeLayer(buffer_handler, native_handle_);
  return true;
}

bool NativeSurface::InitializeForOffScreenRendering(
    NativeBufferHandler *buffer_handler, HWCNativeHandle native_handle) {
  InitializeLayer(buffer_handler, native_handle);
  layer_.SetSourceCrop(HwcRect<float>(0, 0, width_, height_));
  layer_.SetDisplayFrame(HwcRect<int>(0, 0, width_, height_));


  return true;
}

void NativeSurface::SetNativeFence(int32_t fd) {
  layer_.SetAcquireFence(fd);
}

void NativeSurface::SetInUse(bool inuse) {
  in_use_ = inuse;
}

void NativeSurface::SetClearSurface(bool clear_surface) {
  clear_surface_ = clear_surface;
}

void NativeSurface::SetPlaneTarget(DisplayPlaneState &plane, uint32_t gpu_fd) {
  uint32_t format =
      plane.plane()->GetFormatForFrameBuffer(layer_.GetBuffer()->GetFormat());

  const HwcRect<int> &display_rect = plane.GetDisplayFrame();
  surface_damage_ = display_rect;
  last_surface_damage_ = surface_damage_;
  UpdateDisplayFrame(plane.GetDisplayFrame());
  plane.SetOverlayLayer(&layer_);
  SetInUse(true);

  if (framebuffer_format_ == format)
    return;

  framebuffer_format_ = format;
  layer_.GetBuffer()->SetRecommendedFormat(framebuffer_format_);
  layer_.GetBuffer()->CreateFrameBuffer(gpu_fd);
}

void NativeSurface::ResetDisplayFrame(const HwcRect<int> &display_frame) {
  surface_damage_ = display_frame;
  last_surface_damage_ = surface_damage_;
  layer_.SetDisplayFrame(display_frame);
  layer_.SetSourceCrop(HwcRect<float>(display_frame));
  clear_surface_ = true;
}

void NativeSurface::UpdateSurfaceDamage(
    const HwcRect<int> &currentsurface_damage,
    const HwcRect<int> &last_surface_damage) {
  surface_damage_.left =
      std::min(last_surface_damage.left, currentsurface_damage.left);
  surface_damage_.top =
      std::min(last_surface_damage.top, currentsurface_damage.top);
  surface_damage_.right =
      std::max(last_surface_damage.right, currentsurface_damage.right);
  surface_damage_.bottom =
      std::max(last_surface_damage.bottom, currentsurface_damage.bottom);
  last_surface_damage_ = currentsurface_damage;
}

void NativeSurface::UpdateDisplayFrame(const HwcRect<int> &display_frame) {
  layer_.SetDisplayFrame(display_frame);
  layer_.SetSourceCrop(HwcRect<float>(display_frame));
}

void NativeSurface::InitializeLayer(NativeBufferHandler *buffer_handler,
                                    HWCNativeHandle native_handle) {
  layer_.SetBlending(HWCBlending::kBlendingPremult);
  layer_.SetBuffer(buffer_handler, native_handle, -1);
}

}  // namespace hwcomposer
