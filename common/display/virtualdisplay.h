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

#ifndef COMMON_DISPLAY_VIRTUALDISPLAY_H_
#define COMMON_DISPLAY_VIRTUALDISPLAY_H_

#include <nativedisplay.h>

#include <memory>
#include <vector>

#include "compositor.h"
#include "resourcemanager.h"
#ifdef HYPER_DMABUF_SHARING
#include "hyperdmadisplay.h"
#endif

namespace hwcomposer {
struct HwcLayer;
class FrameBufferManager;
class NativeBufferHandler;

class VirtualDisplay : public NativeDisplay {
 public:
  VirtualDisplay(uint32_t gpu_fd, NativeBufferHandler *buffer_handler,
                 uint32_t pipe_id, uint32_t crtc_id);
  VirtualDisplay(const VirtualDisplay &) = delete;
  VirtualDisplay &operator=(const VirtualDisplay &) = delete;
  ~VirtualDisplay() override;

  void InitVirtualDisplay(uint32_t width, uint32_t height) override;

  bool GetActiveConfig(uint32_t *config) override;

  bool SetActiveConfig(uint32_t config) override;

  bool Present(std::vector<HwcLayer *> &source_layers, int32_t *retire_fence,
               PixelUploaderCallback *call_back = NULL,
               bool handle_constraints = false) override;

  void SetOutputBuffer(HWCNativeHandle buffer, int32_t acquire_fence) override;

  bool Initialize(NativeBufferHandler *buffer_handler) override;

  DisplayType Type() const override {
    return DisplayType::kVirtual;
  }

  uint32_t Width() const override {
    return width_;
  }

  uint32_t Height() const override {
    return height_;
  }

  uint32_t PowerMode() const override {
    return 0;
  }

  bool GetDisplayAttribute(uint32_t config, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;
  int GetDisplayPipe() override;

  bool SetPowerMode(uint32_t power_mode) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void VSyncControl(bool enabled) override;
  bool CheckPlaneFormat(uint32_t format) override;
  void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                            uint32_t pavp_instance_id) override {
    if (enabled) {
      discard_protected_video_ = false;
    } else {
      discard_protected_video_ = true;
    }
  }

 private:
  HWCNativeHandle output_handle_ = 0;
  int32_t acquire_fence_ = -1;
  Compositor compositor_;
  uint32_t width_ = 1;
  uint32_t height_ = 1;
  std::vector<OverlayLayer> in_flight_layers_;
  std::unique_ptr<ResourceManager> resource_manager_;
  uint32_t display_index_ = 0;
  bool discard_protected_video_ = false;

#ifdef HYPER_DMABUF_SHARING
  int mHyperDmaBuf_Fd = -1;
  std::map<uint32_t, vm_buffer_info>
      mHyperDmaExportedBuffers;  // Track the hyper dmabuf metadata info mapping
  uint32_t frame_count_ = 0;
#endif
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_VIRTUALDISPLAY_H_
