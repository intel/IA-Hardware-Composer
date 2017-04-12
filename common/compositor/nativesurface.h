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

#include "displayplanestate.h"
#include "nativefence.h"
#include "platformdefines.h"

namespace hwcomposer {

class NativeBufferHandler;
class OverlayBufferManager;

class NativeSurface {
 public:
  NativeSurface() = default;
  NativeSurface(uint32_t width, uint32_t height);
  NativeSurface(const NativeSurface& rhs) = delete;
  NativeSurface& operator=(const NativeSurface& rhs) = delete;

  virtual ~NativeSurface();

  bool Init(OverlayBufferManager* buffer_manager);

  bool InitializeForOffScreenRendering(OverlayBufferManager* buffer_manager,
                                       HWCNativeHandle native_handle);

  virtual bool MakeCurrent() = 0;

  uint32_t GetWidth() const {
    return width_;
  }

  uint32_t GetHeight() const {
    return height_;
  }

  OverlayLayer* GetLayer() {
    return &layer_;
  }

  HWCNativeHandle GetNativeHandle() const {
    return native_handle_;
  }

  void SetNativeFence(int fd);
  int ReleaseNativeFence() {
    return fd_.Release();
  }

  void SetInUse(bool inuse);

  bool InUse() const {
    return in_use_;
  }

  void SetPlaneTarget(DisplayPlaneState& plane, uint32_t gpu_fd);

  void ResetInFlightMode();

 protected:
  OverlayLayer layer_;

 private:
  void InitializeLayer(OverlayBufferManager* buffer_manager,
                       HWCNativeHandle native_handle);
  HWCNativeHandle native_handle_;
  NativeBufferHandler* buffer_handler_;
  uint32_t width_;
  uint32_t height_;
  bool in_use_;
  uint32_t framebuffer_format_;
  NativeFence fd_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_NATIVESURFACE_H_
