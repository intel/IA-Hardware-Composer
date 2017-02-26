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
      gpu_fd_(gpu_fd),
      is_connected_(false),
      is_powered_off_(true) {
}

Display::~Display() {
  display_queue_->Exit();
}

bool Display::Initialize() {
  frame_ = 0;
  flip_handler_.reset(new PageFlipEventHandler());
  display_queue_.reset(new DisplayQueue(gpu_fd_, crtc_id_));

  return true;
}

bool Display::Connect(const drmModeModeInfo &mode_info,
                      const drmModeConnector *connector,
                      NativeBufferHandler *buffer_handler) {
  IHOTPLUGEVENTTRACE("Display::Connect recieved.");
  // TODO(kalyan): Add support for multi monitor case.
  if (connector->connector_id == connector_ && !is_powered_off_) {
    IHOTPLUGEVENTTRACE("Display is already connected to this connector.");
    is_connected_ = true;
    return true;
  }

  IHOTPLUGEVENTTRACE("Display is being connected to a new connector.");
  connector_ = connector->connector_id;
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

  dpix_ = connector->mmWidth ? (width_ * kUmPerInch) / connector->mmWidth : -1;
  dpiy_ =
      connector->mmHeight ? (height_ * kUmPerInch) / connector->mmHeight : -1;

  is_powered_off_ = false;
  is_connected_ = true;

  if (!display_queue_->Initialize(width_, height_, pipe_, connector_, mode_info,
                                  buffer_handler)) {
    ETRACE("Failed to initialize Display Queue.");
    return false;
  }

  flip_handler_->Init(refresh_, gpu_fd_, pipe_);
  return true;
}

void Display::DisConnect() {
  IHOTPLUGEVENTTRACE("Display::DisConnect recieved.");
  is_connected_ = false;
}

void Display::ShutDown() {
  if (is_powered_off_)
    return;

  IHOTPLUGEVENTTRACE("Display::ShutDown recieved.");
  display_queue_->Exit();
  is_powered_off_ = true;
}

bool Display::GetDisplayAttribute(uint32_t /*config*/,
                                  HWCDisplayAttribute attribute,
                                  int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = width_;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = height_;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *value = 1e9 / refresh_;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = dpix_;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value = dpiy_;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

bool Display::GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
  *num_configs = 1;
  if (!configs)
    return true;

  configs[0] = 1;

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

bool Display::SetActiveConfig(uint32_t /*config*/) {
  return true;
}

bool Display::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 1;
  return true;
}

bool Display::SetDpmsMode(uint32_t dpms_mode) {
  return display_queue_->SetDpmsMode(dpms_mode);
}

bool Display::Present(std::vector<HwcLayer *> &source_layers) {
  CTRACE();
  return display_queue_->QueueUpdate(source_layers);
}

int Display::RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                   uint32_t display_id) {
  return flip_handler_->RegisterCallback(callback, display_id);
}

void Display::VSyncControl(bool enabled) {
  flip_handler_->VSyncControl(enabled);
}

}  // namespace hwcomposer
