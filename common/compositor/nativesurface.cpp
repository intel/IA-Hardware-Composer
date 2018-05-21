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
#include "hwcutils.h"
#include "nativebufferhandler.h"
#include "resourcemanager.h"

namespace hwcomposer {

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
    : resource_manager_(NULL),
      native_handle_(0),
      width_(width),
      height_(height),
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
                         uint32_t usage, uint64_t modifier,
                         bool *modifier_succeeded,
                         FrameBufferManager *frame_buffer_manager) {
  fb_manager_ = frame_buffer_manager;
  const NativeBufferHandler *handler =
      resource_manager->GetNativeBufferHandler();
  resource_manager_ = resource_manager;
  HWCNativeHandle native_handle = 0;
  *modifier_succeeded = false;
  bool modifier_used = false;

  if (usage == hwcomposer::kLayerVideo) {
    modifier = 0;
  }

  handler->CreateBuffer(width_, height_, format, &native_handle, usage,
                        &modifier_used, modifier);
  if (!native_handle) {
    ETRACE("Failed to create buffer\n");
    return false;
  }

  InitializeLayer(native_handle);

  if (modifier_used && modifier > 0) {
    if (!layer_.GetBuffer()->CreateFrameBufferWithModifier(modifier)) {
      WTRACE("FB creation failed with modifier, removing modifier usage\n");
      ResourceHandle temp;
      temp.handle_ = native_handle;
      resource_manager_->MarkResourceForDeletion(temp, false);
      native_handle = 0;

      handler->CreateBuffer(width_, height_, format, &native_handle, usage, 0);
      if (!native_handle) {
        ETRACE("Failed to create buffer\n");
        return false;
      }

      InitializeLayer(native_handle);
    } else {
      *modifier_succeeded = true;
    }
  }

  modifier_ = modifier;
  native_handle_ = native_handle;

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

void NativeSurface::SetClearSurface(ClearType clear_surface) {
  if (clear_surface_ != clear_surface) {
    clear_surface_ = clear_surface;
    if (clear_surface_ != kNone) {
      damage_changed_ = true;
    }
  }
}

void NativeSurface::SetTransform(uint32_t transform) {
  layer_.SetTransform(transform);
}

void NativeSurface::SetSurfaceAge(int value) {
  surface_age_ = value;
  if (surface_age_ >= 0) {
    on_screen_ = true;
  } else {
    on_screen_ = false;
  }
}

bool NativeSurface::IsSurfaceDamageChanged() const {
  return damage_changed_;
}

void NativeSurface::SetPlaneTarget(const DisplayPlaneState &plane) {
  HwcRect<int> &current_damage = layer_.GetSurfaceDamage();
  CalculateRect(layer_.GetDisplayFrame(), current_damage);
  CalculateRect(plane.GetDisplayFrame(), current_damage);
  previous_damage_ = current_damage;
  previous_nc_damage_ = current_damage;
  clear_surface_ = kFullClear;
  damage_changed_ = true;
  on_screen_ = false;
  surface_age_ = 0;
  if (layer_.GetBuffer()->GetFb() == 0) {
    layer_.GetBuffer()->CreateFrameBuffer();
  }
}

void NativeSurface::ResetDisplayFrame(const HwcRect<int> &display_frame) {
  layer_.SetDisplayFrame(display_frame);
}

void NativeSurface::ResetSourceCrop(const HwcRect<float> &source_crop) {
  layer_.SetSourceCrop(source_crop);
}

void NativeSurface::UpdateSurfaceDamage(
    const HwcRect<int> &currentsurface_damage, bool force) {
  HwcRect<int> current_damage = currentsurface_damage;
  if (current_damage.right > width_) {
    current_damage.right = width_;
  }

  if (current_damage.bottom > height_) {
    current_damage.bottom = height_;
  }

  HwcRect<int> &surface_damage = layer_.GetSurfaceDamage();
  if (reset_damage_) {
    reset_damage_ = false;
    surface_damage.reset();
  }

  if (surface_damage.empty()) {
    surface_damage = current_damage;
    damage_changed_ = true;

    if (!surface_damage.empty()) {
      CalculateRect(previous_nc_damage_, surface_damage);

      previous_nc_damage_ = current_damage;
    }

    if (!force && (previous_damage_ == surface_damage))
      damage_changed_ = false;

    return;
  }

  CalculateRect(current_damage, previous_nc_damage_);

  if (current_damage == surface_damage) {
    return;
  }

  CalculateRect(current_damage, surface_damage);

  if (!damage_changed_) {
    damage_changed_ = true;
    if (!force && (previous_damage_ == surface_damage))
      damage_changed_ = false;
  }
}

void NativeSurface::ResetDamage() {
  reset_damage_ = true;
  previous_damage_ = layer_.GetSurfaceDamage();
  damage_changed_ = false;
}

void NativeSurface::InitializeLayer(HWCNativeHandle native_handle) {
  layer_.SetBlending(HWCBlending::kBlendingPremult);
  layer_.SetBuffer(native_handle, -1, resource_manager_, false, fb_manager_);
}

}  // namespace hwcomposer
