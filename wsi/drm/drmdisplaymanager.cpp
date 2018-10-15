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

#include "drmdisplaymanager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <linux/netlink.h>
#include <linux/types.h>

#include <gpudevice.h>
#include <hwctrace.h>

#include <nativebufferhandler.h>

namespace hwcomposer {

DrmDisplayManager::DrmDisplayManager(GpuDevice *device)
    : HWCThread(-8, "DisplayManager"), device_(device) {
  CTRACE();
}

DrmDisplayManager::~DrmDisplayManager() {
  CTRACE();
  std::vector<std::unique_ptr<DrmDisplay>>().swap(displays_);
#ifndef DISABLE_HOTPLUG_NOTIFICATION
  close(hotplug_fd_);
#endif
  drmClose(fd_);
}

bool DrmDisplayManager::Initialize() {
  CTRACE();
  fd_ = drmOpen("i915", NULL);
  if (fd_ < 0) {
    ETRACE("Failed to open dri %s", PRINTERROR());
    return -ENODEV;
  }

  struct drm_set_client_cap cap = {DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1};
  drmIoctl(fd_, DRM_IOCTL_SET_CLIENT_CAP, &cap);
  int ret = drmSetClientCap(fd_, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ETRACE("Failed to set atomic cap %s", PRINTERROR());
    return false;
  }

  ret = drmSetClientCap(fd_, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ETRACE("Failed to set atomic cap %d", ret);
    return false;
  }

  ScopedDrmResourcesPtr res(drmModeGetResources(fd_));
  if (res->count_crtcs == 0)
    return false;

  for (int32_t i = 0; i < res->count_crtcs; ++i) {
    ScopedDrmCrtcPtr c(drmModeGetCrtc(fd_, res->crtcs[i]));
    if (!c) {
      ETRACE("Failed to get crtc %d", res->crtcs[i]);
      return false;
    }

    std::unique_ptr<DrmDisplay> display(
        new DrmDisplay(fd_, i, c->crtc_id, this));

    displays_.emplace_back(std::move(display));
  }

#ifndef DISABLE_HOTPLUG_NOTIFICATION
  hotplug_fd_ = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (hotplug_fd_ < 0) {
    ETRACE("Failed to create socket for hot plug monitor. %s", PRINTERROR());
    return true;
  }

  struct sockaddr_nl addr;
  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  addr.nl_groups = 0xffffffff;

  ret = bind(hotplug_fd_, (struct sockaddr *)&addr, sizeof(addr));
  if (ret) {
    ETRACE("Failed to bind sockaddr_nl and hot plug monitor fd. %s",
           PRINTERROR());
    return true;
  }

  fd_handler_.AddFd(hotplug_fd_);
#endif
  IHOTPLUGEVENTTRACE("DisplayManager Initialization succeeded.");
  return true;
}

void DrmDisplayManager::HotPlugEventHandler() {
  CTRACE();
  int fd = hotplug_fd_;
  char buffer[DRM_HOTPLUG_EVENT_SIZE];
  int ret;

  memset(&buffer, 0, sizeof(buffer));
  while (true) {
    bool drm_event = false, hotplug_event = false;
    size_t srclen = DRM_HOTPLUG_EVENT_SIZE - 1;
    ret = read(fd, &buffer, srclen);
    if (ret <= 0) {
      if (ret < 0)
        ETRACE("Failed to read uevent. %s", PRINTERROR());

      return;
    }

    buffer[ret] = '\0';

    for (int32_t i = 0; i < ret;) {
      char *event = buffer + i;
      if (!strcmp(event, "DEVTYPE=drm_minor"))
        drm_event = true;
      else if (!strcmp(event, "HOTPLUG=1") ||  // Common hotplug request
               !strcmp(event,
                       "HDMI-Change")) {  // Hotplug happened during suspend
        hotplug_event = true;
      }

      if (hotplug_event && drm_event)
        break;

      i += strlen(event) + 1;
    }

    if (drm_event && hotplug_event) {
      IHOTPLUGEVENTTRACE(
          "Recieved Hot Plug event related to display calling "
          "UpdateDisplayState.");
      UpdateDisplayState();
    }
  }
}

void DrmDisplayManager::HandleWait() {
  if (fd_handler_.Poll(-1) <= 0) {
    ETRACE("Poll Failed in DisplayManager %s", PRINTERROR());
  }
}

void DrmDisplayManager::InitializeDisplayResources() {
  buffer_handler_.reset(NativeBufferHandler::CreateInstance(fd_));
  frame_buffer_manager_.reset(new FrameBufferManager(fd_));
  if (!buffer_handler_) {
    ETRACE("Failed to create native buffer handler instance");
    return;
  }

  int size = displays_.size();
  for (int i = 0; i < size; ++i) {
    if (!displays_.at(i)->Initialize(buffer_handler_.get(),
                                     frame_buffer_manager_.get())) {
      ETRACE("Failed to Initialize Display %d", i);
    }
  }
}

void DrmDisplayManager::StartHotPlugMonitor() {
  if (!UpdateDisplayState()) {
    ETRACE("Failed to connect display.");
  }

  if (!InitWorker()) {
    ETRACE("Failed to initalizer thread to monitor Hot Plug events. %s",
           PRINTERROR());
  }
}

void DrmDisplayManager::HandleRoutine() {
  CTRACE();
  IHOTPLUGEVENTTRACE("DisplayManager::Routine.");
  if (fd_handler_.IsReady(hotplug_fd_)) {
    IHOTPLUGEVENTTRACE("Recieved Hot plug notification.");
    HotPlugEventHandler();
  }
}

bool DrmDisplayManager::UpdateDisplayState() {
  CTRACE();
  ScopedDrmResourcesPtr res(drmModeGetResources(fd_));
  if (!res) {
    ETRACE("Failed to get DrmResources resources");
    return false;
  }

  spin_lock_.lock();
  // Start of assuming no displays are connected
  for (auto &display : displays_) {
    display->MarkForDisconnect();
  }

  std::vector<NativeDisplay *> connected_displays;
  std::vector<uint32_t> no_encoder;
  uint32_t total_connectors = res->count_connectors;
  for (uint32_t i = 0; i < total_connectors; ++i) {
    ScopedDrmConnectorPtr connector(
        drmModeGetConnector(fd_, res->connectors[i]));
    if (!connector) {
      ETRACE("Failed to get connector %d", res->connectors[i]);
      break;
    }
    // check if a monitor is connected.
    if (connector->connection != DRM_MODE_CONNECTED)
      continue;

    // Ensure we have atleast one valid mode.
    if (connector->count_modes == 0)
      continue;

    if (connector->encoder_id == 0) {
      no_encoder.emplace_back(i);
      continue;
    }

    std::vector<drmModeModeInfo> mode;
    uint32_t preferred_mode = 0;
    uint32_t size = connector->count_modes;
    mode.resize(size);
    for (uint32_t i = 0; i < size; ++i) {
      mode[i] = connector->modes[i];
      // There is only one preferred mode per connector.
      if (mode[i].type & DRM_MODE_TYPE_PREFERRED) {
        preferred_mode = i;
      }
    }

    // Lets try to find crts for any connected encoder.
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd_, connector->encoder_id));
    if (encoder && encoder->crtc_id) {
      for (auto &display : displays_) {
        IHOTPLUGEVENTTRACE(
            "Trying to connect %d with crtc: %d is display connected: %d \n",
            encoder->crtc_id, display->CrtcId(), display->IsConnected());
        // At initilaization  preferred mode is set!
        if (!display->IsConnected() && encoder->crtc_id == display->CrtcId() &&
            display->ConnectDisplay(mode.at(preferred_mode), connector.get(),
                                    preferred_mode)) {
          IHOTPLUGEVENTTRACE("Connected %d with crtc: %d pipe:%d \n",
                             encoder->crtc_id, display->CrtcId(),
                             display->GetDisplayPipe());
          // Set the modes supported for each display
          display->SetDrmModeInfo(mode);
          break;
        }
      }
    }
  }

  // Deal with connectors with encoder_id == 0.
  uint32_t size = no_encoder.size();
  for (uint32_t i = 0; i < size; ++i) {
    ScopedDrmConnectorPtr connector(
        drmModeGetConnector(fd_, res->connectors[no_encoder.at(i)]));
    if (!connector) {
      ETRACE("Failed to get connector %d", res->connectors[i]);
      break;
    }

    std::vector<drmModeModeInfo> mode;
    uint32_t preferred_mode = 0;
    uint32_t size = connector->count_modes;
    mode.resize(size);
    for (uint32_t i = 0; i < size; ++i) {
      mode[i] = connector->modes[i];
      // There is only one preferred mode per connector.
      if (mode[i].type & DRM_MODE_TYPE_PREFERRED) {
        preferred_mode = i;
      }
    }

    // Try to find an encoder for the connector.
    size = connector->count_encoders;
    for (uint32_t j = 0; j < size; ++j) {
      ScopedDrmEncoderPtr encoder(
          drmModeGetEncoder(fd_, connector->encoders[j]));
      if (!encoder)
        continue;

      for (auto &display : displays_) {
        if (!display->IsConnected() &&
            (encoder->possible_crtcs & (1 << display->GetDisplayPipe())) &&
            display->ConnectDisplay(mode.at(preferred_mode), connector.get(),
                                    preferred_mode)) {
          IHOTPLUGEVENTTRACE("Connected with crtc: %d pipe:%d \n",
                             display->CrtcId(), display->GetDisplayPipe());
          // Set the modes supported for each display
          display->SetDrmModeInfo(mode);
          break;
        }
      }
    }
  }

  for (auto &display : displays_) {
    if (!display->IsConnected()) {
      display->DisConnect();
    } else if (callback_) {
      connected_displays.emplace_back(display.get());
    }
  }

  if (callback_) {
    callback_->Callback(connected_displays);
  }

  spin_lock_.unlock();
