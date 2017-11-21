/*
// Copyright (c) 2017 Intel Corporation
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

#include "nesteddisplay.h"

#include <nativebufferhandler.h>

#include <string>
#include <sstream>

#include <hwctrace.h>

namespace hwcomposer {

NestedDisplay::NestedDisplay() {
}

NestedDisplay::~NestedDisplay() {
}

void NestedDisplay::InitNestedDisplay() {
}

bool NestedDisplay::Initialize(NativeBufferHandler * /*buffer_handler*/) {
  return true;
}

bool NestedDisplay::IsConnected() const {
  return true;
}

uint32_t NestedDisplay::PowerMode() const {
  return power_mode_;
}

int NestedDisplay::GetDisplayPipe() {
  return -1;
}

bool NestedDisplay::SetActiveConfig(uint32_t config) {
  config_ = config;
  return true;
}

bool NestedDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 0;
  return true;
}

bool NestedDisplay::SetPowerMode(uint32_t power_mode) {
  return true;
}

bool NestedDisplay::Present(std::vector<HwcLayer *> &source_layers,
                             int32_t *retire_fence, bool handle_constraints) {
  // TODO implement Present
  return true;
}

bool NestedDisplay::PresentClone(std::vector<HwcLayer *> & /*source_layers*/,
                                  int32_t * /*retire_fence*/,
                                  bool /*idle_frame*/) {
  return false;
}

int NestedDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  vsync_callback_ = callback;
  return 0;
}

void NestedDisplay::RegisterRefreshCallback(
    std::shared_ptr<RefreshCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  refresh_callback_ = callback;
}

void NestedDisplay::RegisterHotPlugCallback(
    std::shared_ptr<HotPlugCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  hotplug_callback_ = callback;
}

void NestedDisplay::VSyncControl(bool enabled) {
  enable_vsync_ = enabled;
}

void NestedDisplay::VSyncUpdate(int64_t timestamp) {
  if (vsync_callback_ && enable_vsync_) {
    vsync_callback_->Callback(display_id_, timestamp);
  }
}

void NestedDisplay::RefreshUpdate() {
  if (refresh_callback_ && power_mode_ == kOn) {
    refresh_callback_->Callback(display_id_);
  }
}

void NestedDisplay::HotPlugUpdate(bool connected) {
  if (hotplug_callback_) {
    IHOTPLUGEVENTTRACE("NestedDisplay RegisterHotPlugCallback: id: %d display: %p",
                       display_id_, this);
    hotplug_callback_->Callback(display_id_, true);
  }
}

bool NestedDisplay::CheckPlaneFormat(uint32_t format) {
  // assuming that virtual display supports the format
  return true;
}

void NestedDisplay::SetGamma(float red, float green, float blue) {
}

void NestedDisplay::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
}

void NestedDisplay::SetBrightness(uint32_t red, uint32_t green,
                                   uint32_t blue) {
}

void NestedDisplay::SetExplicitSyncSupport(bool disable_explicit_sync) {
}

void NestedDisplay::UpdateScalingRatio(uint32_t /*primary_width*/,
                                        uint32_t /*primary_height*/,
                                        uint32_t /*display_width*/,
                                        uint32_t /*display_height*/) {
}

void NestedDisplay::CloneDisplay(NativeDisplay * /*source_display*/) {
}

bool NestedDisplay::GetDisplayAttribute(uint32_t config /*config*/,
                                         HWCDisplayAttribute attribute,
                                         int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = 1920;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = 1080;
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

bool NestedDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                       uint32_t *configs) {
  *num_configs = 1;
  if (configs)
    configs[0] = 0;

  return true;
}

bool NestedDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Nested";
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

}  // namespace hwcomposer
