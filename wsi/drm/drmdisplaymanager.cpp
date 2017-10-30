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

#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <errno.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include <hwctrace.h>

#include <nativebufferhandler.h>

namespace hwcomposer {

DrmDisplayManager::DrmDisplayManager() : HWCThread(-8, "DisplayManager") {
  CTRACE();
}

DrmDisplayManager::~DrmDisplayManager() {
  CTRACE();
  std::vector<std::unique_ptr<DrmDisplay>>().swap(displays_);
#ifndef DISABLE_HOTPLUG_NOTIFICATION
  close(hotplug_fd_);
#endif
  close(fd_);
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

  buffer_handler_.reset(NativeBufferHandler::CreateInstance(fd_));
  if (!buffer_handler_) {
    ETRACE("Failed to create native buffer handler instance");
    return false;
  }

  ScopedDrmResourcesPtr res(drmModeGetResources(fd_));

  for (int32_t i = 0; i < res->count_crtcs; ++i) {
    ScopedDrmCrtcPtr c(drmModeGetCrtc(fd_, res->crtcs[i]));
    if (!c) {
      ETRACE("Failed to get crtc %d", res->crtcs[i]);
      return false;
    }

    std::unique_ptr<DrmDisplay> display(
        new DrmDisplay(fd_, i, c->crtc_id, this));
    if (!display->Initialize(buffer_handler_.get())) {
      ETRACE("Failed to Initialize Display %d", c->crtc_id);
      return false;
    }

    displays_.emplace_back(std::move(display));
  }

  virtual_display_.reset(new VirtualDisplay(fd_, buffer_handler_.get(), 0, 0));

  hwc_lock_.reset(new HWCLock());
  if (!hwc_lock_->RegisterCallBack(this)) {
    hwc_lock_.reset(nullptr);
    ForceRefresh();
  }

  if (!UpdateDisplayState()) {
    ETRACE("Failed to connect display.");
    return false;
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

  int flags = fcntl(hotplug_fd_, F_GETFL, 0);
  fcntl(hotplug_fd_, F_SETFL, flags | O_NONBLOCK);

  fd_handler_.AddFd(hotplug_fd_);

  if (!InitWorker()) {
    ETRACE("Failed to initalizer thread to monitor Hot Plug events. %s",
           PRINTERROR());
  }
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

void DrmDisplayManager::HandleRoutine() {
  CTRACE();
  IHOTPLUGEVENTTRACE("DisplayManager::Routine.");
  if (fd_handler_.IsReady(hotplug_fd_)) {
    IHOTPLUGEVENTTRACE("Recieved Hot plug notification.");
    HotPlugEventHandler();
  }

  if (lock_reset_) {
    spin_lock_.lock();
    if (release_lock_ && hwc_lock_.get()) {
      hwc_lock_->DisableWatch();
      hwc_lock_.reset(nullptr);
      release_lock_ = false;
      lock_reset_ = false;
    }
    spin_lock_.unlock();
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

NativeDisplay *DrmDisplayManager::GetVirtualDisplay() {
  spin_lock_.lock();
  NativeDisplay *display = virtual_display_.get();
  spin_lock_.unlock();
  return display;
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
  size_t size = displays_.size();
  for (size_t i = 0; i < size; ++i) {
    displays_.at(i)->ForceRefresh();
  }

  release_lock_ = true;
  spin_lock_.unlock();
}

DisplayManager *DisplayManager::CreateDisplayManager() {
  return new DrmDisplayManager();
}

}  // namespace hwcomposer
