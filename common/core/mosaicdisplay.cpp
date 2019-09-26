/*
// Copyright (c) 2017 Intel Corporation
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

#include "mosaicdisplay.h"

#include <libsync.h>
#include <sstream>
#include <string>

#include <hwclayer.h>

#include "hwctrace.h"

#ifdef ENABLE_PANORAMA
#include "displaymanager.h"
#include "virtualpanoramadisplay.h"
#endif

namespace hwcomposer {

class MDVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  MDVsyncCallback(MosaicDisplay *display) : display_(display) {
  }

  void Callback(uint32_t /*display*/, int64_t timestamp) {
    display_->VSyncUpdate(timestamp);
  }

 private:
  MosaicDisplay *display_;
};

class MDRefreshCallback : public hwcomposer::RefreshCallback {
 public:
  MDRefreshCallback(MosaicDisplay *display) : display_(display) {
  }

  void Callback(uint32_t /*display*/) {
    display_->RefreshUpdate();
  }

 private:
  MosaicDisplay *display_;
};

class MDHotPlugCallback : public hwcomposer::HotPlugCallback {
 public:
  MDHotPlugCallback(MosaicDisplay *display) : display_(display) {
  }

  void Callback(uint32_t /*display*/, bool connected) {
    display_->HotPlugUpdate(connected);
  }

 private:
  MosaicDisplay *display_;
};

MosaicDisplay::MosaicDisplay(const std::vector<NativeDisplay *> &displays)
    : dpix_(0), dpiy_(0) {
  uint32_t size = displays.size();
  physical_displays_.reserve(size);
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.emplace_back(displays.at(i));
  }
#ifdef ENABLE_PANORAMA
  event_.Initialize();
#endif
}

MosaicDisplay::~MosaicDisplay() {
#ifdef ENABLE_PANORAMA
  if (panorama_mode_) {
    while (!virtual_panorama_displays_->empty()) {
      NativeDisplay *ptr_display = virtual_panorama_displays_->back();
      delete ptr_display;
    }
    while (!physical_panorama_displays_->empty()) {
      NativeDisplay *ptr_display = physical_panorama_displays_->back();
      delete ptr_display;
    }
  } else {
    while (!physical_displays_.empty()) {
      NativeDisplay *ptr_display = physical_displays_.back();
      physical_displays_.pop_back();
      delete ptr_display;
    }
  }
#endif
}

bool MosaicDisplay::Initialize(NativeBufferHandler * /*buffer_handler*/) {
  return true;
}

bool MosaicDisplay::IsConnected() const {
  uint32_t size = physical_displays_.size();
  bool connected = false;
  for (uint32_t i = 0; i < size; i++) {
    if (physical_displays_.at(i)->IsConnected()) {
      connected = true;
      break;
    }
  }

  return connected;
}

uint32_t MosaicDisplay::Width() const {
  return width_;
}

uint32_t MosaicDisplay::Height() const {
  return height_;
}

uint32_t MosaicDisplay::PowerMode() const {
  return power_mode_;
}

int MosaicDisplay::GetDisplayPipe() {
  return physical_displays_.at(0)->GetDisplayPipe();
}

bool MosaicDisplay::EnableDRMCommit(bool enable) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++)
    physical_displays_.at(i)->EnableDRMCommit(enable);
  return true;
}

bool MosaicDisplay::SetActiveConfig(uint32_t config) {
  config_ = config;
  width_ = 0;
  height_ = 0;
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetActiveConfig(config_);
  }

  uint32_t avg = 0;
  for (uint32_t i = 0; i < size; i++) {
    int32_t dpix = 0;
    int32_t dpiy = 0;
    int32_t refresh = 0;
    height_ = std::max(height_, physical_displays_.at(i)->Height());
    width_ += physical_displays_.at(i)->Width();
    physical_displays_.at(i)->GetDisplayAttribute(
        config_, HWCDisplayAttribute::kDpiX, &dpix);
    physical_displays_.at(i)->GetDisplayAttribute(
        config_, HWCDisplayAttribute::kDpiY, &dpiy);
    physical_displays_.at(i)->GetDisplayAttribute(
        config_, HWCDisplayAttribute::kRefreshRate, &refresh);
    dpix_ += dpix;
    dpiy_ += dpiy;
    refresh_ += refresh;

    avg++;
  }

  if (avg > 0) {
    refresh_ /= avg;
    dpix_ /= avg;
    dpiy_ /= avg;
  }

  return true;
}

