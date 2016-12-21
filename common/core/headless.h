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

#ifndef HEADLESS_H_
#define HEADLESS_H_

#include <nativedisplay.h>

namespace hwcomposer {

class NativeBufferHandler;

class Headless : public NativeDisplay {
 public:
  Headless(uint32_t gpu_fd, NativeBufferHandler &handler, uint32_t pipe_id,
           uint32_t crtc_id);
  ~Headless();

  bool Initialize() override;

  DisplayType Type() const override {
    return DisplayType::kHeadless;
  }

  uint32_t Pipe() const override {
    return 0;
  }

  int32_t Width() const override {
    return 1;
  }

  int32_t Height() const override {
    return 1;
  }

  int32_t GetRefreshRate() const override {
    return 0;
  }

  bool GetDisplayAttribute(uint32_t config, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetDpmsMode(uint32_t dpms_mode) override;

  bool Present(std::vector<hwcomposer::HwcLayer *> &source_layers) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void VSyncControl(bool enabled) override;

 protected:
  uint32_t CrtcId() const override {
    return 0;
  }

  bool Connect(const drmModeModeInfo &mode_info,
               const ScopedDrmConnectorPtr &connector) override;

  bool IsConnected() const override {
    return false;
  }

  void DisConnect() override;

  void ShutDown() override;

  uint32_t fd_;
};

}  // namespace hwcomposer
#endif  // HEADLESS_H_
