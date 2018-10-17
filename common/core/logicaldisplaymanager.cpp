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

#include "logicaldisplaymanager.h"

#include <hwclayer.h>
#include "logicaldisplay.h"

namespace hwcomposer {

class LDMVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  LDMVsyncCallback(LogicalDisplayManager* manager) : manager_(manager) {
  }

  void Callback(uint32_t /*display*/, int64_t timestamp) {
    manager_->VSyncCallback(timestamp);
  }

 private:
  LogicalDisplayManager* manager_;
};

class LDMRefreshCallback : public hwcomposer::RefreshCallback {
 public:
  LDMRefreshCallback(LogicalDisplayManager* manager) : manager_(manager) {
  }

  void Callback(uint32_t /*display*/) {
    manager_->RefreshCallback();
  }

 private:
  LogicalDisplayManager* manager_;
};

class LDMHotPlugEventCallback : public hwcomposer::HotPlugCallback {
 public:
  LDMHotPlugEventCallback(LogicalDisplayManager* manager) : manager_(manager) {
  }

  void Callback(uint32_t /*display*/, bool connected) {
    manager_->HotPlugCallback(connected);
  }

 private:
  LogicalDisplayManager* manager_;
};

LogicalDisplayManager::LogicalDisplayManager(NativeDisplay* physical_display) {
  physical_display_ = physical_display;
}

LogicalDisplayManager::~LogicalDisplayManager() {
  physical_display_->RegisterVsyncCallback(nullptr, 0);
  physical_display_->RegisterRefreshCallback(nullptr, 0);
  physical_display_->RegisterHotPlugCallback(nullptr, 0);
}

void LogicalDisplayManager::InitializeLogicalDisplays(uint32_t total) {
  for (uint32_t i = 0; i < total; i++) {
    std::unique_ptr<LogicalDisplay> display(
        new LogicalDisplay(this, physical_display_, total, i));
    displays_.emplace_back(std::move(display));
  }

  auto r_callback = std::make_shared<LDMRefreshCallback>(this);
  physical_display_->RegisterRefreshCallback(
      r_callback, physical_display_->GetDisplayPipe());

  auto v_callback = std::make_shared<LDMVsyncCallback>(this);
  physical_display_->RegisterVsyncCallback(v_callback,
                                           physical_display_->GetDisplayPipe());
}

void LogicalDisplayManager::UpdatePowerMode() {
  bool power_off = true;
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    if (displays_.at(i)->PowerMode() != kOff) {
      power_off = false;
      break;
    }
  }

  if (power_off) {
    physical_display_->SetPowerMode(kOff);
  } else {
    physical_display_->SetPowerMode(kOn);
  }
}

void LogicalDisplayManager::UpdateVSyncControl() {
  bool vsync_control = false;
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    if (displays_.at(i)->EnableVSync()) {
      vsync_control = true;
      break;
    }
  }

  if (vsync_control) {
    physical_display_->VSyncControl(true);
  } else {
    physical_display_->VSyncControl(false);
  }
}

void LogicalDisplayManager::RegisterHotPlugNotification() {
  if (hot_plug_registered_)
    return;

  hot_plug_registered_ = true;
  handle_hoplug_notifications_ = true;

  auto h_callback = std::make_shared<LDMHotPlugEventCallback>(this);
  physical_display_->RegisterHotPlugCallback(
      std::move(h_callback), physical_display_->GetDisplayPipe());
}

bool LogicalDisplayManager::Present(std::vector<HwcLayer*>& source_layers,
                                    int32_t* retire_fence,
                                    PixelUploaderCallback* call_back,
                                    bool handle_constraints) {
  uint32_t total_size = displays_.size();
  if (handle_hoplug_notifications_) {
    uint32_t size = displays_.size();
    for (uint32_t i = 1; i < size; i++) {
      displays_.at(i)->HotPlugUpdate(true);
    }
    handle_hoplug_notifications_ = false;
    // In Mosaic case, we expect Present calls for all
    // logical displays.
    if (!handle_constraints)
      total_size = 1;
  } else {
    uint32_t size = displays_.size();
    for (uint32_t i = 0; i < size; i++) {
      if (displays_.at(i)->PowerMode() == kOff) {
        total_size--;
      }
    }
  }

  if (total_size == 0) {
    std::vector<HwcLayer*>().swap(cursor_layers_);
    std::vector<HwcLayer*>().swap(layers_);
    queued_displays_ = 0;
    ETRACE("logical dpm total_size == 0 \n");
    return true;
  }

  if (queued_displays_ != total_size) {
    uint32_t size = source_layers.size();
    for (uint32_t i = 0; i < size; i++) {
      HwcLayer* layer = source_layers.at(i);
      if (layer->IsCursorLayer()) {
        cursor_layers_.emplace_back(layer);
      } else {
        layers_.emplace_back(layer);
      }
    }

    queued_displays_++;
    if (queued_displays_ < total_size) {
      return true;
    }
  }

  uint32_t cursor_layers = cursor_layers_.size();
  for (uint32_t j = 0; j < cursor_layers; j++) {
    layers_.emplace_back(cursor_layers_.at(j));
  }

  bool success = physical_display_->Present(layers_, retire_fence, call_back,
                                            handle_constraints);
  std::vector<HwcLayer*>().swap(cursor_layers_);
  std::vector<HwcLayer*>().swap(layers_);
  queued_displays_ = 0;
  return success;
}

void LogicalDisplayManager::VSyncCallback(int64_t timestamp) {
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    displays_.at(i)->VSyncUpdate(timestamp);
  }
}

void LogicalDisplayManager::RefreshCallback() {
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    displays_.at(i)->RefreshUpdate();
  }
}

void LogicalDisplayManager::HotPlugCallback(bool connected) {
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    displays_.at(i)->HotPlugUpdate(connected);
  }
}

void LogicalDisplayManager::GetLogicalDisplays(
    std::vector<LogicalDisplay*>& displays) {
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    displays.emplace_back(displays_.at(i).get());
  }
}

void LogicalDisplayManager::SetHDCPState(HWCContentProtection state,
                                         HWCContentType content_type) {
  uint32_t size = displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    displays_.at(i)->SetHDCPState(state, content_type);
  }
}

void LogicalDisplayManager::SetHDCPSRM(const int8_t* SRM, uint32_t SRMLength) {
  physical_display_->SetHDCPSRM(SRM, SRMLength);
}

bool LogicalDisplayManager::ContainConnector(const uint32_t connector_id) {
  return physical_display_->ContainConnector(connector_id);
}

}  // namespace hwcomposer
