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

#include "physicaldisplay.h"

#include <cmath>

#include <hwcdefs.h>
#include <hwclayer.h>
#include <hwctrace.h>

#include <algorithm>
#include <string>
#include <sstream>

#include "displayqueue.h"
#include "displayplanemanager.h"
#include "hwcutils.h"

namespace hwcomposer {

PhysicalDisplay::PhysicalDisplay(uint32_t gpu_fd, uint32_t pipe_id)
    : pipe_(pipe_id),
      width_(0),
      height_(0),
      dpix_(0),
      dpiy_(0),
      gpu_fd_(gpu_fd),
      power_mode_(kOn) {
}

PhysicalDisplay::~PhysicalDisplay() {
}

bool PhysicalDisplay::Initialize(NativeBufferHandler *buffer_handler) {
  display_queue_.reset(new DisplayQueue(gpu_fd_, false, buffer_handler, this));
  InitializeDisplay();
  return true;
}

void PhysicalDisplay::MarkForDisconnect() {
  modeset_lock_.lock();
  IHOTPLUGEVENTTRACE("PhysicalDisplay::MarkForDisconnect recieved.");
  display_state_ |= kDisconnectionInProgress;
  display_state_ |= kRefreshClonedDisplays;
  if (pipe_ == 0 && !(display_state_ & kInitialized))
    display_state_ |= kHandlePendingHotPlugNotifications;

  modeset_lock_.unlock();
}

void PhysicalDisplay::NotifyClientOfConnectedState() {
  modeset_lock_.lock();
  if (hotplug_callback_ && (display_state_ & kConnected)) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay Sent Hotplug even call back with connected value set "
        "to true. %p hotplugdisplayid: %d \n",
        this, hot_plug_display_id_);
    hotplug_callback_->Callback(hot_plug_display_id_, true);
  }
  modeset_lock_.unlock();
}

void PhysicalDisplay::NotifyClientOfDisConnectedState() {
  modeset_lock_.lock();
  if (hotplug_callback_ && !(display_state_ & kConnected)) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay Sent Hotplug even call back with connected value set "
        "to false. %p hotplugdisplayid: %d \n",
        this, hot_plug_display_id_);
    hotplug_callback_->Callback(hot_plug_display_id_, false);
  }
  modeset_lock_.unlock();
}

void PhysicalDisplay::DisConnect() {
  modeset_lock_.lock();
  display_state_ &= ~kDisconnectionInProgress;
  if (!(display_state_ & kConnected)) {
    modeset_lock_.unlock();
    return;
  }

  modeset_lock_.unlock();
  SetPowerMode(kOff);
  display_state_ &= ~kConnected;
}

void PhysicalDisplay::Connect() {
  modeset_lock_.lock();
  display_state_ &= ~kDisconnectionInProgress;
  IHOTPLUGEVENTTRACE("PhysicalDisplay::Connect recieved. %p \n", this);
  if (display_state_ & kConnected) {
    modeset_lock_.unlock();
    return;
  }

  display_state_ |= kConnected;
  display_state_ &= ~kInitialized;

  if (!display_queue_->Initialize(pipe_, width_, height_, this)) {
    ETRACE("Failed to initialize Display Queue.");
  } else {
    display_state_ |= kInitialized;
  }

  UpdatePowerMode();

  modeset_lock_.unlock();
}

bool PhysicalDisplay::IsConnected() const {
  if (display_state_ & kDisconnectionInProgress)
    return false;

  return display_state_ & kConnected;
}

uint32_t PhysicalDisplay::PowerMode() const {
  return power_mode_;
}

int PhysicalDisplay::GetDisplayPipe() {
  if (!(display_state_ & kConnected))
    return -1;

  return pipe_;
}

bool PhysicalDisplay::SetActiveConfig(uint32_t config) {
  // update the activeConfig
  IHOTPLUGEVENTTRACE(
      "SetActiveConfig: New config to be used %d pipe: %p display: %p", config,
      pipe_, this);
  config_ = config;
  display_queue_->DisplayConfigurationChanged();
  display_state_ |= kNeedsModeset;
  UpdateDisplayConfig();
  return true;
}