bool MosaicDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  *config = config_;
  return true;
}

bool MosaicDisplay::SetPowerMode(uint32_t power_mode) {
  power_mode_ = power_mode;
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetPowerMode(power_mode);
  }

  return true;
}

bool MosaicDisplay::Present(std::vector<HwcLayer *> &source_layers,
                            int32_t *retire_fence,
                            PixelUploaderCallback *call_back,
                            bool /*handle_constraints*/) {
  if (power_mode_ != kOn) {
#ifdef ENALBE_PANORAMA
    if (skip_update_) {
      event_.Signal();
    }
#endif
    return true;
  }

#ifdef ENABLE_PANORAMA
  if (skip_update_) {
    return true;
  }

  under_present = true;
#endif
  lock_.lock();

#ifdef ENABLE_PANORAMA
  uint32_t curr_displays_num = connected_displays_.size();
  uint32_t physical_displays_num = physical_displays_.size();

  if (curr_displays_num != physical_displays_num) {
    update_connected_displays_ = true;
  }
#endif

  if (update_connected_displays_) {
    std::vector<NativeDisplay *>().swap(connected_displays_);
    uint32_t size = physical_displays_.size();
    for (uint32_t i = 0; i < size; i++) {
      if (physical_displays_.at(i)->IsConnected()) {
        connected_displays_.emplace_back(physical_displays_.at(i));
      }
    }
    update_connected_displays_ = false;
  }
  lock_.unlock();
  uint32_t size = connected_displays_.size();
  int32_t left_constraint = 0;
#ifdef ENABLE_PANORAMA
  if (panorama_mode_ && !panorama_enabling_state_) {
    left_constraint += total_width_virtual_ / 2;
  }
#endif
  size_t total_layers = source_layers.size();
  int32_t fence = -1;
  *retire_fence = -1;
  for (uint32_t i = 0; i < size; i++) {
    NativeDisplay *display = connected_displays_.at(i);
    int32_t right_constraint = left_constraint + display->Width();
    std::vector<HwcLayer *> layers;
    uint32_t dlconstraint = display->GetLogicalIndex() * display->Width();
    uint32_t drconstraint = dlconstraint + display->Width();
    IMOSAICDISPLAYTRACE("Display index %d \n", i);
    IMOSAICDISPLAYTRACE("dlconstraint %d \n", dlconstraint);
    IMOSAICDISPLAYTRACE("drconstraint %d \n", drconstraint);
    IMOSAICDISPLAYTRACE("right_constraint %d \n", right_constraint);
    IMOSAICDISPLAYTRACE("left_constraint %d \n", left_constraint);
    for (size_t j = 0; j < total_layers; j++) {
      HwcLayer *layer = source_layers.at(j);
      const HwcRect<int> &frame_Rect = layer->GetDisplayFrame();
      if ((frame_Rect.right < left_constraint) ||
          (frame_Rect.left > right_constraint)) {
        continue;
      }

      layer->SetUseForMosaic(true);
      layer->SetLeftConstraint(dlconstraint);
      layer->SetRightConstraint(drconstraint);
      layer->SetLeftSourceConstraint(left_constraint);
      layer->SetRightSourceConstraint(right_constraint);

      layers.emplace_back(layer);
    }

    if (layers.empty()) {
      continue;
    }

    display->Present(layers, &fence, call_back, true);
    IMOSAICDISPLAYTRACE("Present called for Display index %d \n", i);
    if (fence > 0) {
      if (*retire_fence < 0) {
        *retire_fence = fence;
      } else {
        int ret = sync_accumulate("iahwc_mosaic_fence", retire_fence, fence);
        if (ret) {
          ETRACE("Unable to merge fences");
          *retire_fence = -1;
        }
        close(fence);
      }
    }

    left_constraint = right_constraint;
  }

#ifdef ENABLE_PANORAMA
  if (skip_update_) {
    event_.Signal();
  }
  under_present = false;
#endif

  return true;
}

