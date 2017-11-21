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

#ifndef WSI_DRM_DISPLAY_MANAGER_H_
#define WSI_DRM_DISPLAY_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "spinlock.h"

#include "displayplanemanager.h"
#include "displaymanager.h"
#include "drmdisplay.h"
#include "drmscopedtypes.h"
#include "hwcthread.h"
#include "hwclock.h"
#include "vblankeventhandler.h"
#include "virtualdisplay.h"
#include "nesteddisplay.h"

namespace hwcomposer {

#define DRM_HOTPLUG_EVENT_SIZE 256

class NativeDisplay;

class DrmDisplayManager : public HWCThread, public DisplayManager {
 public:
  DrmDisplayManager();
  ~DrmDisplayManager() override;

  bool Initialize() override;

  NativeDisplay *GetVirtualDisplay() override;
  NativeDisplay *GetNestedDisplay() override;

  std::vector<NativeDisplay *> GetAllDisplays() override;

  void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback) override;

  void ForceRefresh();

  void NotifyClientsOfDisplayChangeStatus();

  void HandleLazyInitialization();

 protected:
  void HandleWait() override;
  void HandleRoutine() override;

 private:
  void HotPlugEventHandler();
  bool UpdateDisplayState();
  std::unique_ptr<NativeDisplay> virtual_display_;
  std::unique_ptr<NativeDisplay> nested_display_;
  std::unique_ptr<HWCLock> hwc_lock_;
  std::vector<std::unique_ptr<DrmDisplay>> displays_;
  std::shared_ptr<DisplayHotPlugEventCallback> callback_ = NULL;
  std::unique_ptr<NativeBufferHandler> buffer_handler_;
  int fd_ = -1;
  int hotplug_fd_ = -1;
  bool notify_client_ = false;
  bool release_lock_ = false;
  SpinLock spin_lock_;
};

}  // namespace hwcomposer
#endif  // WSI_DRM_DISPLAY_MANAGER_H_