bool PhysicalDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;
  IHOTPLUGEVENTTRACE(
      "GetActiveConfig: Current config being used Config: %d pipe: %d display: "
      "%p",
      config_, pipe_, this);
  *config = config_;
  return true;
}

bool PhysicalDisplay::SetPowerMode(uint32_t power_mode) {
  ScopedSpinLock lock(modeset_lock_);
  if (power_mode_ == power_mode) {
    return true;
  }

  power_mode_ = power_mode;
  if (!(display_state_ & kConnected)) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay is not connected, postponing power mode update.");
    display_state_ |= kPendingPowerMode;
    return true;
  } else if (display_state_ & kDisconnectionInProgress) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay diconnection in progress, postponing power mode "
        "update.");
    // Don't update power mode in case disconnect is in
    // progress.
    display_state_ |= kPendingPowerMode;
    return true;
  }

  return UpdatePowerMode();
}

bool PhysicalDisplay::UpdatePowerMode() {
  display_state_ &= ~kPendingPowerMode;

  if (power_mode_ == kOn) {
    display_state_ |= kNeedsModeset;
    display_state_ |= kUpdateDisplay;
    PowerOn();
  } else {
    display_state_ &= ~kUpdateDisplay;
  }

  if (!(display_state_ & kInitialized))
    return true;

  return display_queue_->SetPowerMode(power_mode_);
}

bool PhysicalDisplay::Present(std::vector<HwcLayer *> &source_layers,
                              int32_t *retire_fence) {
  CTRACE();
  modeset_lock_.lock();

  if (source_display_) {
    ETRACE("Trying to update display independently when in cloned mode.%p \n",
           this);
  }

  if (!(display_state_ & kUpdateDisplay)) {
    bool success = true;
    if (power_mode_ != kDozeSuspend) {
      ETRACE("Trying to update an Disconnected Display.%p \n", this);
      success = false;
    }

    modeset_lock_.unlock();
    return success;
  }

  if (display_state_ & kRefreshClonedDisplays) {
    RefreshClones();
  }

  bool handle_hoplug_notifications = false;
  if (display_state_ & kHandlePendingHotPlugNotifications) {
    display_state_ &= ~kHandlePendingHotPlugNotifications;
    handle_hoplug_notifications = true;
  }
  modeset_lock_.unlock();

  if (handle_hoplug_notifications) {
    NotifyClientsOfDisplayChangeStatus();
  }

  bool cloned = !clones_.empty();
  bool success =
      display_queue_->QueueUpdate(source_layers, retire_fence, false);
  if (success && cloned) {
    HandleClonedDisplays(source_layers);
  }

  size_t size = source_layers.size();
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer *layer = source_layers.at(layer_index);
    if (!layer->IsVisible())
      continue;

    layer->Validate();
  }

  return success;
}

bool PhysicalDisplay::PresentClone(std::vector<HwcLayer *> &source_layers,
                                   int32_t *retire_fence, bool idle_frame) {
  CTRACE();
  modeset_lock_.lock();
  if (display_state_ & kRefreshClonedDisplays) {
    RefreshClones();
  }
  modeset_lock_.unlock();

  bool success =
      display_queue_->QueueUpdate(source_layers, retire_fence, idle_frame);
  HandleClonedDisplays(source_layers);
  return success;
}

void PhysicalDisplay::HandleClonedDisplays(
    std::vector<HwcLayer *> &source_layers) {
  if (clones_.empty())
    return;

  int32_t fence = -1;
  for (auto display : clones_) {
    display->PresentClone(source_layers, &fence,
                          display_queue_->WasLastFrameIdleUpdate());
  }
}

int PhysicalDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display_id) {
  return display_queue_->RegisterVsyncCallback(callback, display_id);
}

void PhysicalDisplay::RegisterRefreshCallback(
    std::shared_ptr<RefreshCallback> callback, uint32_t display_id) {
  return display_queue_->RegisterRefreshCallback(callback, display_id);
}

