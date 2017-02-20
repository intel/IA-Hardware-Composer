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

#ifndef COMMON_DISPLAY_DISPLAY_H_
#define COMMON_DISPLAY_DISPLAY_H_

#include <stdlib.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <nativedisplay.h>
#include <scopedfd.h>
#include <drmscopedtypes.h>

#include <memory>
#include <vector>

#include "platformdefines.h"
#include "pageflipeventhandler.h"

namespace hwcomposer {
class DisplayPlaneState;
class DisplayPlaneManager;
class DisplayQueue;
class NativeBufferHandler;
class GpuDevice;
class NativeSync;
struct HwcLayer;

class Display : public NativeDisplay {
 public:
  Display(uint32_t gpu_fd, uint32_t pipe_id, uint32_t crtc_id);
  ~Display() override;

  bool Initialize() override;

  DisplayType Type() const override {
    return DisplayType::kInternal;
  }

  uint32_t Pipe() const override {
    return pipe_;
  }

  int32_t Width() const override {
    return width_;
  }

  int32_t Height() const override {
    return height_;
  }

  int32_t GetRefreshRate() const override {
    return refresh_;
  }

  uint32_t PowerMode() const override {
    return power_mode_;
  }

  bool GetDisplayAttribute(uint32_t config, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void VSyncControl(bool enabled) override;

 protected:
  uint32_t CrtcId() const override {
    return crtc_id_;
  }

  bool Connect(const drmModeModeInfo &mode_info,
               const drmModeConnector *connector,
               NativeBufferHandler *buffer_handler) override;

  bool IsConnected() const override {
    return is_connected_;
  }

  void DisConnect() override;

  void ShutDown() override;

 private:
  void ShutDownPipe();

  NativeBufferHandler *buffer_handler_;
  uint32_t frame_;
  uint32_t crtc_id_;
  uint32_t pipe_;
  uint32_t connector_;
  int32_t width_;
  int32_t height_;
  int32_t dpix_;
  int32_t dpiy_;
  uint32_t gpu_fd_;
  float refresh_;
  bool is_connected_;
  bool is_powered_off_;
  uint32_t power_mode_;
  std::unique_ptr<PageFlipEventHandler> flip_handler_;
  std::unique_ptr<DisplayQueue> display_queue_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAY_H_
