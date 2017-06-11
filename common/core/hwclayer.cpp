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

#include <hwclayer.h>

namespace hwcomposer {

HwcLayer::~HwcLayer() {
  if (release_fd_ > 0) {
    close(release_fd_);
  }

  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }
}

void HwcLayer::SetNativeHandle(HWCNativeHandle handle) {
  sf_handle_ = handle;
}

void HwcLayer::SetTransform(int32_t transform) {
  transform_ = transform;
}

void HwcLayer::SetAlpha(uint8_t alpha) {
  alpha_ = alpha;
}

void HwcLayer::SetBlending(HWCBlending blending) {
  blending_ = blending;
}

void HwcLayer::SetSourceCrop(const HwcRect<float>& source_crop) {
  source_crop_ = source_crop;
}

void HwcLayer::SetDisplayFrame(const HwcRect<int>& display_frame) {
  display_frame_ = display_frame;
}

void HwcLayer::SetSurfaceDamage(const HwcRegion& surface_damage) {
  uint32_t rects = surface_damage.size();
  surface_damage_changed_ = false;
  layer_content_changed_ = true;
  HwcRect<int> new_damage_rect;
  if (rects == 1) {
    const HwcRect<int>& rect = surface_damage.at(0);
    if ((rect.top == 0) && (rect.bottom == 0) && (rect.left == 0) &&
        (rect.right == 0)) {
      layer_content_changed_ = false;
    }

    new_damage_rect.left = rect.left;
    new_damage_rect.right = rect.right;
    new_damage_rect.bottom = rect.bottom;
    new_damage_rect.top = rect.top;
  } else if (rects == 0) {
    new_damage_rect = display_frame_;
  } else {
    const HwcRect<int>& damage_region = surface_damage.at(0);

    for (uint32_t r = 1; r < rects; r++) {
      const HwcRect<int>& rect = surface_damage.at(r);
      new_damage_rect.left = std::min(damage_region.left, rect.left);
      new_damage_rect.top = std::min(damage_region.top, rect.top);
      new_damage_rect.right = std::max(damage_region.right, rect.right);
      new_damage_rect.bottom = std::max(damage_region.bottom, rect.bottom);
    }
  }

  if (surface_damage_.left != new_damage_rect.left) {
    surface_damage_changed_ = true;
    surface_damage_.left = new_damage_rect.left;
  }

  if (surface_damage_.top != new_damage_rect.top) {
    surface_damage_changed_ = true;
    surface_damage_.top = new_damage_rect.top;
  }

  if (surface_damage_.right != new_damage_rect.right) {
    surface_damage_changed_ = true;
    surface_damage_.right = new_damage_rect.right;
  }

  if (surface_damage_.bottom != new_damage_rect.bottom) {
    surface_damage_changed_ = true;
    surface_damage_.bottom = new_damage_rect.bottom;
  }
}

void HwcLayer::SetReleaseFence(int32_t fd) {
  release_fd_ = fd;
}

int32_t HwcLayer::GetReleaseFence() {
  int32_t old_fd = release_fd_;
  release_fd_ = -1;
  return old_fd;
}

void HwcLayer::SetAcquireFence(int32_t fd) {
  acquire_fence_ = fd;
}

int32_t HwcLayer::GetAcquireFence() {
  int32_t old_fd = acquire_fence_;
  acquire_fence_ = -1;
  return old_fd;
}

}  // namespace hwcomposer
