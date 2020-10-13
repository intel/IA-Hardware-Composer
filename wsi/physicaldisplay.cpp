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
#include <sstream>
#include <string>

#include "displayplanemanager.h"
#include "displayqueue.h"
#include "hwcutils.h"
#include "wsi_utils.h"

namespace hwcomposer {

PhysicalDisplay::PhysicalDisplay(uint32_t gpu_fd, uint32_t pipe_id)
    : pipe_(pipe_id),
      width_(0),
      height_(0),
      custom_resolution_(false),
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

const NativeBufferHandler *PhysicalDisplay::GetNativeBufferHandler() const {
  if (display_queue_) {
    return display_queue_->GetNativeBufferHandler();
  }

  return NULL;
}

void PhysicalDisplay::MarkForDisconnect() {
  SPIN_LOCK(modeset_lock_);

  IHOTPLUGEVENTTRACE("PhysicalDisplay::MarkForDisconnect recieved.");
  connection_state_ |= kDisconnectionInProgress;
  display_state_ |= kRefreshClonedDisplays;
  SPIN_UNLOCK(modeset_lock_);
}

void PhysicalDisplay::NotifyClientOfConnectedState() {
  SPIN_LOCK(modeset_lock_);
  bool refresh_needed = false;
  if (hotplug_callback_ && (connection_state_ & kConnected) &&
      (display_state_ & kNotifyClient)) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay Sent Hotplug even call back with connected value set "
        "to true. %p hotplugdisplayid: %d \n",
        this, hot_plug_display_id_);
    hotplug_callback_->Callback(hot_plug_display_id_, true);
    display_state_ &= ~kNotifyClient;
#ifdef ENABLE_ANDROID_WA
    if (ordered_display_id_ == 0) {
      refresh_needed = true;
    }
#endif
  }
  SPIN_UNLOCK(modeset_lock_);

  if (refresh_needed) {
    if (!display_queue_->IsIgnoreUpdates()) {
      display_queue_->ForceRefresh();
    }
  }
}

void PhysicalDisplay::NotifyClientOfDisConnectedState() {
  SPIN_LOCK(modeset_lock_);
  if (hotplug_callback_ && !(connection_state_ & kConnected) &&
      (display_state_ & kNotifyClient)) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay Sent Hotplug even call back with connected value set "
        "to false. %p hotplugdisplayid: %d \n",
        this, hot_plug_display_id_);
    hotplug_callback_->Callback(hot_plug_display_id_, false);
    display_state_ &= ~kNotifyClient;
  }
  SPIN_UNLOCK(modeset_lock_);
}

void PhysicalDisplay::DisConnect() {
  SPIN_LOCK(modeset_lock_);

  connection_state_ &= ~kDisconnectionInProgress;

  if (!(connection_state_ & kConnected)) {
    SPIN_UNLOCK(modeset_lock_);

    return;
  }
  IHOTPLUGEVENTTRACE(
      "PhysicalDisplay DisConnect called for Display: %p hotplugdisplayid: %d "
      "\n",
      this, hot_plug_display_id_);
  display_state_ |= kNotifyClient;

  if (power_mode_ != kOff) {
    display_queue_->SetPowerMode(kOff);
  }

  connection_state_ &= ~kConnected;
  display_state_ &= ~kUpdateDisplay;
  SPIN_UNLOCK(modeset_lock_);
}

