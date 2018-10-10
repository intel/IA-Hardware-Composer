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
#include <sstream>
#include <string>

#include "displaymanager.h"
#include "hwcthread.h"
#include "logicaldisplaymanager.h"
#include "nativedisplay.h"

namespace hwcomposer {

class NativeDisplay;

class GpuDevice : public HWCThread {
 public:
  GpuDevice();

  virtual ~GpuDevice();

  // Open device.
  bool Initialize();

  uint32_t GetFD() const;

  NativeDisplay* GetDisplay(uint32_t display);

  NativeDisplay* CreateVirtualDisplay(uint32_t display_index);
  void DestroyVirtualDisplay(uint32_t display_index);

  // This display can be a client preparing
  // content which will eventually shown by
  // another parent display.

  void GetConnectedPhysicalDisplays(std::vector<NativeDisplay*>& displays);

  const std::vector<NativeDisplay*>& GetAllDisplays();

  void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback);

  // Enables the usage of HDCP for all planes supporting this feature
  // on display. Some displays can support latest HDCP specification and also
  // have ability to fallback to older specifications i.e. HDCP 2.2 and 1.4
  // in case latest speicification cannot be supported for some reason.
  // Content type is defined by content_type.
  void EnableHDCPSessionForDisplay(uint32_t display,
                                   HWCContentType content_type);

  // Enables the usage of HDCP for all planes supporting this feature
  // on all connected displays. Some displays can support latest HDCP
  // specification and also have ability to fallback to older
  // specifications i.e. HDCP 2.2 and 1.4 in case latest speicification
  // cannot be supported for some reason. Content type is defined by
  // content_type.
  void EnableHDCPSessionForAllDisplays(HWCContentType content_type);

  // The control disables the usage of HDCP for all planes supporting this
  // feature on display.
  void DisableHDCPSessionForDisplay(uint32_t display);

  // The control disables the usage of HDCP for all planes supporting this
  // feature on all connected displays.
  void DisableHDCPSessionForAllDisplays();

  void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                            uint32_t pavp_instance_id);
  void SetHDCPSRMForAllDisplays(const int8_t* SRM, uint32_t SRMLength);
  void SetHDCPSRMForDisplay(uint32_t display, const int8_t* SRM,
                            uint32_t SRMLength);

 private:
  enum InitializationType {
    kUnInitialized = 0,    // Nothing Initialized.
    kInitialized = 1 << 1  // Everything Initialized
  };

  void HandleHWCSettings();
  void DisableWatch();
  void HandleRoutine() override;
  void HandleWait() override;
  std::unique_ptr<DisplayManager> display_manager_;
  std::vector<std::unique_ptr<LogicalDisplayManager>> logical_display_manager_;
  std::vector<std::unique_ptr<NativeDisplay>> mosaic_displays_;
  std::vector<NativeDisplay*> total_displays_;
  uint32_t initialization_state_ = kUnInitialized;
  SpinLock initialization_state_lock_;
  int lock_fd_ = -1;
  friend class DrmDisplayManager;
};

}  // namespace hwcomposer
#endif  // PUBLIC_GPUDEVICE_H_
