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
#include "resourcemanager.h"

namespace hwcomposer {

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
    : native_handle_(0),
      width_(width),
      height_(height),
      in_use_(false),
      clear_surface_(true),
      framebuffer_format_(0) {
}

NativeSurface::~NativeSurface() {
  if (resource_manager_ && native_handle_) {
    ResourceHandle temp;
    temp.handle_ = native_handle_;
    resource_manager_->MarkResourceForDeletion(temp, false);
  }
}

bool NativeSurface::Init(ResourceManager *resource_manager, uint32_t format,
                         bool cursor_layer) {
  uint32_t usage = hwcomposer::kLayerNormal;
  if (cursor_layer) {
    usage = hwcomposer::kLayerCursor;
  }

  resource_manager->GetNativeBufferHandler()->CreateBuffer(
      width_, height_, format, &native_handle_, usage);
  if (!native_handle_) {
    ETRACE("NativeSurface: Failed to create buffer.");
    return false;
  }

  resource_manager_ = resource_manager;
  InitializeLayer(native_handle_);
  return true;
}

bool NativeSurface::InitializeForOffScreenRendering(
    HWCNativeHandle native_handle, ResourceManager *resource_manager) {
  resource_manager_ = resource_manager;
  InitializeLayer(native_handle);
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
  ResetDisplayFrame(plane.GetDisplayFrame());
  ResetSourceCrop(plane.GetSourceCrop());

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
  clear_surface_ = true;
}

void NativeSurface::ResetSourceCrop(const HwcRect<float> &source_crop) {
  layer_.SetSourceCrop(source_crop);
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

void NativeSurface::InitializeLayer(HWCNativeHandle native_handle) {
  layer_.SetBlending(HWCBlending::kBlendingPremult);
  layer_.SetBuffer(native_handle, -1, resource_manager_, false);
}

}  // namespace hwcomposer
