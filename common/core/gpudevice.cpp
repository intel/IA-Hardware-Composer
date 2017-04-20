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

#include <memory>
#include <utility>
#include <vector>

#include "display.h"
#include "displayplanemanager.h"
#include "drmscopedtypes.h"
#include "headless.h"
#include "hwcthread.h"
#include "overlaybuffermanager.h"
#include "spinlock.h"
#include "vblankeventhandler.h"
#include "virtualdisplay.h"

namespace hwcomposer {

class GpuDevice::DisplayManager : public HWCThread {
 public:
  DisplayManager();
  ~DisplayManager() override;

  bool Init(uint32_t fd);

  bool UpdateDisplayState();

  NativeDisplay *GetDisplay(uint32_t display);

  NativeDisplay *GetVirtualDisplay();

  std::vector<NativeDisplay *> GetConnectedPhysicalDisplays() const;

  void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback);

 protected:
  void HandleWait() override;
  void HandleRoutine() override;

 private:
  void HotPlugEventHandler();
  std::unique_ptr<NativeDisplay> headless_;
  std::unique_ptr<NativeDisplay> virtual_display_;
  std::vector<std::unique_ptr<NativeDisplay>> displays_;
  std::vector<NativeDisplay *> connected_displays_;
  std::shared_ptr<DisplayHotPlugEventCallback> callback_ = NULL;
  std::unique_ptr<OverlayBufferManager> buffer_manager_;
  int fd_ = -1;
  ScopedFd hotplug_fd_;
  SpinLock spin_lock_;
};

GpuDevice::DisplayManager::DisplayManager() : HWCThread(-8, "DisplayManager") {
  CTRACE();
}

GpuDevice::DisplayManager::~DisplayManager() {
  CTRACE();
}

bool GpuDevice::DisplayManager::Init(uint32_t fd) {
  CTRACE();
  fd_ = fd;
  int ret = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ETRACE("Failed to set atomic cap %d", ret);
    return false;
  }

  buffer_manager_.reset(new OverlayBufferManager());
  if (!buffer_manager_->Initialize(fd_)) {
    ETRACE("Failed to Initialize Buffer Manager.");
    return false;
  }

  ScopedDrmResourcesPtr res(drmModeGetResources(fd_));

  for (int32_t i = 0; i < res->count_crtcs; ++i) {
    ScopedDrmCrtcPtr c(drmModeGetCrtc(fd_, res->crtcs[i]));
    if (!c) {
      ETRACE("Failed to get crtc %d", res->crtcs[i]);
      return false;
    }

    std::unique_ptr<NativeDisplay> display(new Display(fd_, i, c->crtc_id));
    if (!display->Initialize(buffer_manager_.get())) {
      ETRACE("Failed to Initialize Display %d", c->crtc_id);
      return false;
    }

    displays_.emplace_back(std::move(display));
  }

  virtual_display_.reset(new VirtualDisplay(fd_, buffer_manager_.get(), 0, 0));

  if (!UpdateDisplayState()) {
    ETRACE("Failed to connect display.");
    return false;
  }
  hotplug_fd_.Reset(socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT));
  if (hotplug_fd_.get() < 0) {
    ETRACE("Failed to create socket for hot plug monitor. %s", PRINTERROR());
    return true;
  }

  struct sockaddr_nl addr;
  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  addr.nl_groups = -1;

  ret = bind(hotplug_fd_.get(), (struct sockaddr *)&addr, sizeof(addr));
  if (ret) {
    ETRACE("Failed to bind sockaddr_nl and hot plug monitor fd. %s",
           PRINTERROR());
    return true;
  }

  fd_handler_.AddFd(hotplug_fd_.get());

  if (!InitWorker()) {
    ETRACE("Failed to initalizer thread to monitor Hot Plug events. %s",
           PRINTERROR());
  }

  IHOTPLUGEVENTTRACE("DisplayManager Initialization succeeded.");

  return true;
}

