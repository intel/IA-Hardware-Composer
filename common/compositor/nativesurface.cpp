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
      clear_surface_(kFullClear),
      surface_age_(0) {
}

NativeSurface::~NativeSurface() {
  if (resource_manager_ && native_handle_) {
    ResourceHandle temp;
    temp.handle_ = native_handle_;
    resource_manager_->MarkResourceForDeletion(temp, false);
  }
}

bool NativeSurface::Init(ResourceManager *resource_manager, uint32_t format,
                         uint32_t usage) {
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

void NativeSurface::SetClearSurface(ClearType clear_surface) {
  if (clear_surface_ != clear_surface) {
    clear_surface_ = clear_surface;
    if (clear_surface_ == kFullClear) {
      damage_changed_ = true;
    }
  }
}

void NativeSurface::SetTransform(uint32_t transform) {
  layer_.SetTransform(transform);
}

void NativeSurface::SetSurfaceAge(uint32_t value) {
  surface_age_ = value;
}

bool NativeSurface::IsSurfaceDamageChanged() const {
  return damage_changed_;
}

void NativeSurface::SetPlaneTarget(const DisplayPlaneState &plane,
                                   uint32_t gpu_fd) {
  const HwcRect<int> &display_rect = plane.GetDisplayFrame();
  surface_damage_ = display_rect;
  damage_changed_ = true;
  in_use_ = true;
  clear_surface_ = kFullClear;
  surface_age_ = 0;
  if (layer_.GetBuffer()->GetFb() == 0) {
    layer_.GetBuffer()->CreateFrameBuffer(gpu_fd);
  }
}

void NativeSurface::ResetDisplayFrame(const HwcRect<int> &display_frame) {
  layer_.SetDisplayFrame(display_frame);
}

void NativeSurface::ResetSourceCrop(const HwcRect<float> &source_crop) {
  layer_.SetSourceCrop(source_crop);
}

void NativeSurface::UpdateSurfaceDamage(
    const HwcRect<int> &currentsurface_damage, bool forced) {
  if (surface_damage_.empty()) {
    surface_damage_ = currentsurface_damage;
    if (!surface_damage_.empty())
      damage_changed_ = true;

    return;
  }

  if (currentsurface_damage == surface_damage_) {
    if (!damage_changed_ && forced) {
      damage_changed_ = true;
    }

    return;
  }

  HwcRect<int> temp = surface_damage_;
  surface_damage_.left =
      std::min(surface_damage_.left, currentsurface_damage.left);
  surface_damage_.top =
      std::min(surface_damage_.top, currentsurface_damage.top);
  surface_damage_.right =
      std::max(surface_damage_.right, currentsurface_damage.right);
  surface_damage_.bottom =
      std::max(surface_damage_.bottom, currentsurface_damage.bottom);
  if (!damage_changed_) {
    damage_changed_ = true;
    if (!forced && (temp == surface_damage_)) {
      damage_changed_ = false;
    }
  }
}

void NativeSurface::ResetDamage() {
  surface_damage_ = HwcRect<int>(0, 0, 0, 0);
  damage_changed_ = false;
}

void NativeSurface::InitializeLayer(HWCNativeHandle native_handle) {
  layer_.SetBlending(HWCBlending::kBlendingPremult);
  layer_.SetBuffer(native_handle, -1, resource_manager_, false);
}

}  // namespace hwcomposer