#ifndef ENABLE_ANDROID_WA
  notify_client_ = true;
#endif

  if (notify_client_ || (!(displays_.at(0)->IsConnected()))) {
    IHOTPLUGEVENTTRACE("NotifyClientsOfDisplayChangeStatus Called %d %d \n",
                       notify_client_, displays_.at(0)->IsConnected());
    NotifyClientsOfDisplayChangeStatus();
  }

  return true;
}

void DrmDisplayManager::NotifyClientsOfDisplayChangeStatus() {
  spin_lock_.lock();
  bool disable_last_plane_usage = false;
  uint32_t total_connected_displays = 0;
  for (auto &display : displays_) {
    if (display->IsConnected()) {
      display->NotifyClientOfDisConnectedState();
      total_connected_displays++;
    }

    if (total_connected_displays > 1) {
      disable_last_plane_usage = true;
      break;
    }
  }

  for (auto &display : displays_) {
    display->NotifyDisplayWA(disable_last_plane_usage);
    if (!ignore_updates_) {
      display->ForceRefresh();
    }
  }

  for (auto &display : displays_) {
    if (!display->IsConnected()) {
      display->NotifyClientOfDisConnectedState();
    } else {
      display->NotifyClientOfConnectedState();
    }
  }

#ifdef ENABLE_ANDROID_WA
  notify_client_ = true;
#endif

  spin_lock_.unlock();
}

