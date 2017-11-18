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

#include "overlaybuffer.h"
#include "overlaylayer.h"
#include "platformdefines.h"

namespace hwcomposer {

class NativeBufferHandler;
class DisplayPlaneState;

class NativeSurface {
 public:
  NativeSurface() = default;
  NativeSurface(uint32_t width, uint32_t height);
  NativeSurface(const NativeSurface& rhs) = delete;
  NativeSurface& operator=(const NativeSurface& rhs) = delete;

  virtual ~NativeSurface();

  bool Init(NativeBufferHandler* buffer_handler, uint32_t format,
            bool cursor_layer = false);

  bool InitializeForOffScreenRendering(NativeBufferHandler* buffer_handler,
                                       HWCNativeHandle native_handle);

  virtual bool MakeCurrent() = 0;

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

  void SetInUse(bool inuse);
  void SetClearSurface(bool clear_surface);

  bool InUse() const {
    return in_use_;
  }

  bool ClearSurface() const {
    return clear_surface_;
  }

  void SetPlaneTarget(DisplayPlaneState& plane, uint32_t gpu_fd);

  // Resets DisplayFrame, SurfaceDamage to display_frame.
  void ResetDisplayFrame(const HwcRect<int>& display_frame);
  void UpdateSurfaceDamage(const HwcRect<int>& currentsurface_damage,
                           const HwcRect<int>& last_surface_damage);
  void UpdateDisplayFrame(const HwcRect<int>& display_frame);

  const HwcRect<int>& GetLastSurfaceDamage() const {
    return last_surface_damage_;
  }
  const HwcRect<int>& GetSurfaceDamage() const {
    return surface_damage_;
  }

 protected:
  OverlayLayer layer_;

 private:
  void InitializeLayer(NativeBufferHandler* buffer_handler,
                       HWCNativeHandle native_handle);
  HWCNativeHandle native_handle_;
  NativeBufferHandler* buffer_handler_;
  int width_;
  int height_;
  bool in_use_;
  bool clear_surface_;
  uint32_t framebuffer_format_;
  HwcRect<int> surface_damage_;
  HwcRect<int> last_surface_damage_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_NATIVESURFACE_H_