void PhysicalDisplay::Connect() {
  SPIN_LOCK(modeset_lock_);

  connection_state_ &= ~kDisconnectionInProgress;

  SPIN_UNLOCK(modeset_lock_);

  if (source_display_) {
    // Current display is a cloned display, set the source_display_'s
    // k_RefreshClonedDisplays flag. This makes clone parent have a
    // chance to update it's cloned display list
    PhysicalDisplay *p_clone_parent = (PhysicalDisplay *)source_display_;
    SPIN_LOCK(p_clone_parent->modeset_lock_);
    p_clone_parent->display_state_ |= kRefreshClonedDisplays;
    SPIN_UNLOCK(p_clone_parent->modeset_lock_);
  }

  SPIN_LOCK(modeset_lock_);
  if (connection_state_ & kConnected) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay::Connect connected already, return with power mode "
        "update.");
    UpdatePowerMode();
    SPIN_UNLOCK(modeset_lock_);
    return;
  }

  connection_state_ |= kConnected;
  display_state_ &= ~kInitialized;
  display_state_ |= kNotifyClient;
  IHOTPLUGEVENTTRACE("PhysicalDisplay::Connect recieved. %p \n", this);

  if (!display_queue_->Initialize(pipe_, width_, height_, this)) {
    ETRACE("Failed to initialize Display Queue.");
  } else {
    display_state_ |= kInitialized;
  }

  if (display_state_ & kUpdateConfig) {
    display_state_ &= ~kUpdateConfig;
    display_queue_->DisplayConfigurationChanged();
    UpdateDisplayConfig();
  }

  UpdatePowerMode();

  SPIN_UNLOCK(modeset_lock_);
}

bool PhysicalDisplay::IsConnected() const {
  if (connection_state_ & kDisconnectionInProgress)
    return false;

  return connection_state_ & kConnected;
}

uint32_t PhysicalDisplay::PowerMode() const {
  return power_mode_;
}

int PhysicalDisplay::GetDisplayPipe() {
  return pipe_;
}

bool PhysicalDisplay::EnableDRMCommit(bool enable) {
  display_queue_->ForceIgnoreUpdates(!enable);
  if (enable)
    return !display_queue_->IsIgnoreUpdates();
  else
    return display_queue_->IsIgnoreUpdates();
}

bool PhysicalDisplay::SetActiveConfig(uint32_t config) {
  // update the activeConfig
  IHOTPLUGEVENTTRACE(
      "SetActiveConfig: New config to be used %d pipe: %p display: %p", config,
      pipe_, this);
  config_ = config;
  display_state_ |= kNeedsModeset;
  if (connection_state_ & kConnected) {
    display_queue_->DisplayConfigurationChanged();
    UpdateDisplayConfig();
  } else {
    display_state_ |= kUpdateConfig;
  }

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
#ifndef DISABLE_HOTPLUG_NOTIFICATION
  ScopedSpinLock lock(modeset_lock_);
#endif

  if (power_mode_ == power_mode) {
    return true;
  }

  power_mode_ = power_mode;
  if (!(connection_state_ & kConnected)) {
    IHOTPLUGEVENTTRACE(
        "PhysicalDisplay is not connected, postponing power mode update.");
    display_state_ |= kPendingPowerMode;
    return true;
  } else if (connection_state_ & kDisconnectionInProgress) {
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
    IHOTPLUGEVENTTRACE(
        "UpdatePowerMode: Powering on Display: pipe: %d display: "
        "%p",
        pipe_, this);
    PowerOn();
  } else {
    IHOTPLUGEVENTTRACE(
        "UpdatePowerMode: Power mode is not kOn: pipe: %d display: "
        "%p",
        pipe_, this);
    display_state_ &= ~kUpdateDisplay;
  }

  if (!(display_state_ & kInitialized))
    return true;

  return display_queue_->SetPowerMode(power_mode_);
}