NativeDisplay *DrmDisplayManager::CreateVirtualDisplay(uint32_t display_index) {
  spin_lock_.lock();
  NativeDisplay *latest_display;
  std::unique_ptr<VirtualDisplay> display(
      new VirtualDisplay(fd_, buffer_handler_.get(),
                         frame_buffer_manager_.get(), display_index, 0));
  virtual_displays_.emplace_back(std::move(display));
  size_t size = virtual_displays_.size();
  latest_display = virtual_displays_.at(size - 1).get();
  spin_lock_.unlock();
  return latest_display;
}

void DrmDisplayManager::DestroyVirtualDisplay(uint32_t display_index) {
  spin_lock_.lock();
  virtual_displays_.at(display_index).reset(nullptr);
  spin_lock_.unlock();
}

std::vector<NativeDisplay *> DrmDisplayManager::GetAllDisplays() {
  spin_lock_.lock();
  std::vector<NativeDisplay *> all_displays;
  size_t size = displays_.size();
  for (size_t i = 0; i < size; ++i) {
    all_displays.emplace_back(displays_.at(i).get());
  }
  spin_lock_.unlock();
  return all_displays;
}

void DrmDisplayManager::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  spin_lock_.lock();
  callback_ = callback;
  spin_lock_.unlock();
}

void DrmDisplayManager::ForceRefresh() {
  spin_lock_.lock();
  ignore_updates_ = false;
  size_t size = displays_.size();
  for (size_t i = 0; i < size; ++i) {
    displays_.at(i)->ForceRefresh();
  }

  release_lock_ = true;
  spin_lock_.unlock();
}

void DrmDisplayManager::IgnoreUpdates() {
  spin_lock_.lock();
  ignore_updates_ = true;
  spin_lock_.unlock();

  size_t size = displays_.size();
  for (size_t i = 0; i < size; ++i) {
    displays_.at(i)->IgnoreUpdates();
  }
}

void DrmDisplayManager::setDrmMaster() {
  int ret = drmSetMaster(fd_);
  if (ret) {
    ETRACE("Failed to call drmSetMaster : %s", PRINTERROR());
  }
}

void DrmDisplayManager::HandleLazyInitialization() {
  spin_lock_.lock();
  if (release_lock_) {
    device_->DisableWatch();
    release_lock_ = false;
  }
  spin_lock_.unlock();
}

uint32_t DrmDisplayManager::GetConnectedPhysicalDisplayCount() {
  size_t size = displays_.size();
  uint32_t count = 0;
  for (size_t i = 0; i < size; i++) {
    if (displays_[i]->IsConnected()) {
      count++;
    }
  }
  return count;
}

DisplayManager *DisplayManager::CreateDisplayManager(GpuDevice *device) {
  return new DrmDisplayManager(device);
}

}  // namespace hwcomposer
