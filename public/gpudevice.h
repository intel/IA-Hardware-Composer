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

#ifndef PUBLIC_GPUDEVICE_H_
#define PUBLIC_GPUDEVICE_H_

#include <stdint.h>
#include <fstream>
#include <string>
#include <sstream>

#include "displaymanager.h"
#include "logicaldisplaymanager.h"
#include "nativedisplay.h"
#include "hwcthread.h"

namespace hwcomposer {

class NativeDisplay;

class GpuDevice : public HWCThread {
 public:
  GpuDevice();

  virtual ~GpuDevice();

  // Open device.
  bool Initialize();

  NativeDisplay* GetDisplay(uint32_t display);

  NativeDisplay* GetVirtualDisplay();

  // This display can be a client preparing
  // content which will eventually shown by
  // another parent display.
  NativeDisplay* GetNestedDisplay();

  void GetConnectedPhysicalDisplays(std::vector<NativeDisplay*>& displays);

  std::vector<NativeDisplay*> GetAllDisplays();

  void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback);

 private:
  enum InitializationType {
    kUnInitialized = 0,               // Nothing Initialized.
    kHWCSettingsInProgress = 1 << 0,  // Reading HWC Settings is in progress.
    kHWCSettingsDone = 1 << 1,        // Reading HWC Settings is done.
    kInitializeHotPlugMonitor =
        1 << 2,  // Initialize resources to monitor for Hotplug events.
    kInitializedHotPlugMonitor =
        1 << 3,  // Initialized resources to monitor for Hotplug events.
    kInitialized = 1 << 4  // Everything Initialized
  };

  void HandleHWCSettings();
  void InitializeHotPlugEvents(bool take_lock = true);
  void DisableWatch();
  void HandleRoutine() override;
  void HandleWait() override;
  std::unique_ptr<DisplayManager> display_manager_;
  std::vector<std::unique_ptr<LogicalDisplayManager>> logical_display_manager_;
  std::vector<std::unique_ptr<NativeDisplay>> mosaic_displays_;
  std::vector<NativeDisplay*> total_displays_;
  uint32_t initialization_state_ = kUnInitialized;
  SpinLock initialization_state_lock_;
  SpinLock thread_stage_lock_;
  SpinLock thread_sync_lock_;
  int lock_fd_ = -1;
  friend class DrmDisplayManager;
};

}  // namespace hwcomposer
#endif  // PUBLIC_GPUDEVICE_H_