bool PhysicalDisplay::Present(std::vector<HwcLayer *> &source_layers,
                              int32_t *retire_fence,
                              PixelUploaderCallback *call_back,
                              bool handle_constraints) {
  CTRACE();
  SPIN_LOCK(modeset_lock_);

  bool handle_hotplug_notifications = false;
  if (display_state_ & kHandlePendingHotPlugNotifications) {
    display_state_ &= ~kHandlePendingHotPlugNotifications;
    handle_hotplug_notifications = true;
  }

  if (!(display_state_ & kUpdateDisplay)) {
    bool success = true;
    if (power_mode_ != kDozeSuspend) {
      ETRACE("Trying to update an Disconnected Display.%p \n", this);
    }

    SPIN_UNLOCK(modeset_lock_);

    if (handle_hotplug_notifications) {
      NotifyClientsOfDisplayChangeStatus();
    }

    return success;
  }

  if (source_display_) {
    ETRACE("Trying to update display independently when in cloned mode.%p \n",
           this);
  }

  if (display_state_ & kRefreshClonedDisplays) {
    RefreshClones();
  }

  SPIN_UNLOCK(modeset_lock_);

  if (handle_hotplug_notifications) {
    NotifyClientsOfDisplayChangeStatus();
    IHOTPLUGEVENTTRACE("Handle_hoplug_notifications done. %p \n", this);
  }

  bool ignore_clone_update = false;
  bool success = display_queue_->QueueUpdate(source_layers, retire_fence,
                                             &ignore_clone_update, call_back,
                                             handle_constraints);
  if (success && !clones_.empty() && !ignore_clone_update) {
    HandleClonedDisplays(this);
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

bool PhysicalDisplay::PresentClone(NativeDisplay *display) {
  CTRACE();
  SPIN_LOCK(modeset_lock_);

  if (display_state_ & kRefreshClonedDisplays) {
    RefreshClones();
  }
  SPIN_UNLOCK(modeset_lock_);

  display_queue_->PresentClonedCommit(
      static_cast<PhysicalDisplay *>(display)->display_queue_.get());
  HandleClonedDisplays(display);
  return true;
}

void PhysicalDisplay::HandleClonedDisplays(NativeDisplay *display) {
  if (clones_.empty())
    return;

  for (auto clone_display : clones_) {
    clone_display->PresentClone(display);
  }
}

int PhysicalDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display_id) {
  return display_queue_->RegisterVsyncCallback(callback, display_id);
}

int PhysicalDisplay::RegisterVsyncPeriodCallback(
    std::shared_ptr<VsyncPeriodCallback> callback, uint32_t display_id) {
  return display_queue_->RegisterVsyncPeriodCallback(callback, display_id);
}

void PhysicalDisplay::RegisterRefreshCallback(
    std::shared_ptr<RefreshCallback> callback, uint32_t display_id) {
  display_queue_->RegisterRefreshCallback(callback, display_id);
}

void PhysicalDisplay::RegisterHotPlugCallback(
    std::shared_ptr<HotPlugCallback> callback, uint32_t display_id) {
  SPIN_LOCK(modeset_lock_);
  hot_plug_display_id_ = display_id;
  hotplug_callback_ = callback;
#ifndef ENABLE_ANDROID_WA
  bool connected = connection_state_ & kConnected;
#endif
  SPIN_UNLOCK(modeset_lock_);
#ifdef ENABLE_ANDROID_WA
  if (hotplug_callback_ && ordered_display_id_ == 0) {
    display_state_ &= ~kNotifyClient;
    display_state_ |= kHandlePendingHotPlugNotifications;
    IHOTPLUGEVENTTRACE("RegisterHotPlugCallback: pipe: %d display: %p", pipe_,
                       this);
    hotplug_callback_->Callback(hot_plug_display_id_, true);
  }
#else
  if (hotplug_callback_) {
    if (connected) {
      hotplug_callback_->Callback(hot_plug_display_id_, true);
    } else {
      hotplug_callback_->Callback(hot_plug_display_id_, false);
    }
  }
#endif
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

void PhysicalDisplay::SetColorTransform(const float *matrix,
                                        HWCColorTransform hint) {
  display_queue_->SetColorTransform(matrix, hint);
}

void PhysicalDisplay::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  display_queue_->SetContrast(red, green, blue);
}

void PhysicalDisplay::SetBrightness(uint32_t red, uint32_t green,
                                    uint32_t blue) {
  display_queue_->SetBrightness(red, green, blue);
}

void PhysicalDisplay::SetDisableExplicitSync(bool disable_explicit_sync) {
  display_queue_->SetDisableExplicitSync(disable_explicit_sync);
}

void PhysicalDisplay::SetVideoScalingMode(uint32_t mode) {
  display_queue_->SetVideoScalingMode(mode);
}

void PhysicalDisplay::SetVideoColor(HWCColorControl color, float value) {
  display_queue_->SetVideoColor(color, value);
}

void PhysicalDisplay::GetVideoColor(HWCColorControl color, float *value,
                                    float *start, float *end) {
  display_queue_->GetVideoColor(color, value, start, end);
}

void PhysicalDisplay::RestoreVideoDefaultColor(HWCColorControl color) {
  display_queue_->RestoreVideoDefaultColor(color);
}

void PhysicalDisplay::SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                                          HWCDeinterlaceControl mode) {
  display_queue_->SetVideoDeinterlace(flag, mode);
}

