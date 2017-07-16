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

namespace hwcomposer {

static const int32_t kUmPerInch = 25400;

PhysicalDisplay::PhysicalDisplay(uint32_t gpu_fd, uint32_t pipe_id)
    : pipe_(pipe_id),
      width_(0),
      height_(0),
      dpix_(0),
      dpiy_(0),
      gpu_fd_(gpu_fd),
      power_mode_(kOn),
      refresh_(0.0) {
}

PhysicalDisplay::~PhysicalDisplay() {
  display_queue_->SetPowerMode(kOff);
}

bool PhysicalDisplay::Initialize(NativeBufferHandler *buffer_handler) {
  display_queue_.reset(new DisplayQueue(gpu_fd_, false, buffer_handler, this));
  InitializeDisplay();
  return true;
}

void PhysicalDisplay::DisConnect() {
  IHOTPLUGEVENTTRACE("PhysicalDisplay::DisConnect recieved.");
  display_state_ |= kDisconnectionInProgress;
}

void PhysicalDisplay::Connect() {
  IHOTPLUGEVENTTRACE("PhysicalDisplay::Connect recieved.");
  display_state_ &= ~kDisconnectionInProgress;
  if (display_state_ & kConnected)
    return;

  modeset_lock_.lock();
  display_state_ |= kConnected;
  display_state_ &= ~kInitialized;

  if (!display_queue_->Initialize(refresh_, pipe_, width_, height_, this)) {
    ETRACE("Failed to initialize Display Queue.");
  } else {
    display_state_ |= kInitialized;
    if (hotplug_callback_) {
      hotplug_callback_->Callback(hot_plug_display_id_, true);
    }
  }

  UpdatePowerMode();
  modeset_lock_.unlock();
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
  config_ = config;
  display_queue_->DisplayConfigurationChanged();
  display_state_ |= kNeedsModeset;
  UpdateDisplayConfig();
  return true;
}

bool PhysicalDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  *config = config_;
  return true;
}

bool PhysicalDisplay::SetPowerMode(uint32_t power_mode) {
  if (power_mode_ == power_mode)
    return true;

  modeset_lock_.lock();
  // Don't update power mode in case disconnect is in
  // progress.
  if (!(display_state_ & kDisconnectionInProgress)) {
    power_mode_ = power_mode;
  } else if (power_mode == kOff) {
    display_state_ &= ~kConnected;
    if (hotplug_callback_) {
      hotplug_callback_->Callback(hot_plug_display_id_, false);
    }
  }
  modeset_lock_.unlock();

  if (!(display_state_ & kConnected)) {
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

  if (!(display_state_ & kUpdateDisplay)) {
    bool success = true;
    if (power_mode_ != kDozeSuspend) {
      ETRACE("Trying to update an Disconnected Display.%p \n", this);
      success = false;
    }

    modeset_lock_.unlock();
    return success;
  }

  modeset_lock_.unlock();

  return display_queue_->QueueUpdate(source_layers, retire_fence);
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
  if (hotplug_callback_) {
    if (display_state_ & kConnected) {
      hotplug_callback_->Callback(hot_plug_display_id_, true);
    } else {
      hotplug_callback_->Callback(hot_plug_display_id_, false);
    }
  }
  modeset_lock_.unlock();
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
  if ((primary_width == display_width) && (primary_height == display_height))
    return;
  modeset_lock_.lock();
  display_queue_->UpdateScalingRatio(primary_width, primary_height,
                                     display_width, display_height);
  modeset_lock_.unlock();
}

}  // namespace hwcomposer
