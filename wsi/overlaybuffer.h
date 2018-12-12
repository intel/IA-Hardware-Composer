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
#ifndef WSI_OVERLAYBUFFER_H_
#define WSI_OVERLAYBUFFER_H_

#include <platformdefines.h>

#include <memory>

#include "compositordefs.h"

#include "hwcdefs.h"

namespace hwcomposer {

class FrameBufferManager;
class NativeBufferHandler;
class ResourceManager;

class OverlayBuffer {
 public:
  static std::shared_ptr<OverlayBuffer> CreateOverlayBuffer();

  OverlayBuffer(OverlayBuffer&& rhs) = default;
  OverlayBuffer& operator=(OverlayBuffer&& other) = default;
  OverlayBuffer() = default;

  virtual ~OverlayBuffer() {
  }

  virtual void InitializeFromNativeHandle(
      HWCNativeHandle handle, ResourceManager* buffer_manager,
      FrameBufferManager* frame_buffer_manager) = 0;

  virtual uint32_t GetDataSpace() const = 0;

  virtual uint32_t GetWidth() const = 0;

  virtual uint32_t GetHeight() const = 0;

  virtual uint32_t GetFormat() const = 0;

  virtual HWCLayerType GetUsage() const = 0;

  virtual uint32_t GetFb() const = 0;

  virtual uint32_t GetPrimeFD() const = 0;

  virtual const uint32_t* GetPitches() const = 0;

  virtual const uint32_t* GetOffsets() const = 0;

  virtual uint32_t GetTilingMode() const = 0;

  virtual void SetDataSpace(uint32_t dataspace) = 0;

  virtual bool GetInterlace() = 0;

  virtual void SetInterlace(bool isInterlaced) = 0;

  // external_import should be true if this resource is not owned by HWC.
  // If resource is owned by HWC, than the implementation needs to create
  // frame buffer for this buffer.
  virtual const ResourceHandle& GetGpuResource(GpuDisplay egl_display,
                                               bool external_import) = 0;

  virtual const ResourceHandle& GetGpuResource() = 0;

  // Returns Media resource for this buffer which can be used by compositor.
  // Surface will be clipped to width, height even if buffer size is
  // greater than these values.
  virtual const MediaResourceHandle& GetMediaResource(MediaDisplay display,
                                                      uint32_t width,
                                                      uint32_t height) = 0;

  virtual bool CreateFrameBuffer() = 0;

  // Creates Framebuffer taking into account any Modifiers.
  virtual bool CreateFrameBufferWithModifier(uint64_t modifier) = 0;

  virtual HWCNativeHandle GetOriginalHandle() const = 0;

  virtual void SetOriginalHandle(HWCNativeHandle handle) = 0;

  virtual void Dump() = 0;
};

}  // namespace hwcomposer
#endif  // WSI_OVERLAYBUFFER_H_
