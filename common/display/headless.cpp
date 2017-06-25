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

#include "headless.h"

#include <algorithm>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace hwcomposer {

Headless::Headless(uint32_t gpu_fd, uint32_t /*pipe_id*/, uint32_t /*crtc_id*/)
    : fd_(gpu_fd) {
}

Headless::~Headless() {
}

bool Headless::Initialize(NativeBufferHandler * /*buffer_manager*/) {
  return true;
}

bool Headless::GetDisplayAttribute(uint32_t /*config*/,
                                   HWCDisplayAttribute attribute,
                                   int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = 1;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = 1;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *value = 60;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = 1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value = 1;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

bool Headless::GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
  *num_configs = 1;
  if (configs)
    configs[0] = 0;

  return true;
}

bool Headless::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Headless";
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length - 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

int Headless::GetDisplayPipe() {
  return -1;
}

bool Headless::SetActiveConfig(uint32_t /*config*/) {
  return false;
}

bool Headless::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 0;
  return true;
}

bool Headless::SetPowerMode(uint32_t /*power_mode*/) {
  return true;
}

bool Headless::Present(std::vector<HwcLayer *> & /*source_layers*/,
                       int32_t * /*retire_fence*/) {
  return true;
}

int Headless::RegisterVsyncCallback(std::shared_ptr<VsyncCallback> /*callback*/,
                                    uint32_t /*display_id*/) {
  return 0;
}

void Headless::VSyncControl(bool /*enabled*/) {
}

bool Headless::CheckPlaneFormat(uint32_t /*format*/) {
  // assuming that virtual display supports the format
  return true;
}

}  // namespace hwcomposer
