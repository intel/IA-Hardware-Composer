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

#include "display.h"

#include <hwcdefs.h>
#include <hwclayer.h>
#include <hwctrace.h>

#include <algorithm>
#include <string>
#include <sstream>

#include "displayqueue.h"

namespace hwcomposer {

static const int32_t kUmPerInch = 25400;

Display::Display(uint32_t gpu_fd, uint32_t pipe_id, uint32_t crtc_id)
    : crtc_id_(crtc_id),
      pipe_(pipe_id),
      connector_(0),
      width_(0),
      height_(0),
      dpix_(0),
      dpiy_(0),
      gpu_fd_(gpu_fd),
      power_mode_(kOn),
      refresh_(0.0),
      is_connected_(false) {
}

Display::~Display() {
  display_queue_->SetPowerMode(kOff);
}

bool Display::Initialize(OverlayBufferManager *buffer_manager) {
  display_queue_.reset(new DisplayQueue(gpu_fd_, crtc_id_, buffer_manager));

  return true;
}

bool Display::Connect(const drmModeModeInfo &mode_info,
                      const drmModeConnector *connector) {
  IHOTPLUGEVENTTRACE("Display::Connect recieved.");
  // TODO(kalyan): Add support for multi monitor case.
  if (connector_ && connector->connector_id == connector_) {
    IHOTPLUGEVENTTRACE("Display is already connected to this connector.");
    is_connected_ = true;
    return true;
  }

  IHOTPLUGEVENTTRACE("Display is being connected to a new connector.");
  connector_ = connector->connector_id;
  mmWidth_ = connector->mmWidth;
  mmHeight_ = connector->mmHeight;
  SetDisplayAttribute(mode_info);

  is_connected_ = true;

  if (!display_queue_->Initialize(refresh_, width_, height_, pipe_, connector_,
                                  mode_info)) {
    ETRACE("Failed to initialize Display Queue.");
    return false;
  }

  if (!display_queue_->SetPowerMode(power_mode_)) {
    ETRACE("Failed to enable Display Queue.");
    return false;
  }

  return true;
}

void Display::DisConnect() {
  IHOTPLUGEVENTTRACE("Display::DisConnect recieved.");
  is_connected_ = false;
}

void Display::ShutDown() {
  if (!connector_)
    return;

  IHOTPLUGEVENTTRACE("Display::ShutDown recieved.");
  display_queue_->SetPowerMode(kOff);
  connector_ = 0;
}

bool Display::GetDisplayAttribute(uint32_t config /*config*/,
                                  HWCDisplayAttribute attribute,
                                  int32_t *value) {
  float refresh;
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = mode_[config].hdisplay;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = mode_[config].vdisplay;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      refresh = (mode_[config].clock * 1000.0f) /
                (mode_[config].htotal * mode_[config].vtotal);

      if (mode_[config].flags & DRM_MODE_FLAG_INTERLACE)
        refresh *= 2;

      if (mode_[config].flags & DRM_MODE_FLAG_DBLSCAN)
        refresh /= 2;

      if (mode_[config].vscan > 1)
        refresh /= mode_[config].vscan;
      // in nanoseconds
      *value = 1e9 / refresh;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = mmWidth_ ? (mode_[config].hdisplay * kUmPerInch) / mmWidth_ : -1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value =
          mmHeight_ ? (mode_[config].vdisplay * kUmPerInch) / mmHeight_ : -1;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

bool Display::GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
  *num_configs = mode_.size();
  if (!configs)
    return true;

  for (uint32_t i = 0; i < *num_configs; i++)
    configs[i] = i;

  return true;
}

bool Display::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Display-" << connector_;
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length - 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

int Display::GetDisplayPipe() {
  if (!is_connected_)
    return -1;
  return pipe_;
}

bool Display::SetActiveConfig(uint32_t config) {
  // update the activeConfig
  config_ = config;
  if (!display_queue_->SetActiveConfig(mode_[config_]))
    return false;

  SetDisplayAttribute(mode_[config_]);

  return true;
}

bool Display::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;
  *config = config_;
  return true;
}

bool Display::SetPowerMode(uint32_t power_mode) {
  if (power_mode_ == power_mode)
    return true;

  power_mode_ = power_mode;
  if (!is_connected_)
    return true;

  return display_queue_->SetPowerMode(power_mode);
}

bool Display::Present(std::vector<HwcLayer *> &source_layers,
                      int32_t *retire_fence) {
  CTRACE();

  if (!is_connected_ || power_mode_ != kOn) {
    IHOTPLUGEVENTTRACE("Trying to update an Disconnected Display.");
    return false;
  }

  return display_queue_->QueueUpdate(source_layers, retire_fence);
}

int Display::RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                   uint32_t display_id) {
  return display_queue_->RegisterVsyncCallback(callback, display_id);
}

void Display::VSyncControl(bool enabled) {
  display_queue_->VSyncControl(enabled);
}

bool Display::CheckPlaneFormat(uint32_t format) {
  return display_queue_->CheckPlaneFormat(format);
}

void Display::SetGamma(float red, float green, float blue) {
  display_queue_->SetGamma(red, green, blue);
}

void Display::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  display_queue_->SetContrast(red, green, blue);
}

void Display::SetBrightness(uint32_t red, uint32_t green, uint32_t blue) {
  display_queue_->SetBrightness(red, green, blue);
}

bool Display::SetBroadcastRGB(const char *range_property) {
  return display_queue_->SetBroadcastRGB(range_property);
}

void Display::SetExplicitSyncSupport(bool disable_explicit_sync) {
  display_queue_->SetExplicitSyncSupport(disable_explicit_sync);
}

void Display::SetDrmModeInfo(
    const std::vector<drmModeModeInfo> &mode_info) {
  for (uint32_t i = 0; i < mode_info.size(); ++i)
    mode_.emplace_back(mode_info[i]);
}

void Display::SetDisplayAttribute(const drmModeModeInfo &mode_info) {
  width_ = mode_info.hdisplay;
  height_ = mode_info.vdisplay;
  refresh_ =
      (mode_info.clock * 1000.0f) / (mode_info.htotal * mode_info.vtotal);

  if (mode_info.flags & DRM_MODE_FLAG_INTERLACE)
    refresh_ *= 2;

  if (mode_info.flags & DRM_MODE_FLAG_DBLSCAN)
    refresh_ /= 2;

  if (mode_info.vscan > 1)
    refresh_ /= mode_info.vscan;

  dpix_ = mmWidth_ ? (width_ * kUmPerInch) / mmWidth_ : -1;
  dpiy_ = mmHeight_ ? (height_ * kUmPerInch) / mmHeight_ : -1;
}

}  // namespace hwcomposer