void PhysicalDisplay::RestoreVideoDefaultDeinterlace() {
  display_queue_->RestoreVideoDefaultDeinterlace();
}

void PhysicalDisplay::SetCanvasColor(uint16_t bpc, uint16_t red, uint16_t green,
                                     uint16_t blue, uint16_t alpha) {
  display_queue_->SetCanvasColor(bpc, red, green, blue, alpha);
}

void PhysicalDisplay::SetPAVPSessionStatus(bool enabled,
                                           uint32_t pavp_session_id,
                                           uint32_t pavp_instance_id) {
  display_queue_->SetPAVPSessionStatus(enabled, pavp_session_id,
                                       pavp_instance_id);
}

bool PhysicalDisplay::PopulatePlanes(
    std::vector<std::unique_ptr<DisplayPlane>> & /*overlay_planes*/) {
  ETRACE("PopulatePlanes unimplemented in PhysicalDisplay.");
  return false;
}

bool PhysicalDisplay::TestCommit(
    const DisplayPlaneStateList & /*commit_planes*/) const {
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

void PhysicalDisplay::SetDisplayOrder(uint32_t display_order) {
  ordered_display_id_ = display_order;
}

void PhysicalDisplay::RotateDisplay(HWCRotation rotation) {
  display_queue_->RotateDisplay(rotation);
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

bool PhysicalDisplay::GetDisplayAttribute(uint32_t /*config*/,
                                          HWCDisplayAttribute attribute,
                                          int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = 1920;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = 1080;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *value = 16666666;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = 1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value = 1;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

/* Setting custom resolution instead of preferred as fetched from display */
bool PhysicalDisplay::SetCustomResolution(const HwcRect<int32_t> &rect) {
  if ((rect.right - rect.left) && (rect.bottom - rect.top)) {
    // Custom rectangle with valid width and height
    rect_.left = rect.left;
    rect_.top = rect.top;
    rect_.right = rect.right;
    rect_.bottom = rect.bottom;
    custom_resolution_ = true;

    IHOTPLUGEVENTTRACE(
        "SetCustomResolution: custom width %d, height %d, bool %d",
        rect_.right - rect_.left, rect_.bottom - rect_.top, custom_resolution_);

    return true;
  } else {
    // Default display rectangle
    custom_resolution_ = false;
    return false;
  }
}

bool PhysicalDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                        uint32_t *configs) {
  if (!num_configs)
    return false;
  *num_configs = 1;
  if (configs) {
    configs[0] = DEFAULT_CONFIG_ID;
  }
  connection_state_ |= kFakeConnected;
  return true;
}

bool PhysicalDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Headless";
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length + 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

int PhysicalDisplay::GetTotalOverlays() const {
  if (display_queue_)
    return display_queue_->GetTotalOverlays();
  else
    return 0;
}

bool PhysicalDisplay::IsBypassClientCTM() const {
  return bypassClientCTM_;
}

void PhysicalDisplay::GetDisplayCapabilities(uint32_t *numCapabilities,
                                             uint32_t *capabilities) {
}

}  // namespace hwcomposer
