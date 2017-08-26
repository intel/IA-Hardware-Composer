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

#ifndef COMMON_DISPLAY_HEADLESS_H_
#define COMMON_DISPLAY_HEADLESS_H_

#include <nativedisplay.h>

#include <memory>
#include <vector>

namespace hwcomposer {

class Headless : public NativeDisplay {
 public:
  Headless(uint32_t gpu_fd, uint32_t pipe_id, uint32_t crtc_id);
  ~Headless() override;

  bool Initialize(NativeBufferHandler *buffer_handler) override;

  DisplayType Type() const override {
    return DisplayType::kHeadless;
  }

  uint32_t Pipe() const override {
    return 0;
  }

  uint32_t Width() const override {
    return 1;
  }

  uint32_t Height() const override {
    return 1;
  }

  uint32_t PowerMode() const override {
    return 0;
  }

  bool GetDisplayAttribute(uint32_t config, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;
  int GetDisplayPipe() override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers,
               int32_t *retire_fence) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void VSyncControl(bool enabled) override;
  bool CheckPlaneFormat(uint32_t format) override;

  uint32_t fd_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_HEADLESS_H_
