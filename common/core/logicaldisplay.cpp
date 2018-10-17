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

#include "logicaldisplay.h"

#include <sstream>
#include <string>

#include "logicaldisplaymanager.h"

namespace hwcomposer {

LogicalDisplay::LogicalDisplay(LogicalDisplayManager *display_manager,
                               NativeDisplay *physical_display,
                               uint32_t total_divisions, uint32_t index)
    : logical_display_manager_(display_manager),
      physical_display_(physical_display),
      index_(index),
      total_divisions_(total_divisions) {
}

LogicalDisplay::~LogicalDisplay() {
}

bool LogicalDisplay::Initialize(NativeBufferHandler * /*buffer_handler*/,
                                FrameBufferManager * /*frame_buffer_manager*/) {
  return true;
}

bool LogicalDisplay::IsConnected() const {
  return physical_display_->IsConnected();
}

uint32_t LogicalDisplay::PowerMode() const {
  return power_mode_;
}

int LogicalDisplay::GetDisplayPipe() {
  return physical_display_->GetDisplayPipe();
}

bool LogicalDisplay::SetActiveConfig(uint32_t config) {
  bool success = physical_display_->SetActiveConfig(config);
  width_ = (physical_display_->Width()) / total_divisions_;
  return success;
}

bool LogicalDisplay::GetActiveConfig(uint32_t *config) {
  return physical_display_->GetActiveConfig(config);
}

bool LogicalDisplay::SetPowerMode(uint32_t power_mode) {
  power_mode_ = power_mode;
  logical_display_manager_->UpdatePowerMode();
  return true;
}

void LogicalDisplay::SetHDCPState(HWCContentProtection state,
                                  HWCContentType content_type) {
  logical_display_manager_->SetHDCPState(state, content_type);
}

void LogicalDisplay::SetHDCPSRM(const int8_t *SRM, uint32_t SRMLength) {
  logical_display_manager_->SetHDCPSRM(SRM, SRMLength);
}

bool LogicalDisplay::ContainConnector(const uint32_t connector_id) {
  return logical_display_manager_->ContainConnector(connector_id);
}

bool LogicalDisplay::Present(std::vector<HwcLayer *> &source_layers,
                             int32_t *retire_fence,
                             PixelUploaderCallback *call_back,
                             bool handle_constraints) {
  if (power_mode_ != kOn)
    return true;

  return logical_display_manager_->Present(source_layers, retire_fence,
                                           call_back, handle_constraints);
}

bool LogicalDisplay::PresentClone(NativeDisplay * /*display*/) {
  return false;
}

int LogicalDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  vsync_callback_ = callback;
  return 0;
}

void LogicalDisplay::RegisterRefreshCallback(
    std::shared_ptr<RefreshCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  refresh_callback_ = callback;
}

void LogicalDisplay::RegisterHotPlugCallback(
    std::shared_ptr<HotPlugCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  hotplug_callback_ = callback;
  logical_display_manager_->RegisterHotPlugNotification();
}

void LogicalDisplay::VSyncControl(bool enabled) {
  enable_vsync_ = enabled;
  logical_display_manager_->UpdateVSyncControl();
}

void LogicalDisplay::VSyncUpdate(int64_t timestamp) {
  if (vsync_callback_ && enable_vsync_) {
    vsync_callback_->Callback(display_id_, timestamp);
  }
}

void LogicalDisplay::RefreshUpdate() {
  if (refresh_callback_ && power_mode_ == kOn) {
    refresh_callback_->Callback(display_id_);
  }
}

void LogicalDisplay::HotPlugUpdate(bool connected) {
  if (hotplug_callback_) {
    hotplug_callback_->Callback(display_id_, connected);
  }
}

bool LogicalDisplay::CheckPlaneFormat(uint32_t format) {
  return physical_display_->CheckPlaneFormat(format);
}

void LogicalDisplay::SetGamma(float red, float green, float blue) {
  physical_display_->SetGamma(red, green, blue);
}

void LogicalDisplay::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  physical_display_->SetContrast(red, green, blue);
}

void LogicalDisplay::SetBrightness(uint32_t red, uint32_t green,
                                   uint32_t blue) {
  physical_display_->SetBrightness(red, green, blue);
}

void LogicalDisplay::SetExplicitSyncSupport(bool disable_explicit_sync) {
  physical_display_->SetExplicitSyncSupport(disable_explicit_sync);
}

void LogicalDisplay::SetVideoScalingMode(uint32_t mode) {
  physical_display_->SetVideoScalingMode(mode);
}

void LogicalDisplay::SetVideoColor(HWCColorControl color, float value) {
  physical_display_->SetVideoColor(color, value);
}

void LogicalDisplay::GetVideoColor(HWCColorControl color, float *value,
                                   float *start, float *end) {
  physical_display_->GetVideoColor(color, value, start, end);
}

void LogicalDisplay::RestoreVideoDefaultColor(HWCColorControl color) {
  physical_display_->RestoreVideoDefaultColor(color);
}

void LogicalDisplay::SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                                         HWCDeinterlaceControl mode) {
  physical_display_->SetVideoDeinterlace(flag, mode);
}

void LogicalDisplay::RestoreVideoDefaultDeinterlace() {
  physical_display_->RestoreVideoDefaultDeinterlace();
}

void LogicalDisplay::SetCanvasColor(uint16_t bpc, uint16_t red, uint16_t green,
                                    uint16_t blue, uint16_t alpha) {
  physical_display_->SetCanvasColor(bpc, red, green, blue, alpha);
}

void LogicalDisplay::UpdateScalingRatio(uint32_t /*primary_width*/,
                                        uint32_t /*primary_height*/,
                                        uint32_t /*display_width*/,
                                        uint32_t /*display_height*/) {
}

void LogicalDisplay::CloneDisplay(NativeDisplay * /*source_display*/) {
}

bool LogicalDisplay::GetDisplayAttribute(uint32_t config /*config*/,
                                         HWCDisplayAttribute attribute,
                                         int32_t *value) {
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      physical_display_->GetDisplayAttribute(config, attribute, value);
      *value = *value / total_divisions_;
      return true;
    default:
      break;
  }

  return physical_display_->GetDisplayAttribute(config, attribute, value);
}

bool LogicalDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                       uint32_t *configs) {
  return physical_display_->GetDisplayConfigs(num_configs, configs);
}

bool LogicalDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Logical";
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
