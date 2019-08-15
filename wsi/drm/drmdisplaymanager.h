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

#include "displaymanager.h"
#include "displayplanemanager.h"
#include "drmdisplay.h"
#include "drmscopedtypes.h"
#include "framebuffermanager.h"
#include "gpudevice.h"
#include "hwcthread.h"
#include "vblankeventhandler.h"
#include "virtualdisplay.h"
#ifdef ENABLE_PANORAMA
#include "virtualpanoramadisplay.h"
#endif

namespace hwcomposer {

#define DRM_HOTPLUG_EVENT_SIZE 256

class NativeDisplay;

class DrmDisplayManager : public HWCThread, public DisplayManager {
 public:
  DrmDisplayManager();
  ~DrmDisplayManager() override;

  bool Initialize() override;

  void InitializeDisplayResources() override;

  void StartHotPlugMonitor() override;

  NativeDisplay *CreateVirtualDisplay(uint32_t display_index) override;
  void DestroyVirtualDisplay(uint32_t display_index) override;

#ifdef ENABLE_PANORAMA
  NativeDisplay *CreateVirtualPanoramaDisplay(uint32_t display_index) override;
#endif

  std::vector<NativeDisplay *> GetAllDisplays() override;

  void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback) override;

  void ForceRefresh() override;

  void IgnoreUpdates() override;

  void setDrmMaster(bool must_set) override;

  void DropDrmMaster() override;

  bool IsDrmMaster() override {
    return drm_master_;
  }

  uint32_t GetFD() const override {
    return fd_;
  }

  void NotifyClientsOfDisplayChangeStatus();

  void HandleLazyInitialization();

  uint32_t GetConnectedPhysicalDisplayCount();

  void EnableHDCPSessionForDisplay(uint32_t connector,
                                   HWCContentType content_type) override;
  void EnableHDCPSessionForAllDisplays(HWCContentType content_type) override;
  void DisableHDCPSessionForDisplay(uint32_t connector) override;
  void DisableHDCPSessionForAllDisplays() override;
  void SetHDCPSRMForAllDisplays(const int8_t *SRM, uint32_t SRMLength) override;
  void SetHDCPSRMForDisplay(uint32_t connector, const int8_t *SRM,
                            uint32_t SRMLength) override;
  void RemoveUnreservedPlanes() override;

  FrameBufferManager *GetFrameBufferManager() override;

 protected:
  void HandleWait() override;
  void HandleRoutine() override;

 private:
  void HotPlugEventHandler();
  bool UpdateDisplayState();
  std::map<uint32_t, std::unique_ptr<NativeDisplay>> virtual_displays_;
  std::unique_ptr<FrameBufferManager> frame_buffer_manager_;
  std::vector<std::unique_ptr<DrmDisplay>> displays_;
  std::shared_ptr<DisplayHotPlugEventCallback> callback_ = NULL;
  std::unique_ptr<NativeBufferHandler> buffer_handler_;
  GpuDevice &device_ = GpuDevice::getInstance();
  bool ignore_updates_ = false;
  int fd_ = -1;
  int hotplug_fd_ = -1;
  bool notify_client_ = false;
  bool release_lock_ = false;
  SpinLock spin_lock_;
  int connected_display_count_ = 0;
  bool drm_master_ = false;
};

}  // namespace hwcomposer
#endif  // WSI_DRM_DISPLAY_MANAGER_H_