bool MosaicDisplay::PresentClone(NativeDisplay * /*display*/) {
  return false;
}

int MosaicDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  vsync_callback_ = callback;

  uint32_t size = physical_displays_.size();
  auto v_callback = std::make_shared<MDVsyncCallback>(this);
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->RegisterVsyncCallback(
        v_callback, physical_displays_.at(i)->GetDisplayPipe());
  }

  return 0;
}

void MosaicDisplay::RegisterRefreshCallback(
    std::shared_ptr<RefreshCallback> callback, uint32_t display_id) {
  display_id_ = display_id;
  refresh_callback_ = callback;

  uint32_t size = physical_displays_.size();
  auto r_callback = std::make_shared<MDRefreshCallback>(this);
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->RegisterRefreshCallback(
        r_callback, physical_displays_.at(i)->GetDisplayPipe());
  }
}

void MosaicDisplay::RegisterHotPlugCallback(
    std::shared_ptr<HotPlugCallback> callback, uint32_t display_id) {
  lock_.lock();
  display_id_ = display_id;
  hotplug_callback_ = callback;
  lock_.unlock();

  uint32_t size = physical_displays_.size();
  auto h_callback = std::make_shared<MDHotPlugCallback>(this);
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->RegisterHotPlugCallback(
        h_callback, physical_displays_.at(i)->GetDisplayPipe());
  }
}

void MosaicDisplay::VSyncControl(bool enabled) {
  if (enable_vsync_ == enabled)
    return;

  enable_vsync_ = enabled;
  vsync_timestamp_ = 0;
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->VSyncControl(enabled);
  }
}

void MosaicDisplay::VSyncUpdate(int64_t timestamp) {
  lock_.lock();
  if (vsync_callback_ && enable_vsync_ && vsync_divisor_ > 0) {
    vsync_counter_--;
    vsync_timestamp_ += timestamp;
    if (vsync_counter_ == 0) {
      vsync_timestamp_ /= vsync_divisor_;
      vsync_callback_->Callback(display_id_, vsync_timestamp_);
      vsync_counter_ = vsync_divisor_;
      vsync_timestamp_ = 0;
      pending_vsync_ = false;
    } else {
      pending_vsync_ = true;
    }
  }

  lock_.unlock();
}

void MosaicDisplay::RefreshUpdate() {
  if (connected_ && refresh_callback_ && power_mode_ == kOn) {
    refresh_callback_->Callback(display_id_);
  }
}

void MosaicDisplay::HotPlugUpdate(bool connected) {
  lock_.lock();
  update_connected_displays_ = true;
  uint32_t total_connected_displays = 0;
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    if (physical_displays_.at(i)->IsConnected()) {
      total_connected_displays++;
    }
  }

  if (vsync_callback_ && enable_vsync_ && pending_vsync_ &&
      (total_connected_displays > 0)) {
    if (vsync_counter_ == total_connected_displays) {
      vsync_timestamp_ /= total_connected_displays;
      vsync_callback_->Callback(display_id_, vsync_timestamp_);
      pending_vsync_ = false;
    }
  }

  vsync_counter_ = total_connected_displays;

#ifdef ENABLE_PANORAMA
  if (panorama_mode_) {
    vsync_divisor_ = num_physical_displays_;
    if (vsync_divisor_ < 1) {
      vsync_divisor_ = 1;
    }
  } else {
    vsync_divisor_ = vsync_counter_;
  }
#else
  vsync_divisor_ = vsync_counter_;
#endif

#ifdef ENABLE_PANORAMA
  if (!panorama_mode_) {
    if (connected_ == connected) {
      lock_.unlock();
      return;
    }
  }
#else
  if (connected_ == connected) {
    lock_.unlock();
    return;
  }
