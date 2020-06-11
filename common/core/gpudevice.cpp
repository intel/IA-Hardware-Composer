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

#include <sys/file.h>

#include "mosaicdisplay.h"

#include "hwctrace.h"
#include "hwcutils.h"

namespace hwcomposer {

GpuDevice::GpuDevice() : HWCThread(-8, "GpuDevice") {
}

GpuDevice::~GpuDevice() {
  display_manager_.reset(nullptr);
  HWCThread::Exit();

  if (-1 != lock_fd_) {
    close(lock_fd_);
    lock_fd_ = -1;
  }
}

void GpuDevice::ResetAllDisplayCommit(bool enable) {
  if (enable == enable_all_display_)
    return;
  enable_all_display_ = enable;
  size_t size = total_displays_.size();
  for (size_t i = 0; i < size; i++)
    total_displays_.at(i)->EnableDRMCommit(enable);
}

bool GpuDevice::Initialize() {
  initialization_state_lock_.lock();
  if (initialization_state_ & kInitialized) {
    initialization_state_lock_.unlock();
    return true;
  }

  initialization_state_ |= kInitialized;
  initialization_state_lock_.unlock();

  display_manager_.reset(DisplayManager::CreateDisplayManager());

  bool success = display_manager_->Initialize();
  if (!success) {
    return false;
  }

  display_manager_->InitializeDisplayResources();
  display_manager_->StartHotPlugMonitor();

  HandleHWCSettings();

  if (reserve_plane_) {
    display_manager_->RemoveUnreservedPlanes();
  }

  lock_fd_ = open(HWC_LOCK_FILE, O_RDONLY);
  if (-1 != lock_fd_) {
    if (!InitWorker()) {
      ETRACE("Failed to initalize thread for GpuDevice. %s", PRINTERROR());
    }
  } else {
    ETRACE("Failed to open %s", HWC_LOCK_FILE);
    // HWC should become drm master and start to commit.
    // if hwc.lock is not available
    if (!display_manager_->IsDrmMasterByDefault())
      display_manager_->setDrmMaster(true);
    ResetAllDisplayCommit(true);
  }

  return true;
}

FrameBufferManager *GpuDevice::GetFrameBufferManager() {
  return display_manager_->GetFrameBufferManager();
}

uint32_t GpuDevice::GetFD() const {
  return display_manager_->GetFD();
}

NativeDisplay *GpuDevice::GetDisplay(uint32_t display_id) {
  if (total_displays_.size() > display_id)
    return total_displays_.at(display_id);

  return NULL;
}

NativeDisplay *GpuDevice::CreateVirtualDisplay(uint32_t display_index) {
  return display_manager_->CreateVirtualDisplay(display_index);
}

void GpuDevice::DestroyVirtualDisplay(uint32_t display_index) {
  display_manager_->DestroyVirtualDisplay(display_index);
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

bool GpuDevice::EnableDRMCommit(bool enable, uint32_t display_id) {
  size_t size = total_displays_.size();
  bool ret = false;
  if (size > display_id)
    ret = total_displays_.at(display_id)->EnableDRMCommit(enable);
  return ret;
}

void GpuDevice::MarkDisplayForFirstCommit() {
  size_t size = total_displays_.size();
  for (size_t i = 0; i < size; i++) {
    if (total_displays_.at(i)->IsConnected()) {
      total_displays_.at(i)->MarkFirstCommit();
    }
  }
}

bool GpuDevice::ResetDrmMaster(bool drop_master) {
  bool ret = true;
  if (drop_master) {
    ResetAllDisplayCommit(false);
    display_manager_->DropDrmMaster();
    ITRACE("locking %s and monitoring if %s is unlocked.", HWC_LOCK_FILE,
           HWC_LOCK_FILE);
    // Resume GpuDevice thread to check hwc.lock and re-apply drm master.
    lock_fd_ = open(HWC_LOCK_FILE, O_RDONLY);
    if (-1 != lock_fd_) {
      // Only resume GpuDevice thread for dropping DRM Master and
      // the lock file exist.
      Resume();
      return !display_manager_->IsDrmMaster();
    }
  }
  // In case of setDrmMaster or the lock file is not exist.
  // Re-set DRM Master true.
  display_manager_->setDrmMaster(false);
  MarkDisplayForFirstCommit();
  ResetAllDisplayCommit(true);
  DisableWatch();

  if (drop_master)
    ret = !display_manager_->IsDrmMaster();
  else
    ret = display_manager_->IsDrmMaster();
  return ret;
}

bool GpuDevice::IsDrmMaster() {
  return display_manager_->IsDrmMaster();
}

const std::vector<NativeDisplay *> &GpuDevice::GetAllDisplays() {
  return total_displays_;
}

void GpuDevice::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  display_manager_->RegisterHotPlugEventCallback(callback);
}

bool GpuDevice::IsReservedDrmPlane() {
  return reserve_plane_;
}

std::vector<uint32_t> GpuDevice::GetDisplayReservedPlanes(uint32_t display_id) {
  std::map<uint8_t, std::vector<uint32_t>>::const_iterator pos =
      reserved_drm_display_planes_map_.find(display_id);
  if (pos == reserved_drm_display_planes_map_.end())
    return std::vector<uint32_t>();
  else
    return pos->second;
}

void GpuDevice::ParsePlaneReserveSettings(std::string &value) {
  std::string display_line_str;
  std::string reserved_plane_index_str;
  std::istringstream i_value(value);

  // Get each display setting
  while (std::getline(i_value, display_line_str, ';')) {
    if (display_line_str.empty() ||
        display_line_str.find_first_not_of("0123456789:+") != std::string::npos)
      continue;
    std::string display_index_str =
        display_line_str.substr(0, display_line_str.find(":"));
    uint8_t display_index = atoi(display_index_str.c_str());
    std::string tmp = display_line_str.substr(display_line_str.find(":") + 1);
    std::vector<uint32_t> reserved_drm_planes_;
    std::istringstream planes_index_value(
        display_line_str.substr(display_line_str.find(":") + 1));
    // Get reversed drm plane index
    while (std::getline(planes_index_value, reserved_plane_index_str, '+')) {
      if (reserved_plane_index_str.empty() ||
          reserved_plane_index_str.find_first_not_of("0123456789") !=
              std::string::npos)
        continue;
      uint32_t reserved_plane_index_num =
          atoi(reserved_plane_index_str.c_str());
      // Check if the plane index is duplicated
      bool skip_duplicate_plane = false;
      size_t reserved_plane_size = reserved_drm_planes_.size();
      for (size_t i = 0; i < reserved_plane_size; i++) {
        if (reserved_drm_planes_.at(i) == reserved_plane_index_num) {
          skip_duplicate_plane = true;
          break;
        }
      }
      if (skip_duplicate_plane) {
        continue;
      }
      IPLANERESERVEDTRACE(
          "Parsing configure for reserving display[%d], plane[%d]",
          display_index, reserved_plane_index_num);
      reserved_drm_planes_.emplace_back(reserved_plane_index_num);
    }
    reserved_drm_display_planes_map_[display_index] = reserved_drm_planes_;
  }
}

#ifdef ENABLE_PANORAMA
void GpuDevice::ParsePanoramaDisplayConfig(
    std::string &value, std::vector<std::vector<uint32_t>> &panorama_displays) {
  std::istringstream i_value(value);
  std::string i_panorama_split_str;
  std::vector<uint32_t> panorama_duplicate_check;
  // Got panorama sub display num
  std::vector<uint32_t> panorama_display;
  while (std::getline(i_value, i_panorama_split_str, '+')) {
    if (i_panorama_split_str.empty() ||
        i_panorama_split_str.find_first_not_of("0123456789") !=
            std::string::npos)
      continue;
    size_t i_panorama_split_num = atoi(i_panorama_split_str.c_str());
    // Check and skip if the display already been used in other panorama
    bool skip_duplicate_display = false;
    size_t panorama_size = panorama_duplicate_check.size();
    for (size_t i = 0; i < panorama_size; i++) {
      if (panorama_duplicate_check.at(i) == i_panorama_split_num) {
        skip_duplicate_display = true;
        break;
      }
    }
    if (!skip_duplicate_display) {
      // save the sub display num for the panorama display (don't care if
      // the physical/logical display is existing/connected here)
      panorama_display.emplace_back(i_panorama_split_num);
      panorama_duplicate_check.emplace_back(i_panorama_split_num);
    }
  }
  panorama_displays.emplace_back(panorama_display);
}

void GpuDevice::ParsePanoramaSOSDisplayConfig(
    std::string &value,
    std::vector<std::vector<uint32_t>> &panorama_sos_displays) {
  std::istringstream i_value(value);
  std::string i_panorama_sos_split_str;

  std::vector<uint32_t> panorama_sos_duplicate_check;
  // Got panorama sub display num
  std::vector<uint32_t> panorama_sos_display;
  while (std::getline(i_value, i_panorama_sos_split_str, '+')) {
    if (i_panorama_sos_split_str.empty() ||
        i_panorama_sos_split_str.find_first_not_of("0123456789") !=
            std::string::npos)
      continue;
    size_t i_panorama_sos_split_num = atoi(i_panorama_sos_split_str.c_str());
    // Check and skip if the display already been used in other panorama
    bool skip_duplicate_display = false;
    size_t panorama_sos_size = panorama_sos_duplicate_check.size();
    for (size_t i = 0; i < panorama_sos_size; i++) {
      if (panorama_sos_duplicate_check.at(i) == i_panorama_sos_split_num) {
        skip_duplicate_display = true;
        break;
      }
    }
    if (!skip_duplicate_display) {
      // save the sub display num for the panorama display (don't care if
      // the physical/logical display is existing/connected here)
      panorama_sos_display.emplace_back(i_panorama_sos_split_num);
      panorama_sos_duplicate_check.emplace_back(i_panorama_sos_split_num);
    }
  }
  panorama_sos_displays.emplace_back(panorama_sos_display);
}

void GpuDevice::InitializePanorama(
    std::vector<NativeDisplay *> &total_displays_,
    std::vector<NativeDisplay *> &temp_displays,
    std::vector<std::vector<uint32_t>> &panorama_displays,
    std::vector<std::vector<uint32_t>> &panorama_sos_displays,
    std::vector<bool> &available_displays) {
  std::vector<NativeDisplay *> i_available_panorama_displays;
  temp_displays.swap(total_displays_);
  // Add the virtual panorama displays mapping the SOS virtual displays.
  size_t sos_displays_size = panorama_sos_displays.size();
  for (size_t sos_it = 0; sos_it < sos_displays_size; sos_it++) {
    NativeDisplay *virtualdisp = display_manager_->CreateVirtualPanoramaDisplay(
        panorama_sos_displays.at(0).at(sos_it));
    virtualdisp->InitVirtualDisplay(1920, 1080);
    i_available_panorama_displays.emplace_back(virtualdisp);
    virtual_panorama_displays_.emplace_back(virtualdisp);
  }

  // Add the native displays
  size_t displays_size = temp_displays.size();
  for (size_t t = 0; t < displays_size; t++) {
    // Skip the displays which already be marked in other panorama
    if (!available_displays.at(t)) {
      ETRACE("display: %u is not present in the vector of avaialble_displays");
      continue;
    }
    bool skip_display = false;
    size_t panorama_size = panorama_displays.size();
    for (size_t m = 0; m < panorama_size; m++) {
      size_t panorama_inner_size = panorama_displays.at(m).size();
      for (size_t l = 0; l < panorama_inner_size; l++) {
        // Check if the logical display is in panorama, keep the order of
        // logical displays list
        // Get the smallest logical num of the panorama for order keeping
        if (t == panorama_displays.at(m).at(l)) {
          if (panorama_displays.at(m).at(l) < displays_size) {
            // Skip the disconnected display here
            i_available_panorama_displays.emplace_back(
                temp_displays.at(panorama_displays.at(m).at(l)));
            physical_panorama_displays_.emplace_back(
                temp_displays.at(panorama_displays.at(m).at(l)));
            // Add tag for panorama-ed displays
            available_displays.at(panorama_displays.at(m).at(l)) = false;
          }
          skip_display = true;
          break;
        }
      }
      if (skip_display)
        break;
    }
  }
  // Create panorama for those logical displays
  if (i_available_panorama_displays.size() > 0) {
    std::unique_ptr<MosaicDisplay> panorama(
        new MosaicDisplay(i_available_panorama_displays));
    panorama->SetPanoramaMode(true);
    panorama->SetExtraDispInfo(&virtual_panorama_displays_,
                               &physical_panorama_displays_);
    ptr_mosaicdisplay = (MosaicDisplay *)panorama.get();
    panorama_displays_.emplace_back(std::move(panorama));
    // Save the panorama to the final displays list
    total_displays_.emplace_back(panorama_displays_.back().get());
  }
}

bool GpuDevice::TriggerPanorama(uint32_t hotplug_simulation) {
  bool ret = false;
  if (ptr_mosaicdisplay) {
    ret = ptr_mosaicdisplay->TriggerPanorama(hotplug_simulation);
  }
  return ret;
}

bool GpuDevice::ShutdownPanorama(uint32_t hotplug_simulation) {
  bool ret = false;
  if (ptr_mosaicdisplay) {
    ret = ptr_mosaicdisplay->ShutdownPanorama(hotplug_simulation);
  }
  return ret;
}

#endif

void GpuDevice::ParseLogicalDisplaySetting(
    std::string &value, std::vector<uint32_t> &logical_displays) {
  std::istringstream i_value(value);
  std::string physical_index_str;
  std::getline(i_value, physical_index_str, ':');
  if (physical_index_str.empty() ||
      physical_index_str.find_first_not_of("0123456789") != std::string::npos)
    return;
  std::string logical_split_str;
  // Got split num
  std::getline(i_value, logical_split_str, ':');
  if (logical_split_str.empty() ||
      logical_split_str.find_first_not_of("0123456789") != std::string::npos)
    return;
  if (physical_index_str.length() > 1)
    return;
  uint32_t physical_index = atoi(physical_index_str.c_str());
  uint32_t logical_split_num = atoi(logical_split_str.c_str());
  if (logical_split_num <= 1)
    return;
  // Set logical num 1 for physical display which is not in config
  while (physical_index > logical_displays.size()) {
    logical_displays.emplace_back(1);
  }
  // Save logical split num for the physical display (don't care if the
  // physical display is disconnected/connected here)
  logical_displays.emplace_back(logical_split_num);
  // Got mosaic config
}

void GpuDevice::ParseMosaicDisplaySetting(
    std::string &value, std::vector<std::vector<uint32_t>> &mosaic_displays) {
  std::istringstream i_value(value);
  std::string i_mosaic_split_str;
  std::vector<uint32_t> mosaic_duplicate_check;

  // Got mosaic sub display num
  std::vector<uint32_t> mosaic_display;
  while (std::getline(i_value, i_mosaic_split_str, '+')) {
    if (i_mosaic_split_str.empty() ||
        i_mosaic_split_str.find_first_not_of("0123456789") != std::string::npos)
      continue;
    size_t i_mosaic_split_num = atoi(i_mosaic_split_str.c_str());
    // Check and skip if the display already been used in other mosaic
    bool skip_duplicate_display = false;
    size_t mosaic_size = mosaic_duplicate_check.size();
    for (size_t i = 0; i < mosaic_size; i++) {
      if (mosaic_duplicate_check.at(i) == i_mosaic_split_num) {
        skip_duplicate_display = true;
        break;
      }
    }
    if (!skip_duplicate_display) {
      // save the sub display num for the mosaic display (don't care
      // if
      // the physical/logical display is existing/connected here)
      mosaic_display.emplace_back(i_mosaic_split_num);
      mosaic_duplicate_check.emplace_back(i_mosaic_split_num);
    }
  }
  mosaic_displays.emplace_back(mosaic_display);
}

void GpuDevice::ParsePhysicalDisplaySetting(
    std::string &value, std::vector<uint32_t> &physical_displays) {
  std::istringstream i_value(value);
  std::string physical_split_str;
  std::vector<uint32_t> physical_duplicate_check;

  // Got physical display num
  while (std::getline(i_value, physical_split_str, ':')) {
    if (physical_split_str.empty() ||
        physical_split_str.find_first_not_of("0123456789") != std::string::npos)
      continue;
    size_t physical_split_num = atoi(physical_split_str.c_str());
    // Check and skip if the display already been used in other mosaic
    bool skip_duplicate_display = false;
    size_t physical_size = physical_duplicate_check.size();
    for (size_t i = 0; i < physical_size; i++) {
      if (physical_duplicate_check.at(i) == physical_split_num) {
        skip_duplicate_display = true;
        break;
      }
    }
    if (!skip_duplicate_display) {
      physical_displays.emplace_back(physical_split_num);
      physical_duplicate_check.emplace_back(physical_split_num);
    }
  }
}

void GpuDevice::ParseCloneDisplaySetting(
    std::string &value, std::vector<std::vector<uint32_t>> &cloned_displays) {
  std::istringstream i_value(value);
  std::string i_clone_split_str;
  std::vector<uint32_t> clone_duplicate_check;

  // Got clone sub display num
  std::vector<uint32_t> clone_display;
  while (std::getline(i_value, i_clone_split_str, '+')) {
    if (i_clone_split_str.empty() ||
        i_clone_split_str.find_first_not_of("0123456789") != std::string::npos)
      continue;
    size_t i_clone_split_num = atoi(i_clone_split_str.c_str());
    // Check and skip if the display already been used in other clone
    bool skip_duplicate_display = false;
    size_t clone_size = clone_duplicate_check.size();
    for (size_t i = 0; i < clone_size; i++) {
      if (clone_duplicate_check.at(i) == i_clone_split_num) {
        skip_duplicate_display = true;
        break;
      }
    }
    if (!skip_duplicate_display) {
      // save the sub display num for the mosaic display (don't care
      // if
      // the physical/logical display is existing/connected here)
      clone_display.emplace_back(i_clone_split_num);
      clone_duplicate_check.emplace_back(i_clone_split_num);
    }
  }
  cloned_displays.emplace_back(clone_display);
}

void GpuDevice::ParsePhysicalDisplayRotation(
    std::string &value, std::vector<uint32_t> &display_rotation,
    std::vector<uint32_t> &rotation_display_index) {
  std::istringstream i_value(value);
  std::string physical_index_str;
  std::getline(i_value, physical_index_str, ':');
  if (physical_index_str.empty() ||
      physical_index_str.find_first_not_of("0123456789") != std::string::npos)
    return;

  uint32_t physical_index = atoi(physical_index_str.c_str());
  // Check and skip if the display is already in use.
  bool skip_duplicate_display = false;
  size_t rotation_size = rotation_display_index.size();
  for (size_t i = 0; i < rotation_size; i++) {
    if (rotation_display_index.at(i) == physical_index) {
      skip_duplicate_display = true;
      break;
    }
  }

  if (skip_duplicate_display) {
    return;
  }

  std::string rotation_str;
  // Got split num
  std::getline(i_value, rotation_str, ':');
  if (rotation_str.empty() ||
      rotation_str.find_first_not_of("0123") != std::string::npos)
    return;

  uint32_t rotation_num = atoi(rotation_str.c_str());
  display_rotation.emplace_back(rotation_num);
  rotation_display_index.emplace_back(physical_index);
}

void GpuDevice::ParseFloatDisplaySetting(
    std::string &value, std::vector<HwcRect<int32_t>> &float_displays,
    std::vector<uint32_t> &float_display_indices) {
  std::istringstream i_value(value);
  std::string index_str;
  std::string float_rect_str;
  std::vector<int32_t> float_rect;

  // Got display index
  std::getline(i_value, index_str, ':');
  if (index_str.empty() ||
      index_str.find_first_not_of("0123456789") != std::string::npos) {
    return;
  }

  int32_t index = atoi(index_str.c_str());

  // Got rectangle configuration
  while (std::getline(i_value, float_rect_str, '+')) {
    if (float_rect_str.empty() ||
        float_rect_str.find_first_not_of("0123456789") != std::string::npos) {
      continue;
    }
    size_t float_rect_val = atoi(float_rect_str.c_str());

    // Save the rectangle - left, top, right & bottom
    float_rect.emplace_back(float_rect_val);
  }

  // If entire rect is available, store the parameters
  // TODO remove hard code
  if (float_rect.size() == 4) {
    float_display_indices.emplace_back(index);
    HwcRect<int32_t> rect = HwcRect<int32_t>(
        float_rect.at(0), float_rect.at(1), float_rect.at(2), float_rect.at(3));
    float_displays.emplace_back(rect);
  }
}

void GpuDevice::InitializeDisplayIndex(std::vector<uint32_t> &physical_displays,
                                       std::vector<NativeDisplay *> &displays) {
  std::vector<NativeDisplay *> unordered_displays =
      display_manager_->GetAllDisplays();
  size_t size = unordered_displays.size();
  uint32_t dispmgr_display_size = (uint32_t)size;

  if (physical_displays.empty()) {
    displays.swap(unordered_displays);
  } else {
    size = physical_displays.size();
    for (size_t i = 0; i < size; i++) {
      uint32_t pdisp_index = physical_displays.at(i);
      // Add the physical display only if it has been enumerated from DRM API.
      // Skip the non-existence display defined in hwc_display.ini file.
      if (pdisp_index < dispmgr_display_size) {
        displays.emplace_back(unordered_displays.at(pdisp_index));
      } else {
        ETRACE(
            "Physical display number: %u defined in hwc_display.ini "
            "doesn't exist in enumerated DRM display list (total: %u).",
            pdisp_index, dispmgr_display_size);
      }
    }

    if (displays.size() != unordered_displays.size()) {
      size = unordered_displays.size();
      for (size_t i = 0; i < size; i++) {
        NativeDisplay *display = unordered_displays.at(i);
        uint32_t temp = displays.size();
        for (size_t i = 0; i < temp; i++) {
          if (displays.at(i) == display) {
            display = NULL;
            break;
          }
        }

        if (display) {
          displays.emplace_back(display);
        }
      }
    }
  }
  // Re-order displays based on connection status.
  std::vector<NativeDisplay *> connected_displays;
  std::vector<NativeDisplay *> un_connected_displays;
  size = displays.size();
  for (size_t i = 0; i < size; i++) {
    NativeDisplay *temp = displays.at(i);
    if (temp->IsConnected()) {
      connected_displays.emplace_back(temp);
    } else {
      un_connected_displays.emplace_back(temp);
    }
  }

  displays.swap(connected_displays);

  if (!un_connected_displays.empty()) {
    size_t temp = un_connected_displays.size();
    for (size_t i = 0; i < temp; i++) {
      displays.emplace_back(un_connected_displays.at(i));
    }
  }

  for (size_t i = 0; i < size; i++) {
    displays.at(i)->SetDisplayOrder(i);
  }
}
void GpuDevice::InitializeDisplayRotation(
    std::vector<uint32_t> &display_rotation,
    std::vector<uint32_t> &rotation_display_index,
    std::vector<NativeDisplay *> &displays) {
  size_t rotation_size = rotation_display_index.size();
  for (size_t i = 0; i < rotation_size; i++) {
    HWCRotation rotation = static_cast<HWCRotation>(display_rotation.at(i));
    displays.at(rotation_display_index.at(i))->RotateDisplay(rotation);
  }
}

void GpuDevice::InitializeLogicalDisplay(
    std::vector<uint32_t> &logical_displays,
    std::vector<NativeDisplay *> &displays,
    std::vector<NativeDisplay *> &temp_displays, bool use_logical) {
  size_t size = displays.size();
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
}

void GpuDevice::InitializeMosaicDisplay(
    std::vector<NativeDisplay *> &total_displays_,
    std::vector<std::vector<uint32_t>> &mosaic_displays,
    std::vector<NativeDisplay *> &temp_displays,
    std::vector<bool> &available_displays) {
  size_t displays_size = temp_displays.size();
  for (size_t t = 0; t < displays_size; t++) {
    // Skip the displays which already be marked in other mosaic
    if (!available_displays.at(t))
      continue;
    bool skip_display = false;
    size_t mosaic_size = mosaic_displays.size();
    for (size_t m = 0; m < mosaic_size; m++) {
      std::vector<NativeDisplay *> i_available_mosaic_displays;
      size_t mosaic_inner_size = mosaic_displays.at(m).size();
      for (size_t l = 0; l < mosaic_inner_size; l++) {
        // Check if the logical display is in mosaic, keep the order of
        // logical displays list
        // Get the smallest logical num of the mosaic for order keeping
        if (t == mosaic_displays.at(m).at(l)) {
          // Loop to get logical displays of mosaic, keep the order of config
          for (size_t i = 0; i < mosaic_inner_size; i++) {
            // Verify the logical display num
            if (mosaic_displays.at(m).at(i) < displays_size) {
              // Skip the disconnected display here
              i_available_mosaic_displays.emplace_back(
                  temp_displays.at(mosaic_displays.at(m).at(i)));
              // Add tag for mosaic-ed displays
              available_displays.at(mosaic_displays.at(m).at(i)) = false;
            }
          }
          // Create mosaic for those logical displays
          if (i_available_mosaic_displays.size() > 0) {
            std::unique_ptr<MosaicDisplay> mosaic(
                new MosaicDisplay(i_available_mosaic_displays));
            mosaic_displays_.emplace_back(std::move(mosaic));
            // Save the mosaic to the final displays list
            total_displays_.emplace_back(mosaic_displays_.back().get());
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
}

void GpuDevice::InitializeCloneDisplay(
    std::vector<NativeDisplay *> &total_displays_,
    std::vector<std::vector<uint32_t>> &cloned_displays) {
  std::vector<NativeDisplay *> temp_displays;
  size_t displays_size = total_displays_.size();
  size_t cloned_displays_size = cloned_displays.size();
  for (size_t c = 0; c < cloned_displays_size; c++) {
    std::vector<uint32_t> &temp = cloned_displays.at(c);
    if (!temp.empty() && temp.size() >= 2) {
      size_t c_size = temp.size();
      if (total_displays_.size() > temp.at(0)) {
        NativeDisplay *physical_display = total_displays_.at(temp.at(0));
        for (size_t clone = 1; clone < c_size; clone++) {
          if (total_displays_.size() > temp.at(clone))
            total_displays_.at(temp.at(clone))->CloneDisplay(physical_display);
        }
      }
    }
  }

  for (size_t t = 0; t < displays_size; t++) {
    bool found = false;
    for (size_t c = 0; c < cloned_displays_size; c++) {
      std::vector<uint32_t> &temp = cloned_displays.at(c);
      size_t c_size = temp.size();
      for (size_t clone = 1; clone < c_size; clone++) {
        uint32_t temp_clone = temp.at(clone);
        if (temp_clone == t) {
          found = true;
          break;
        }
      }

      if (found) {
        break;
      }
    }

    // Don't advertise cloned display as another independent
    // Physical Display.
    if (found) {
      continue;
    }

    temp_displays.emplace_back(total_displays_.at(t));
  }
  temp_displays.swap(total_displays_);
}

void GpuDevice::InitializeFloatDisplay(
    std::vector<NativeDisplay *> &total_displays_,
    std::vector<HwcRect<int32_t>> &float_displays,
    std::vector<uint32_t> &float_display_indices) {
  bool ret = false;
  size_t size = float_display_indices.size();
  size_t num_displays = total_displays_.size();

  // Set custom resolution to desired display instance
  for (size_t i = 0; i < size; i++) {
    int index = float_display_indices.at(i);

    // Ignore float index if out of range of connected displays
    if ((size_t)index < num_displays) {
      const HwcRect<int32_t> &rect = float_displays.at(i);
      ret = total_displays_.at(index)->SetCustomResolution(rect);
    }
  }
}

void GpuDevice::HandleHWCSettings() {
// Handle config file reading
#ifndef KVM_HWC_PROPERTY
  const char *hwc_dp_cfg_path = HWC_DISPLAY_INI_PATH;
#else
  // Get Kvm property to decide if need to load KVM config
  std::string hwc_dp_cfg_path_str;
  if (IsKvmPlatform())
    hwc_dp_cfg_path_str = KVM_HWC_DISPLAY_INI_PATH;
  else
    hwc_dp_cfg_path_str = HWC_DISPLAY_INI_PATH;
  const char *hwc_dp_cfg_path = hwc_dp_cfg_path_str.c_str();
#endif
  ITRACE("Hwc display config file is %s", hwc_dp_cfg_path);

  bool use_logical = false;
  bool use_mosaic = false;
  bool use_cloned = false;
  bool rotate_display = false;
  bool use_float = false;
  std::vector<uint32_t> logical_displays;
  std::vector<uint32_t> physical_displays;
  std::vector<uint32_t> display_rotation;
  std::vector<uint32_t> float_display_indices;
  std::vector<uint32_t> rotation_display_index;
  std::vector<HwcRect<int32_t>> float_displays;
  std::vector<std::vector<uint32_t>> cloned_displays;
  std::vector<std::vector<uint32_t>> mosaic_displays;
#ifdef ENABLE_PANORAMA
  bool use_panorama = false;
  std::vector<std::vector<uint32_t>> panorama_displays;
  std::vector<std::vector<uint32_t>> panorama_sos_displays;
#endif

  std::ifstream fin(hwc_dp_cfg_path);
  std::string cfg_line;
  std::string key_logical("LOGICAL");
  std::string key_mosaic("MOSAIC");
  std::string key_clone("CLONE");
  std::string key_rotate("ROTATION");
  std::string key_float("FLOAT");
  std::string key_plane_reserved("PLANE_RESERVED");
  std::string key_logical_display("LOGICAL_DISPLAY");
  std::string key_mosaic_display("MOSAIC_DISPLAY");
  std::string key_physical_display("PHYSICAL_DISPLAY");
  std::string key_physical_display_rotation("PHYSICAL_DISPLAY_ROTATION");
  std::string key_clone_display("CLONE_DISPLAY");
  std::string key_float_display("FLOAT_DISPLAY");
#ifdef ENABLE_PANORAMA
  std::string key_panorama("PANORAMA");
  std::string key_panorama_display("PANORAMA_DISPLAY");
  std::string key_panorama_sos_display("PANORAMA_SOS_DISPLAY");
#endif

  std::string key_reserved_drm_plane("DRM_PLANE_RESERVED");

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
          // Got mosaic switch
        } else if (!key.compare(key_mosaic)) {
          if (!value.compare(enable_str)) {
            use_mosaic = true;
          }
#ifdef ENABLE_PANORAMA
          // Got panorama switch
        } else if (!key.compare(key_panorama)) {
          if (!value.compare(enable_str)) {
            use_panorama = true;
          }
#endif
          // Got clone switch
        } else if (!key.compare(key_clone)) {
          if (!value.compare(enable_str)) {
            use_cloned = true;
          }
          // Got rotation switch.
        } else if (!key.compare(key_rotate)) {
          if (!value.compare(enable_str)) {
            rotate_display = true;
          }
          // Got float switch
        } else if (!key.compare(key_float)) {
          if (!value.compare(enable_str)) {
            use_float = true;
          }
        } else if (!key.compare(key_plane_reserved)) {
          if (!value.compare(enable_str)) {
            reserve_plane_ = true;
          }
          // Got logical display index
        } else if (!key.compare(key_logical_display)) {
          ParseLogicalDisplaySetting(value, logical_displays);
          // Got mosaic config
        } else if (!key.compare(key_mosaic_display)) {
          ParseMosaicDisplaySetting(value, mosaic_displays);
#ifdef ENABLE_PANORAMA
          // Got panorama config
        } else if (!key.compare(key_panorama_display)) {
          ParsePanoramaDisplayConfig(value, panorama_displays);
          size_t panorama_displays_size = panorama_displays.size();
          // Got panorama sos config
        } else if (!key.compare(key_panorama_sos_display)) {
          ParsePanoramaSOSDisplayConfig(value, panorama_sos_displays);
          size_t panorama_sos_displays_size = panorama_sos_displays.size();
#endif
          // Got physical display config
        } else if (!key.compare(key_physical_display)) {
          ParsePhysicalDisplaySetting(value, physical_displays);
          // Got clone display config
        } else if (!key.compare(key_clone_display)) {
          ParseCloneDisplaySetting(value, cloned_displays);
          // Got physical display rotation config
        } else if (!key.compare(key_physical_display_rotation)) {
          ParsePhysicalDisplayRotation(value, display_rotation,
                                       rotation_display_index);
          // Got float display config
        } else if (!key.compare(key_float_display)) {
          ParseFloatDisplaySetting(value, float_displays,
                                   float_display_indices);
          // Got plan reserve config
        } else if (!key.compare(key_reserved_drm_plane)) {
          ParsePlaneReserveSettings(value);
        }
      }
    }
  };

  std::vector<NativeDisplay *> displays;
  InitializeDisplayIndex(physical_displays, displays);

  // We should have all displays ordered. Apply rotation settings.
  if (rotate_display) {
    InitializeDisplayRotation(display_rotation, rotation_display_index,
                              displays);
  }

  // Now, we should have all physical displays ordered as required.
  // Let's handle any Logical Display combinations or Mosaic.
  std::vector<NativeDisplay *> temp_displays;
  InitializeLogicalDisplay(logical_displays, displays, temp_displays,
                           use_logical);

  std::vector<bool> available_displays(temp_displays.size(), true);
  if (use_mosaic) {
    InitializeMosaicDisplay(total_displays_, mosaic_displays, temp_displays,
                            available_displays);
  } else {
    total_displays_.swap(temp_displays);
  }

#ifdef ENABLE_PANORAMA
  if (use_panorama && !use_mosaic && !use_cloned && !use_float) {
    InitializePanorama(total_displays_, temp_displays, panorama_displays,
                       panorama_sos_displays, available_displays);
  }
#endif

#ifdef ENABLE_PANORAMA
  if (use_cloned && !use_mosaic && !use_logical && !use_panorama) {
#else
  if (use_cloned && !use_mosaic && !use_logical) {
#endif
    InitializeCloneDisplay(total_displays_, cloned_displays);
  }

// Now set floating display configuration
// Get the floating display index and the respective rectangle
// TODO Logical display on & mosaic display on scenario
#ifdef ENABLE_PANORAMA
  if (use_float && !use_logical && !use_mosaic && !use_panorama) {
#else
  if (use_float && !use_logical && !use_mosaic) {
#endif
    InitializeFloatDisplay(total_displays_, float_displays,
                           float_display_indices);
  }
}

void GpuDevice::EnableHDCPSessionForDisplay(uint32_t connector,
                                            HWCContentType content_type) {
  display_manager_->EnableHDCPSessionForDisplay(connector, content_type);
}

void GpuDevice::EnableHDCPSessionForAllDisplays(HWCContentType content_type) {
  display_manager_->EnableHDCPSessionForAllDisplays(content_type);
}

void GpuDevice::DisableHDCPSessionForDisplay(uint32_t connector) {
  display_manager_->DisableHDCPSessionForDisplay(connector);
}

void GpuDevice::DisableHDCPSessionForAllDisplays() {
  display_manager_->DisableHDCPSessionForAllDisplays();
}

void GpuDevice::SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                                     uint32_t pavp_instance_id) {
  size_t size = total_displays_.size();
  for (size_t i = 0; i < size; i++) {
    total_displays_.at(i)->SetPAVPSessionStatus(enabled, pavp_session_id,
                                                pavp_instance_id);
  }
}

void GpuDevice::SetHDCPSRMForAllDisplays(const int8_t *SRM,
                                         uint32_t SRMLength) {
  display_manager_->SetHDCPSRMForAllDisplays(SRM, SRMLength);
}

void GpuDevice::SetHDCPSRMForDisplay(uint32_t connector, const int8_t *SRM,
                                     uint32_t SRMLength) {
  display_manager_->SetHDCPSRMForDisplay(connector, SRM, SRMLength);
}

uint32_t GpuDevice::GetDisplayIDFromConnectorID(const uint32_t connector_id) {
  size_t size = total_displays_.size();
  for (size_t i = 0; i < size; i++) {
    if (total_displays_.at(i)->ContainConnector(connector_id))
      return i;
  }
  return -1;
}

void GpuDevice::HandleRoutine() {
  // Iniitialize resources to monitor external events.
  // These can be two types:
  // 1) We are showing splash screen and another App
  //    needs to take the control. In this case splash
  //    is true.
  // 2) Another app is having control of display and we
  //    we need to take control.
  // TODO: Add splash screen support.
  if (lock_fd_ != -1) {
    if (flock(lock_fd_, LOCK_EX) != 0) {
      ITRACE("Fail to grab the hwc lock.");
    } else {
      ITRACE("Successfully grabbed the hwc lock.");
      // Set DRM master
      if (!display_manager_->IsDrmMaster())
        display_manager_->setDrmMaster(true);
      // stop ignoring and force refresh
      ResetAllDisplayCommit(true);
      flock(lock_fd_, LOCK_UN);
      close(lock_fd_);
      lock_fd_ = -1;
    }
  }
}

void GpuDevice::HandleWait() {
  if (lock_fd_ == -1) {
    HWCThread::HandleWait();
  }
}

void GpuDevice::DisableWatch() {
  if (lock_fd_ != -1) {
    close(lock_fd_);
    lock_fd_ = -1;
  }
}

GpuDevice &GpuDevice::getInstance() {
  static GpuDevice gpu_device_;
  return gpu_device_;
}

}  // namespace hwcomposer
