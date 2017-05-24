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

bool Headless::Initialize(OverlayBufferManager * /*buffer_manager*/) {
  return true;
}

bool Headless::Connect(const drmModeModeInfo & /*mode_info*/,
                       const drmModeConnector * /*connector*/) {
  return true;
}

void Headless::DisConnect() {
}

void Headless::ShutDown() {
}

bool Headless::onGetDisplayAttribute(uint32_t /*configHandle*/,
                                     HWCDisplayAttribute attribute,
                                     int32_t *pValue) const {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *pValue = 1;
      break;
    case HWCDisplayAttribute::kHeight:
      *pValue = 1;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *pValue = 60;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *pValue = 1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *pValue = 1;
      break;
    default:
      *pValue = -1;
      return false;
  }

  return true;
}

bool Headless::onGetDisplayConfigs(uint32_t* pNumConfigs, uint32_t* paConfigHandles) const {
  *pNumConfigs = 1;
  if (paConfigHandles)
    paConfigHandles[0] = 0;

  return true;
}

bool Headless::getName(uint32_t *size, char *name) const {
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

bool Headless::onSetActiveConfig(uint32_t /*configIndex*/) {
  return false;
}

bool Headless::onGetActiveConfig(uint32_t *configIndex) const {
  if (!configIndex)
    return false;

  configIndex[0] = 0;
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
