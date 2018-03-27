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

#ifndef COMMON_COMPOSITOR_NATIVESURFACE_H_
#define COMMON_COMPOSITOR_NATIVESURFACE_H_

#include <memory>

#include "overlaylayer.h"
#include "platformdefines.h"

namespace hwcomposer {

class ResourceManager;
class DisplayPlaneState;

class NativeSurface {
 public:
  enum ClearType {
    kNone = 0,         // No need to clear the buffer.
    kFullClear = 1,    // Clear the whole buffer.
    kPartialClear = 2  // Clear rect equal to SurfaceDamage of this layer.
  };

  NativeSurface() = default;
  NativeSurface(uint32_t width, uint32_t height);
  NativeSurface(const NativeSurface& rhs) = delete;
  NativeSurface& operator=(const NativeSurface& rhs) = delete;

  virtual ~NativeSurface();

  bool Init(ResourceManager* resource_manager, uint32_t format, uint32_t usage);

  bool InitializeForOffScreenRendering(HWCNativeHandle native_handle,
                                       ResourceManager* resource_manager);

  virtual bool MakeCurrent() {
    return false;
  }

  int GetWidth() const {
    return width_;
  }

  int GetHeight() const {
    return height_;
  }

  OverlayLayer* GetLayer() {
    return &layer_;
  }

  void SetNativeFence(int32_t fd);

  void SetClearSurface(NativeSurface::ClearType clear_surface);

  // Set's the no of frames before this
  // surface goes from offscreen to onscreen
  // and than offscreen.
  // 2 indicates that the surface is in queue to
  // be updated during next vblank.
  // 1 indicates that the surface is now onscreen.
  // 0 indicates that the surface is offscreen
  // and is not yet queued to be presented.
  // -1 Surface can be deleted or recycled.
  void SetSurfaceAge(int age);

  int GetSurfaceAge() const {
    return surface_age_;
  }

  bool ClearSurface() const {
    return clear_surface_ == kFullClear;
  }

  bool IsPartialClear() const {
    return clear_surface_ == kPartialClear;
  }

  void SetPlaneTarget(const DisplayPlaneState& plane, uint32_t gpu_fd);

  // Resets DisplayFrame, SurfaceDamage to display_frame.
  void ResetDisplayFrame(const HwcRect<int>& display_frame);

  // Resets Source Crop to source_crop.
  void ResetSourceCrop(const HwcRect<float>& source_crop);

  // Set's Damage rect of this surface.
  void UpdateSurfaceDamage(const HwcRect<int>& currentsurface_damage,
                           bool force);

  // Resets damage of this surface to empty.
  void ResetDamage();

  // Return's damage area of this surface.
  const HwcRect<int>& GetSurfaceDamage() const {
    return surface_damage_;
  }

  // Applies rotation transform to this surface.
  void SetTransform(uint32_t transform);

  // Returns true in case damage of this surface has changed
  // compared to previous frame.
  bool IsSurfaceDamageChanged() const;

  // Returns true if this surface has been scanned out with the plane
  // it's associated with currently.
  bool IsOnScreen() const {
    return on_screen_;
  }

 protected:
  OverlayLayer layer_;
  ResourceManager* resource_manager_;

 private:
  void InitializeLayer(HWCNativeHandle native_handle);
  HWCNativeHandle native_handle_;
  int width_;
  int height_;
  ClearType clear_surface_;
  int surface_age_;
  bool damage_changed_ = true;
  bool on_screen_ = false;
  HwcRect<int> surface_damage_;
  HwcRect<int> previous_damage_;
  HwcRect<int> previous_nc_damage_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_NATIVESURFACE_H_