#endif

  if (hotplug_callback_) {
#ifdef ENABLE_PANORAMA
    if (!panorama_mode_) {
      if (!connected && connected_ && total_connected_displays) {
        lock_.unlock();
        return;
      }
    }
#else
    if (!connected && connected_ && total_connected_displays) {
      lock_.unlock();
      return;
    }
#endif
    connected_ = connected;
    hotplug_callback_->Callback(display_id_, connected);
  }
  lock_.unlock();
}

bool MosaicDisplay::CheckPlaneFormat(uint32_t format) {
  return physical_displays_.at(0)->CheckPlaneFormat(format);
}

void MosaicDisplay::SetGamma(float red, float green, float blue) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetGamma(red, green, blue);
  }
}

void MosaicDisplay::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetContrast(red, green, blue);
  }
}

void MosaicDisplay::SetBrightness(uint32_t red, uint32_t green, uint32_t blue) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetBrightness(red, green, blue);
  }
}

void MosaicDisplay::SetDisableExplicitSync(bool disable_explicit_sync) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetDisableExplicitSync(disable_explicit_sync);
  }
}

void MosaicDisplay::SetVideoScalingMode(uint32_t mode) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetVideoScalingMode(mode);
  }
}

void MosaicDisplay::SetVideoColor(HWCColorControl color, float value) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetVideoColor(color, value);
  }
}

void MosaicDisplay::GetVideoColor(HWCColorControl color, float *value,
                                  float *start, float *end) {
  physical_displays_.at(0)->GetVideoColor(color, value, start, end);
}

void MosaicDisplay::RestoreVideoDefaultColor(HWCColorControl color) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->RestoreVideoDefaultColor(color);
  }
}

void MosaicDisplay::SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                                        HWCDeinterlaceControl mode) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetVideoDeinterlace(flag, mode);
  }
}

void MosaicDisplay::RestoreVideoDefaultDeinterlace() {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->RestoreVideoDefaultDeinterlace();
  }
}

void MosaicDisplay::UpdateScalingRatio(uint32_t /*primary_width*/,
                                       uint32_t /*primary_height*/,
                                       uint32_t /*display_width*/,
                                       uint32_t /*display_height*/) {
}

void MosaicDisplay::CloneDisplay(NativeDisplay * /*source_display*/) {
}

bool MosaicDisplay::GetDisplayAttribute(uint32_t /*config*/,
                                        HWCDisplayAttribute attribute,
                                        int32_t *value) {
  bool status = true;
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = width_;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = height_;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      *value = refresh_;
      break;
    case HWCDisplayAttribute::kDpiX:
      *value = dpix_;
      break;
    case HWCDisplayAttribute::kDpiY:
      *value = dpiy_;
      break;
    default:
      *value = -1;
      status = false;
  }

  return status;
}

bool MosaicDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                      uint32_t *configs) {
  if (!num_configs)
    return false;
  *num_configs = 1;
  if (configs) {
    configs[0] = 0;
  }
  return true;
}

bool MosaicDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
#ifdef ENABLE_PANORAMA
  if (panorama_mode_) {
    stream << "Panorama";
  } else {
    stream << "Mosaic";
  }
#else
  stream << "Mosaic";
#endif
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

bool MosaicDisplay::GetDisplayIdentificationData(uint8_t *outPort,
                                                 uint32_t *outDataSize,
                                                 uint8_t *outData) {
  return true;
}

bool MosaicDisplay::IsBypassClientCTM() const {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    if (!physical_displays_.at(i)->IsBypassClientCTM()) {
      return false;
    }
  }
  return true;
}

void MosaicDisplay::GetDisplayCapabilities(uint32_t *numCapabilities,
                                           uint32_t *capabilities) {
  if (IsBypassClientCTM()) {
    ++*numCapabilities;
    *capabilities |= static_cast<uint32_t>(
        HWCDisplayCapability::kDisplayCapabilitySkipClientColorTransform);
  }
}

void MosaicDisplay::SetHDCPState(HWCContentProtection state,
                                 HWCContentType content_type) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetHDCPState(state, content_type);
  }
}