void PhysicalDisplay::RegisterHotPlugCallback(
    std::shared_ptr<HotPlugCallback> callback, uint32_t display_id) {
  modeset_lock_.lock();
  hot_plug_display_id_ = display_id;
  hotplug_callback_ = callback;
  bool connected = display_state_ & kConnected;
  modeset_lock_.unlock();
  if (hotplug_callback_ && pipe_ == 0) {
    if (connected) {
      hotplug_callback_->Callback(hot_plug_display_id_, true);
    } else {
      hotplug_callback_->Callback(hot_plug_display_id_, false);
    }
  }
}

void PhysicalDisplay::VSyncControl(bool enabled) {
  display_queue_->VSyncControl(enabled);
}

bool PhysicalDisplay::CheckPlaneFormat(uint32_t format) {
  return display_queue_->CheckPlaneFormat(format);
}

void PhysicalDisplay::SetGamma(float red, float green, float blue) {
  display_queue_->SetGamma(red, green, blue);
}

void PhysicalDisplay::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  display_queue_->SetContrast(red, green, blue);
}

void PhysicalDisplay::SetBrightness(uint32_t red, uint32_t green,
                                    uint32_t blue) {
  display_queue_->SetBrightness(red, green, blue);
}

void PhysicalDisplay::SetExplicitSyncSupport(bool disable_explicit_sync) {
  display_queue_->SetExplicitSyncSupport(disable_explicit_sync);
}

bool PhysicalDisplay::PopulatePlanes(
    std::unique_ptr<DisplayPlane> & /*primary_plane*/,
    std::unique_ptr<DisplayPlane> & /*cursor_plane*/,
    std::vector<std::unique_ptr<DisplayPlane>> & /*overlay_planes*/) {
  ETRACE("PopulatePlanes unimplemented in PhysicalDisplay.");
  return false;
}

bool PhysicalDisplay::TestCommit(
    const std::vector<OverlayPlane> & /*commit_planes*/) const {
  ETRACE("TestCommit unimplemented in PhysicalDisplay.");
  return false;
}

void PhysicalDisplay::UpdateScalingRatio(uint32_t primary_width,
                                         uint32_t primary_height,
                                         uint32_t display_width,
                                         uint32_t display_height) {
  display_queue_->UpdateScalingRatio(primary_width, primary_height,
                                     display_width, display_height);
}

void PhysicalDisplay::CloneDisplay(NativeDisplay *source_display) {
  if (source_display_) {
    source_display_->DisOwnPresentation(this);
    display_queue_->SetCloneMode(false);
    source_display_ = NULL;
  }

  source_display_ = source_display;
  if (source_display_) {
    source_display_->OwnPresentation(this);
    display_queue_->SetCloneMode(true);
  }
}

void PhysicalDisplay::OwnPresentation(NativeDisplay *clone) {
  cloned_displays_.emplace_back(clone);
  display_state_ |= kRefreshClonedDisplays;
}

void PhysicalDisplay::DisOwnPresentation(NativeDisplay *clone) {
  if (cloned_displays_.empty())
    return;

  std::vector<NativeDisplay *> displays;
  size_t size = cloned_displays_.size();
  for (size_t i = 0; i < size; i++) {
    NativeDisplay *display = cloned_displays_.at(i);
    if (display == clone)
      continue;

    displays.emplace_back(display);
  }

  cloned_displays_.swap(displays);
  display_state_ |= kRefreshClonedDisplays;
}

void PhysicalDisplay::RefreshClones() {
  display_state_ &= ~kRefreshClonedDisplays;
  std::vector<NativeDisplay *>().swap(clones_);
  if (cloned_displays_.empty())
    return;

  size_t size = cloned_displays_.size();
  for (size_t i = 0; i < size; i++) {
    NativeDisplay *display = cloned_displays_.at(i);
    if (!display->IsConnected())
      continue;

    clones_.emplace_back(display);
  }

  uint32_t primary_width = Width();
  uint32_t primary_height = Height();
  for (auto display : clones_) {
    uint32_t display_width = display->Width();
    uint32_t display_height = display->Height();
    if ((primary_width == display_width) && (primary_height == display_height))
      return;

    display->UpdateScalingRatio(primary_width, primary_height, display_width,
                                display_height);
  }
}

}  // namespace hwcomposer
