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

#ifndef WSI_LOGICALDISPLAY_MANAGER_H_
#define WSI_LOGICALDISPLAY_MANAGER_H_

#include <stdlib.h>
#include <stdint.h>

#include <nativedisplay.h>
#include "logicaldisplay.h"

namespace hwcomposer {

class LogicalDisplayManager;

class LogicalDisplayManager {
 public:
  LogicalDisplayManager(NativeDisplay* physical_display);
  ~LogicalDisplayManager();

  void InitializeLogicalDisplays(uint32_t total);
  void UpdatePowerMode();
  void UpdateVSyncControl();
  void RegisterHotPlugNotification();

  bool Present(std::vector<HwcLayer*>& source_layers, int32_t* retire_fence,
               bool handle_constraints);

  void VSyncCallback(int64_t timestamp);

  void RefreshCallback();

  void HotPlugCallback(bool connected);

  NativeDisplay* GetPhysicalDisplay() const {
    return physical_display_;
  }

  void GetLogicalDisplays(std::vector<LogicalDisplay*>& displays);

  void SetHDCPState(HWCContentProtection state);

 private:
  NativeDisplay* physical_display_;
  std::vector<std::unique_ptr<LogicalDisplay>> displays_;
  std::vector<HwcLayer*> layers_;
  std::vector<HwcLayer*> cursor_layers_;
  uint32_t queued_displays_ = 0;
  bool hot_plug_registered_ = false;
  bool handle_hoplug_notifications_ = false;
};

}  // namespace hwcomposer
#endif  // WSI_LOGICALDISPLAY_MANAGER_H_
