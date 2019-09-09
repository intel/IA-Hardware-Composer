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

#ifndef HWC_LOCK_FILE
#define HWC_LOCK_FILE "/vendor/hwc.lock"
#endif

#include <stdint.h>
#include <fstream>
#include <sstream>
#include <string>

#include "displaymanager.h"
#include "framebuffermanager.h"
#include "hwcthread.h"
#include "logicaldisplaymanager.h"
#include "nativedisplay.h"

namespace hwcomposer {

#ifdef ENABLE_PANORAMA
class MosaicDisplay;
#endif
class NativeDisplay;

class GpuDevice : public HWCThread {
 public:
  static GpuDevice& getInstance();

 public:
  virtual ~GpuDevice();

  // Open device.
  bool Initialize();

  FrameBufferManager* GetFrameBufferManager();

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
  void EnableHDCPSessionForDisplay(uint32_t connector,
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
  void DisableHDCPSessionForDisplay(uint32_t connector);

  // The control disables the usage of HDCP for all planes supporting this
  // feature on all connected displays.
  void DisableHDCPSessionForAllDisplays();

  void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                            uint32_t pavp_instance_id);
  void SetHDCPSRMForAllDisplays(const int8_t* SRM, uint32_t SRMLength);
  void SetHDCPSRMForDisplay(uint32_t connector, const int8_t* SRM,
                            uint32_t SRMLength);
  uint32_t GetDisplayIDFromConnectorID(const uint32_t connector_id);
#ifdef ENABLE_PANORAMA
  bool TriggerPanorama(uint32_t hotplug_simulation);
  bool ShutdownPanorama(uint32_t hotplug_simulation);
#endif

  bool IsReservedDrmPlane();

  bool EnableDRMCommit(bool enable, uint32_t display_id);

  bool ResetDrmMaster(bool drop_master);

  bool IsDrmMaster();

  std::vector<uint32_t> GetDisplayReservedPlanes(uint32_t display_id);

 private:
  GpuDevice();

  void ResetAllDisplayCommit(bool enable);

  void MarkDisplayForFirstCommit();

  enum InitializationType {
    kUnInitialized = 0,    // Nothing Initialized.
    kInitialized = 1 << 1  // Everything Initialized
  };

  void HandleHWCSettings();
  void DisableWatch();
  void HandleRoutine() override;
  void HandleWait() override;
  void ParsePlaneReserveSettings(std::string& value);
  std::unique_ptr<DisplayManager> display_manager_;
  std::vector<std::unique_ptr<LogicalDisplayManager>> logical_display_manager_;
  std::vector<std::unique_ptr<NativeDisplay>> mosaic_displays_;
#ifdef ENABLE_PANORAMA
  std::vector<std::unique_ptr<NativeDisplay>> panorama_displays_;
  void ParsePanoramaDisplayConfig(
      std::string& value,
      std::vector<std::vector<uint32_t>>& panorama_displays);
  void ParsePanoramaSOSDisplayConfig(
      std::string& value,
      std::vector<std::vector<uint32_t>>& panorama_sos_displays);
  void InitializePanorama(
      std::vector<NativeDisplay*>& total_displays_,
      std::vector<NativeDisplay*>& temp_displays,
      std::vector<std::vector<uint32_t>>& panorama_displays,
      std::vector<std::vector<uint32_t>>& panorama_sos_displays,
      std::vector<bool>& available_displays);

  std::vector<NativeDisplay*> virtual_panorama_displays_;
  std::vector<NativeDisplay*> physical_panorama_displays_;
  MosaicDisplay* ptr_mosaicdisplay = NULL;
#endif
  void ParseLogicalDisplaySetting(std::string& value,
                                  std::vector<uint32_t>& logical_displays);
  void ParseMosaicDisplaySetting(
      std::string& value, std::vector<std::vector<uint32_t>>& mosaic_displays);
  void ParsePhysicalDisplaySetting(std::string& value,
                                   std::vector<uint32_t>& physical_displays);
  void ParseCloneDisplaySetting(
      std::string& value, std::vector<std::vector<uint32_t>>& cloned_displays);
  void ParsePhysicalDisplayRotation(
      std::string& value, std::vector<uint32_t>& display_rotation,
      std::vector<uint32_t>& rotation_display_index);
  void ParseFloatDisplaySetting(std::string& value,
                                std::vector<HwcRect<int32_t>>& float_displays,
                                std::vector<uint32_t>& float_display_indices);

  void InitializeDisplayIndex(std::vector<uint32_t>& physical_displays,
                              std::vector<NativeDisplay*>& displays);
  void InitializeLogicalDisplay(std::vector<uint32_t>& logical_displays,
                                std::vector<NativeDisplay*>& displays,
                                std::vector<NativeDisplay*>& temp_displays,
                                bool use_logical);
  void InitializeDisplayRotation(std::vector<uint32_t>& display_rotation,
                                 std::vector<uint32_t>& rotation_display_index,
                                 std::vector<NativeDisplay*>& displays);
  void InitializeMosaicDisplay(
      std::vector<NativeDisplay*>& total_displays_,
      std::vector<std::vector<uint32_t>>& mosaic_displays,
      std::vector<NativeDisplay*>& temp_displays,
      std::vector<bool>& available_displays);
  void InitializeCloneDisplay(
      std::vector<NativeDisplay*>& total_displays_,
      std::vector<std::vector<uint32_t>>& cloned_displays);
  void InitializeFloatDisplay(std::vector<NativeDisplay*>& total_displays_,
                              std::vector<HwcRect<int32_t>>& float_displays,
                              std::vector<uint32_t>& float_display_indices);
  std::vector<NativeDisplay*> total_displays_;

  bool reserve_plane_ = false;
  bool enable_all_display_ = false;
  std::map<uint8_t, std::vector<uint32_t>> reserved_drm_display_planes_map_;
  uint32_t initialization_state_ = kUnInitialized;
  SpinLock initialization_state_lock_;
  SpinLock drm_master_lock_;
  int lock_fd_ = -1;
  friend class DrmDisplayManager;
};

}  // namespace hwcomposer
#endif  // PUBLIC_GPUDEVICE_H_
