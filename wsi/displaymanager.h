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

#ifndef WSI_MANAGER_H_
#define WSI_MANAGER_H_

#include <stdint.h>

#include "nativedisplay.h"

namespace hwcomposer {

class GpuDevice;
class DisplayManager {
 public:
  static DisplayManager *CreateDisplayManager(GpuDevice *device);
  DisplayManager() = default;
  virtual ~DisplayManager() {
  }

  // Initialize things which are critical for
  // Display Manager. InitializeDisplayResources
  // is expected to be called to handle things
  // which can be initialized later to finish
  // the initialization.
  virtual bool Initialize() = 0;

  // GetAllDisplays is expected to return set
  // of correct displays after this call is done.
  virtual void InitializeDisplayResources() = 0;

  // Display Manager should initialize resources to start monitoring
  // for Hotplug events.
  virtual void StartHotPlugMonitor() = 0;

  // Refresh all displays managed by this display manager.
  virtual void ForceRefresh() = 0;

  // Ignore updates for all displays managed by this display
  // manager until ForceRefresh is called.
  virtual void IgnoreUpdates() = 0;

  // Get FD associated with this DisplayManager.
  virtual uint32_t GetFD() const = 0;

  virtual NativeDisplay *CreateVirtualDisplay(uint32_t display_index) = 0;
  virtual void DestroyVirtualDisplay(uint32_t display_index) = 0;

  virtual std::vector<NativeDisplay *> GetAllDisplays() = 0;

  virtual void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback) = 0;

  virtual uint32_t GetConnectedPhysicalDisplayCount() = 0;
};

}  // namespace hwcomposer
#endif  // WSI_MANAGER_H_