void MosaicDisplay::SetHDCPSRM(const int8_t *SRM, uint32_t SRMLength) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    physical_displays_.at(i)->SetHDCPSRM(SRM, SRMLength);
  }
}

bool MosaicDisplay::ContainConnector(const uint32_t connector_id) {
  uint32_t size = physical_displays_.size();
  for (uint32_t i = 0; i < size; i++) {
    if (physical_displays_.at(i)->ContainConnector(connector_id))
      return true;
  }
  return false;
}

#ifdef ENABLE_PANORAMA
void MosaicDisplay::SetPanoramaMode(bool mode) {
  panorama_lock_.lock();
  panorama_mode_ = mode;
  if (panorama_mode_)
    panorama_enabling_state_ = true;
  panorama_lock_.unlock();
}

void MosaicDisplay::SetExtraDispInfo(
    std::vector<NativeDisplay *> *virtual_panorama_displays,
    std::vector<NativeDisplay *> *physical_panorama_displays) {
  virtual_panorama_displays_ = virtual_panorama_displays;
  physical_panorama_displays_ = physical_panorama_displays;

  num_physical_displays_ = physical_panorama_displays->size();

  num_virtual_displays_ = virtual_panorama_displays->size();

  for (uint32_t i = 0; i < physical_panorama_displays->size(); i++) {
    real_physical_displays_.emplace_back(physical_panorama_displays->at(i));
    total_width_physical_ += physical_panorama_displays->at(i)->Width();
  }

  for (uint32_t i = 0; i < virtual_panorama_displays->size(); i++) {
    total_width_virtual_ += virtual_panorama_displays->at(i)->Width();
  }
}

bool MosaicDisplay::TriggerPanorama(uint32_t hotplug_simulation) {
  bool ret = true;

  if (!panorama_mode_) {
    ret = false;
  } else {
    panorama_lock_.lock();
    if (!panorama_enabling_state_) {
      panorama_enabling_state_ = true;
    } else {
      ETRACE("Panorama mode already enabled!");
      ret = false;
    }
    panorama_lock_.unlock();
  }

  if (!ret) {
    return ret;
  }

  skip_update_ = true;
  if (under_present) {
    event_.Wait();
  }
  lock_.lock();
  physical_displays_.swap(real_physical_displays_);
  lock_.unlock();

  SetActiveConfig(0);

  uint32_t size = virtual_panorama_displays_->size();
  for (uint32_t i = 0; i < size; i++) {
    VirtualPanoramaDisplay *ppdisplay =
        (VirtualPanoramaDisplay *)virtual_panorama_displays_->at(i);
    ppdisplay->SetHyperDmaBufMode(1);
  }

  update_connected_displays_ = true;
  if (hotplug_simulation) {
    HotPlugUpdate(false);
    HotPlugUpdate(true);
  }
  skip_update_ = false;
  return ret;
}

bool MosaicDisplay::ShutdownPanorama(uint32_t hotplug_simulation) {
  bool ret = true;

  if (!panorama_mode_) {
    ret = false;
  } else {
    panorama_lock_.lock();
    if (panorama_enabling_state_) {
      panorama_enabling_state_ = false;
    } else {
      ETRACE("Panorama mode already disabled!");
      ret = false;
    }
    panorama_lock_.unlock();
  }

  if (!ret) {
    return ret;
  }

  skip_update_ = true;
  if (under_present) {
    event_.Wait();
  }
  lock_.lock();

  uint32_t size = virtual_panorama_displays_->size();
  for (uint32_t i = 0; i < size; i++) {
    VirtualPanoramaDisplay *ppdisplay =
        (VirtualPanoramaDisplay *)virtual_panorama_displays_->at(i);
    ppdisplay->SetHyperDmaBufMode(0);
  }

  physical_displays_.swap(real_physical_displays_);
  lock_.unlock();

  SetActiveConfig(0);

  update_connected_displays_ = true;
  if (hotplug_simulation) {
    HotPlugUpdate(false);
    HotPlugUpdate(true);
  }
  skip_update_ = false;
  return ret;
}

#endif

}  // namespace hwcomposer
