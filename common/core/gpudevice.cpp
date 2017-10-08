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

#include <gpudevice.h>

#include "mosiacdisplay.h"

namespace hwcomposer {

GpuDevice::GpuDevice() : initialized_(false) {
}

GpuDevice::~GpuDevice() {
  display_manager_.reset(nullptr);
}

bool GpuDevice::Initialize() {
  if (initialized_)
    return true;

  initialized_ = true;
  display_manager_.reset(DisplayManager::CreateDisplayManager());

  bool success = display_manager_->Initialize();
  if (!success) {
    return false;
  }

  std::vector<NativeDisplay *> displays = display_manager_->GetAllDisplays();
  size_t size = displays.size();
  NativeDisplay *primary_display = NULL;
  for (size_t i = 0; i < size; i++) {
    hwcomposer::NativeDisplay *display = displays.at(i);
    if (display->IsConnected()) {
      primary_display = display;
      break;
    }
  }

  if (!primary_display) {
    primary_display = displays.at(0);
  }

  // TODO: How do we determine when to use Logical Manager ?
  bool use_logical = true;
  bool use_mosiac = true;
  std::vector<NativeDisplay *> physical_displays;
  std::vector<std::vector<NativeDisplay *>> mdisplays;

  if (use_logical) {
    // TODO: Map required physical display here instead of primary.
    std::unique_ptr<LogicalDisplayManager> manager(
        new LogicalDisplayManager(primary_display));
    logical_display_manager_.emplace_back(std::move(manager));
    // We Assume Primary is logically split for now. We should have a way to
    // determine the physical display and no of logical displays to split.
    logical_display_manager_.back()->InitializeLogicalDisplays(2);
  }

  // We have logical displays setup, let's find the
  // physical displays which are still not mapped to
  // Logical Displays.
  uint32_t ldmsize = logical_display_manager_.size();
  for (size_t i = 0; i < size; i++) {
    hwcomposer::NativeDisplay *display = displays.at(i);
    bool skip_display = false;
    for (size_t i = 0; i < ldmsize; ++i) {
      if (logical_display_manager_.at(i)->GetPhysicalDisplay() == display) {
        skip_display = true;
        std::vector<LogicalDisplay *> displays;
        logical_display_manager_.at(i)->GetLogicalDisplays(displays);
        size_t total_size = displays.size();
        for (size_t i = 0; i < total_size; i++) {
          physical_displays.emplace_back(displays.at(i));
        }
        break;
      }
    }

    if (skip_display) {
      continue;
    }

    physical_displays.emplace_back(display);
  }

  int32_t least_mosiac_index = -1;

  if (use_mosiac) {
    int32_t l_size = physical_displays.size();
    mdisplays.emplace_back();
    std::vector<NativeDisplay *> &temp = mdisplays.back();
    for (int32_t i = 0; i < l_size; i++) {
      hwcomposer::NativeDisplay *display = physical_displays.at(i);
      temp.emplace_back(display);

      if (least_mosiac_index == -1) {
        least_mosiac_index = i;
      } else {
        least_mosiac_index = std::min(least_mosiac_index, i);
      }
    }

    std::unique_ptr<MosiacDisplay> mosiac(new MosiacDisplay(temp));
    mosiac_displays_.emplace_back(std::move(mosiac));
  }

  if (least_mosiac_index == 0) {
    total_displays_.emplace_back(mosiac_displays_.at(0).get());
  } else {
    if (least_mosiac_index == -1) {
      least_mosiac_index = physical_displays.size();
    }

    for (int32_t i = 0; i < least_mosiac_index; i++) {
      total_displays_.emplace_back(physical_displays.at(i));
    }
  }

  if (least_mosiac_index == static_cast<int32_t>(physical_displays.size())) {
    return true;
  }

  uint32_t m_size = mdisplays.size();
  uint32_t mosiac_index_added = least_mosiac_index;
  least_mosiac_index++;
  for (size_t i = least_mosiac_index; i < size; i++) {
    hwcomposer::NativeDisplay *display = physical_displays.at(i);
    bool skip_display = false;
    for (size_t i = 0; i < m_size; ++i) {
      std::vector<NativeDisplay *> &temp_display = mdisplays.at(i);
      size_t temp = temp_display.size();
      for (size_t i = 0; i < temp; ++i) {
        if (temp_display.at(i) == display) {
          skip_display = true;
          if (mosiac_index_added < i) {
            total_displays_.emplace_back(mosiac_displays_.at(0).get());
            mosiac_index_added = i;
          }

          break;
        }
      }
    }

    if (skip_display) {
      continue;
    }

    total_displays_.emplace_back(display);
  }

  return true;
}

NativeDisplay *GpuDevice::GetDisplay(uint32_t display_id) {
  if (total_displays_.size() > display_id)
    return total_displays_.at(display_id);

  return NULL;
}

NativeDisplay *GpuDevice::GetVirtualDisplay() {
  return display_manager_->GetVirtualDisplay();
}

void GpuDevice::GetConnectedPhysicalDisplays(
    std::vector<NativeDisplay *> &displays) {
  size_t size = total_displays_.size();
  for (size_t i = 0; i < size; i++) {
    if (total_displays_.at(i)->IsConnected()) {
      displays.emplace_back(total_displays_.at(i));
    }
  }
}

std::vector<NativeDisplay *> GpuDevice::GetAllDisplays() {
  return total_displays_;
}

void GpuDevice::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  display_manager_->RegisterHotPlugEventCallback(callback);
}

}  // namespace hwcomposer
