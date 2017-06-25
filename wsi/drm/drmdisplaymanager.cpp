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
  close(hotplug_fd_);
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

    std::unique_ptr<DrmDisplay> display(new DrmDisplay(fd_, i, c->crtc_id));
    if (!display->Initialize(buffer_handler_.get())) {
      ETRACE("Failed to Initialize Display %d", c->crtc_id);
      return false;
    }

    displays_.emplace_back(std::move(display));
  }

  virtual_display_.reset(new VirtualDisplay(fd_, buffer_handler_.get(), 0, 0));

  if (!UpdateDisplayState()) {
    ETRACE("Failed to connect display.");
    return false;
  }
  hotplug_fd_ = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (hotplug_fd_ < 0) {
    ETRACE("Failed to create socket for hot plug monitor. %s", PRINTERROR());
    return true;
  }

  struct sockaddr_nl addr;
  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  addr.nl_groups = -1;

  ret = bind(hotplug_fd_, (struct sockaddr *)&addr, sizeof(addr));
  if (ret) {
    ETRACE("Failed to bind sockaddr_nl and hot plug monitor fd. %s",
           PRINTERROR());
    return true;
  }

  fd_handler_.AddFd(hotplug_fd_);

  if (!InitWorker()) {
    ETRACE("Failed to initalizer thread to monitor Hot Plug events. %s",
           PRINTERROR());
  }

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
      else if (!strcmp(event, "HOTPLUG=1"))
        hotplug_event = true;

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
    display->DisConnect();
  }

  std::vector<NativeDisplay *>().swap(connected_displays_);
  for (int32_t i = 0; i < res->count_connectors; ++i) {
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
    std::vector<drmModeModeInfo> mode;
    uint32_t preferred_mode = 0;
    for (int32_t i = 0; i < connector->count_modes; ++i) {
      mode.emplace_back(connector->modes[i]);
      // There is only one preferred mode per connector.
      if (mode[i].type & DRM_MODE_TYPE_PREFERRED) {
        preferred_mode = i;
        break;
      }
    }

    // Lets try to find crts for any connected encoder.
    if (connector->encoder_id) {
      ScopedDrmEncoderPtr encoder(
          drmModeGetEncoder(fd_, connector->encoder_id));
      if (encoder && encoder->crtc_id) {
        for (auto &display : displays_) {
          // Set the modes supported for each display
          display->SetDrmModeInfo(mode);
          // At initilaization  preferred mode is set!
          if (encoder->crtc_id == display->CrtcId() &&
              display->ConnectDisplay(mode.at(preferred_mode),
                                      connector.get())) {
            connected_displays_.emplace_back(display.get());
            break;
          }
        }
      }
    } else {
      // Try to find an encoder for the connector.
      bool found_encoder = false;
      for (int32_t j = 0; j < connector->count_encoders; ++j) {
        ScopedDrmEncoderPtr encoder(
            drmModeGetEncoder(fd_, connector->encoders[j]));
        if (!encoder)
          continue;
        for (auto &display : displays_) {
          if (!display->IsConnected() &&
              (encoder->possible_crtcs & (1 << display->Pipe())) &&
              display->ConnectDisplay(mode.at(preferred_mode),
                                      connector.get())) {
            IHOTPLUGEVENTTRACE("connected pipe:%d \n", display->Pipe());
            // Set the modes supported for each display
            display->SetDrmModeInfo(mode);
            connected_displays_.emplace_back(display.get());
            found_encoder = true;
            break;
          }
        }
      }
      if (found_encoder)
        break;
    }
  }

  for (auto &display : displays_) {
    if (!display->IsConnected()) {
      display->SetPowerMode(kOff);
    }
  }

  if (connected_displays_.empty()) {
    if (!headless_)
      headless_.reset(new Headless(fd_, 0, 0));
  } else if (headless_) {
    headless_.reset(nullptr);
  }

  if (callback_) {
    callback_->Callback(connected_displays_);
  }

  spin_lock_.unlock();
  return true;
}

NativeDisplay *DrmDisplayManager::GetDisplay(uint32_t display_id) {
  CTRACE();
  spin_lock_.lock();

  NativeDisplay *display = NULL;
  if (headless_.get()) {
    display = headless_.get();
  } else {
    if (connected_displays_.size() > display_id)
      display = connected_displays_.at(display_id);
  }

  spin_lock_.unlock();
  return display;
}

NativeDisplay *DrmDisplayManager::GetVirtualDisplay() {
  return virtual_display_.get();
}

std::vector<NativeDisplay *> DrmDisplayManager::GetConnectedPhysicalDisplays()
    const {
  return connected_displays_;
}

void DrmDisplayManager::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  spin_lock_.lock();
  callback_ = callback;
  spin_lock_.unlock();
}

DisplayManager *DisplayManager::CreateDisplayManager() {
  return new DrmDisplayManager();
}

}  // namespace hwcomposer
