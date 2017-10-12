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

  // Handle config file reading
  const char *hwc_dp_cfg_path = std::getenv("HWC_DISPLAY_CONFIG");
  if (!hwc_dp_cfg_path) {
    hwc_dp_cfg_path = "/etc/hwc_display.ini";
  }

  bool use_logical = false;
  bool use_mosiac = false;
  std::vector<uint32_t> logical_displays;
  std::vector<std::vector<uint32_t>> mosiac_displays;
  std::ifstream fin(hwc_dp_cfg_path);
  std::string cfg_line;
  std::string key_logical("LOGICAL");
  std::string key_mosiac("MOSIAC");
  std::string key_logical_display("LOGICAL_DISPLAY");
  std::string key_mosiac_display("MOSIAC_DISPLAY");
  std::vector<uint32_t> mosiac_duplicate_check;
  while (std::getline(fin, cfg_line)) {
    std::istringstream i_line(cfg_line);
    std::string key;
    // Skip comments
    if (cfg_line[0] != '#' && std::getline(i_line, key, '=')) {
      std::string content;
      std::string value;
      std::getline(i_line, content, '=');
      std::istringstream i_content(content);
      while (std::getline(i_content, value, '"')) {
        if (value.empty())
          continue;

        std::string enable_str("true");
        // Got logical switch
        if (!key.compare(key_logical)) {
          if (!value.compare(enable_str)) {
            use_logical = true;
          }
          // Got mosiac switch
        } else if (!key.compare(key_mosiac)) {
          if (!value.compare(enable_str)) {
            use_mosiac = true;
          }
          // Got logical config
        } else if (!key.compare(key_logical_display)) {
          std::string physical_index_str;
          std::istringstream i_value(value);
          // Got physical display index
          std::getline(i_value, physical_index_str, ':');
          if (physical_index_str.empty() ||
              physical_index_str.find_first_not_of("0123456789") !=
                  std::string::npos)
            continue;
          std::string logical_split_str;
          // Got split num
          std::getline(i_value, logical_split_str, ':');
          if (logical_split_str.empty() ||
              logical_split_str.find_first_not_of("0123456789") !=
                  std::string::npos)
            continue;
          uint32_t physical_index = atoi(physical_index_str.c_str());
          uint32_t logical_split_num = atoi(logical_split_str.c_str());
          if (logical_split_num <= 1)
            continue;
          // Set logical num 1 for physical display which is not in config
          while (physical_index > logical_displays.size()) {
            logical_displays.emplace_back(1);
          }
          // Save logical split num for the physical display (don't care if the
          // physical display is disconnected/connected here)
          logical_displays.emplace_back(logical_split_num);
          // Got mosiac config
        } else if (!key.compare(key_mosiac_display)) {
          std::istringstream i_value(value);
          std::string i_mosiac_split_str;
          // Got mosiac sub display num
          std::vector<uint32_t> mosiac_display;
          while (std::getline(i_value, i_mosiac_split_str, '+')) {
            if (i_mosiac_split_str.empty() ||
                i_mosiac_split_str.find_first_not_of("0123456789") !=
                    std::string::npos)
              continue;
            size_t i_mosiac_split_num = atoi(i_mosiac_split_str.c_str());
            // Check and skip if the display already been used in other mosiac
            bool skip_duplicate_display = false;
            for (size_t i = 0; i < mosiac_duplicate_check.size(); i++) {
              if (mosiac_duplicate_check.at(i) == i_mosiac_split_num) {
                skip_duplicate_display = true;
              };
            };
            if (!skip_duplicate_display) {
              // save the sub display num for the mosiac display (don't care if
              // the physical/logical display is existing/connected here)
              mosiac_display.emplace_back(i_mosiac_split_num);
              mosiac_duplicate_check.push_back(i_mosiac_split_num);
            };
          };
          mosiac_displays.emplace_back(mosiac_display);
        }
      }
    }
  };

  std::vector<NativeDisplay *> temp_displays;
  for (size_t i = 0; i < size; i++) {
    hwcomposer::NativeDisplay *display = displays.at(i);
    // Save logical displays to temp_displays, skip the physical display
    if (use_logical && (logical_displays.size() >= i + 1) &&
        logical_displays[i] > 1) {
      std::unique_ptr<LogicalDisplayManager> manager(
          new LogicalDisplayManager(display));
      logical_display_manager_.emplace_back(std::move(manager));
      // don't care if the displays is connected/disconnected here
      logical_display_manager_.back()->InitializeLogicalDisplays(
          logical_displays[i]);
      std::vector<LogicalDisplay *> temp_logical_displays;
      logical_display_manager_.back()->GetLogicalDisplays(
          temp_logical_displays);
      size_t logical_display_total_size = temp_logical_displays.size();
      for (size_t j = 0; j < logical_display_total_size; j++) {
        temp_displays.emplace_back(temp_logical_displays.at(j));
      }
      // Save no split physical displays to temp_displays
    } else {
      temp_displays.emplace_back(display);
    }
  }

  std::vector<bool> available_displays(temp_displays.size(), true);
  if (use_mosiac) {
    for (size_t t = 0; t < temp_displays.size(); t++) {
      // Skip the displays which already be marked in other mosiac
      if (!available_displays.at(t))
        continue;
      bool skip_display = false;
      for (size_t m = 0; m < mosiac_displays.size(); m++) {
        std::vector<NativeDisplay *> i_available_mosiac_displays;
        for (size_t l = 0; l < mosiac_displays.at(m).size(); l++) {
          // Check if the logical display is in mosiac, keep the order of
          // logical displays list
          // Get the smallest logical num of the mosiac for order keeping
          if (t == mosiac_displays.at(m).at(l)) {
            // Loop to get logical displays of mosiac, keep the order of config
            for (size_t i = 0; i < mosiac_displays.at(m).size(); i++) {
              // Verify the logical display num
              if (mosiac_displays.at(m).at(i) < temp_displays.size()) {
                // Skip the disconnected display here
                i_available_mosiac_displays.emplace_back(
                    temp_displays.at(mosiac_displays.at(m).at(i)));
                // Add tag for mosiac-ed displays
                available_displays.at(mosiac_displays.at(m).at(i)) = false;
              }
            }
            // Create mosiac for those logical displays
            if (i_available_mosiac_displays.size() > 0) {
              std::unique_ptr<MosiacDisplay> mosiac(
                  new MosiacDisplay(i_available_mosiac_displays));
              mosiac_displays_.emplace_back(std::move(mosiac));
              // Save the mosiac to the final displays list
              total_displays_.emplace_back(mosiac_displays_.back().get());
            }
            skip_display = true;
            break;
          }
        }
        if (skip_display)
          break;
      }
      if (!skip_display) {
        total_displays_.emplace_back(temp_displays.at(t));
      }
    }
  } else {
    total_displays_.swap(temp_displays);
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