void GpuDevice::DisplayManager::HotPlugEventHandler() {
  CTRACE();
  uint32_t buffer_size = 1024;
  char buffer[buffer_size];
  int ret;

  while (true) {
    bool drm_event = false, hotplug_event = false;
    memset(&buffer, 0, sizeof(buffer));
    size_t srclen = buffer_size - 1;
    ret = read(hotplug_fd_.get(), &buffer, srclen);
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

void GpuDevice::DisplayManager::HandleWait() {
  if (fd_handler_.Poll(-1) <= 0) {
    ETRACE("Poll Failed in DisplayManager %s", PRINTERROR());
  }
}

void GpuDevice::DisplayManager::HandleRoutine() {
  CTRACE();
  IHOTPLUGEVENTTRACE("DisplayManager::Routine.");
  if (fd_handler_.IsReady(hotplug_fd_.get())) {
    IHOTPLUGEVENTTRACE("Recieved Hot plug notification.");
    HotPlugEventHandler();
  }
}

bool GpuDevice::DisplayManager::UpdateDisplayState() {
  CTRACE();
  ScopedDrmResourcesPtr res(drmModeGetResources(fd_));
  if (!res) {
    ETRACE("Failed to get DrmResources resources");
    return false;
  }

  ScopedSpinLock lock(spin_lock_);
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

    drmModeModeInfo mode;
    memset(&mode, 0, sizeof(mode));
    bool found_prefered_mode = false;
    for (int32_t i = 0; i < connector->count_modes; ++i) {
      mode = connector->modes[i];
      // There is only one preferred mode per connector.
      if (mode.type & DRM_MODE_TYPE_PREFERRED) {
        found_prefered_mode = true;
        break;
      }
    }

    if (!found_prefered_mode)
      continue;

    // Lets try to find crts for any connected encoder.
    if (connector->encoder_id) {
      ScopedDrmEncoderPtr encoder(
          drmModeGetEncoder(fd_, connector->encoder_id));
      if (encoder && encoder->crtc_id) {
        for (auto &display : displays_) {
          if (encoder->crtc_id == display->CrtcId() &&
              display->Connect(mode, connector.get())) {
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
              display->Connect(mode, connector.get())) {
            IHOTPLUGEVENTTRACE("connected pipe:%d \n", display->Pipe());
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
      display->ShutDown();
    }
  }

  if (connected_displays_.empty()) {
    if (!headless_)
      headless_.reset(new Headless(fd_, 0, 0));
  } else if (headless_) {
    headless_.release();
  }

  if (callback_) {
    callback_->Callback(connected_displays_);
  }

  return true;
}

NativeDisplay *GpuDevice::DisplayManager::GetDisplay(uint32_t display_id) {
  CTRACE();
  ScopedSpinLock lock(spin_lock_);

  NativeDisplay *display = NULL;
  if (headless_.get()) {
    display = headless_.get();
  } else {
    if (connected_displays_.size() > display_id)
      display = connected_displays_.at(display_id);
  }

  return display;
}

NativeDisplay *GpuDevice::DisplayManager::GetVirtualDisplay() {
  return virtual_display_.get();
}

std::vector<NativeDisplay *>
GpuDevice::DisplayManager::GetConnectedPhysicalDisplays() const {
  return connected_displays_;
}

void GpuDevice::DisplayManager::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  ScopedSpinLock lock(spin_lock_);
  callback_ = callback;
}

GpuDevice::GpuDevice() : initialized_(false) {
  CTRACE();
}

GpuDevice::~GpuDevice() {
  CTRACE();
}

bool GpuDevice::Initialize() {
  CTRACE();
  if (initialized_)
    return true;

  fd_.Reset(drmOpen("i915", NULL));
  if (fd_.get() < 0) {
    ETRACE("Failed to open dri %s", PRINTERROR());
    return -ENODEV;
  }

  struct drm_set_client_cap cap = {DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1};
  drmIoctl(fd_.get(), DRM_IOCTL_SET_CLIENT_CAP, &cap);
  int ret = drmSetClientCap(fd_.get(), DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ETRACE("Failed to set atomic cap %s", PRINTERROR());
    return false;
  }
  ScopedDrmResourcesPtr res(drmModeGetResources(fd_.get()));

  initialized_ = true;
  display_manager_.reset(new DisplayManager());

  return display_manager_->Init(fd_.get());
}

NativeDisplay *GpuDevice::GetDisplay(uint32_t display_id) {
  return display_manager_->GetDisplay(display_id);
}

NativeDisplay *GpuDevice::GetVirtualDisplay() {
  return display_manager_->GetVirtualDisplay();
}

std::vector<NativeDisplay *> GpuDevice::GetConnectedPhysicalDisplays() {
  return display_manager_->GetConnectedPhysicalDisplays();
}

void GpuDevice::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  display_manager_->RegisterHotPlugEventCallback(callback);
}

}  // namespace hwcomposer
