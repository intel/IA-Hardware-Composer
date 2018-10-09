/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "iahwc2.h"
#include "utils_android.h"

#include <inttypes.h>

#include <android/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <system/graphics.h>

#include <gpudevice.h>
#include <hwcdefs.h>
#include <nativedisplay.h>
#include "mosaicdisplay.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace android {

class IAVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  IAVsyncCallback(hwc2_callback_data_t data, hwc2_function_pointer_t hook)
      : data_(data), hook_(hook) {
  }

  void Callback(uint32_t display, int64_t timestamp) {
    if (hook_ != NULL) {
      auto hook = reinterpret_cast<HWC2_PFN_VSYNC>(hook_);
      hook(data_, display, timestamp);
    }
  }

 private:
  hwc2_callback_data_t data_;
  hwc2_function_pointer_t hook_;
};

class IARefreshCallback : public hwcomposer::RefreshCallback {
 public:
  IARefreshCallback(hwc2_callback_data_t data, hwc2_function_pointer_t hook)
      : data_(data), hook_(hook) {
  }

  void Callback(uint32_t display) {
    if (hook_ != NULL) {
      auto hook = reinterpret_cast<HWC2_PFN_REFRESH>(hook_);
      hook(data_, display);
    }
  }

 private:
  hwc2_callback_data_t data_;
  hwc2_function_pointer_t hook_;
};

class IAHotPlugEventCallback : public hwcomposer::HotPlugCallback {
 public:
  IAHotPlugEventCallback(hwc2_callback_data_t data,
                         hwc2_function_pointer_t hook,
                         IAHWC2::HwcDisplay *display)
      : data_(data), hook_(hook), display_(display) {
  }

  void Callback(uint32_t display, bool connected) {
    if (display == 0) {
      // SF expects primary display to be always
      // connected. Let's notify it first time and ignore
      // any followup status changes.
      if (notified_) {
        return;
      }

      if (connected)
        notified_ = true;
    }

    auto hook = reinterpret_cast<HWC2_PFN_HOTPLUG>(hook_);
    int32_t status = static_cast<int32_t>(HWC2::Connection::Connected);
    if (!connected) {
      status = static_cast<int32_t>(HWC2::Connection::Disconnected);
    }

    IHOTPLUGEVENTTRACE(
        "IAHotPlugEventCallback called displayid: %d status: %d \n", display,
        status);
    if (hook)
      hook(data_, display, status);

    // FIXME: SurfaceFlinger doesn't seem to reset layers correctly
    // when display is connected/disconnected. We force it here.
    // Remove this workaround once fixed correctly in SurfaceFlinger.
    if (!connected && display > 0) {
      display_->FreeAllLayers();
    }
  }

 private:
  hwc2_callback_data_t data_;
  hwc2_function_pointer_t hook_;
  IAHWC2::HwcDisplay *display_;
  bool notified_ = false;
};

IAHWC2::IAHWC2() {
  common.tag = HARDWARE_DEVICE_TAG;
  common.version = HWC_DEVICE_API_VERSION_2_0;
  common.close = HookDevClose;
  getCapabilities = HookDevGetCapabilities;
  getFunction = HookDevGetFunction;
}

HWC2::Error IAHWC2::Init() {
  char value[PROPERTY_VALUE_MAX];
  property_get("board.disable.explicit.sync", value, "0");
  disable_explicit_sync_ = atoi(value);
  if (disable_explicit_sync_)
    ALOGI("EXPLICIT SYNC support is disabled");
  else
    ALOGI("EXPLICIT SYNC support is enabled");

  property_get("board.hwc.scaling.mode", value, "2");
  scaling_mode_ = atoi(value);
  switch (scaling_mode_) {
    case 1:
      ALOGI("HWC Scaling Mode Fast");
      break;
    case 2:
      ALOGI("HWC Scaling Mode High Quality");
      break;
    default:
      ALOGI("Unsupport HWC Scaling Mode");
      break;
  }

  if (!device_.Initialize()) {
    ALOGE("Can't initialize drm object.");
    return HWC2::Error::NoResources;
  }

  const std::vector<NativeDisplay *> &displays = device_.GetAllDisplays();
  NativeDisplay *primary_display = displays.at(0);
  uint32_t external_display_id = 1;
  primary_display_.Init(primary_display, 0, disable_explicit_sync_,
                        scaling_mode_);
  size_t size = displays.size();

  for (size_t i = 0; i < size; ++i) {
    hwcomposer::NativeDisplay *display = displays.at(i);
    if (display == primary_display)
      continue;

    std::unique_ptr<HwcDisplay> temp(new HwcDisplay());
    temp->Init(display, external_display_id, disable_explicit_sync_,
               scaling_mode_);
    extended_displays_.emplace_back(std::move(temp));
    external_display_id++;

    // Let's not confuse things with Virtual Display.
    if (external_display_id == HWC_DISPLAY_VIRTUAL)
      external_display_id = HWC_DISPLAY_VIRTUAL + 1;
  }

  // Start the hwc service
  hwcService_.Start(*this);

  return HWC2::Error::None;
}

template <typename... Args>
static inline HWC2::Error unsupported(char const *func, Args... /*args*/) {
  ALOGV("Unsupported function: %s", func);
  return HWC2::Error::Unsupported;
}

static inline void supported(char const *func) {
  ALOGV("supported function: %s", func);
}

HWC2::Error IAHWC2::CreateVirtualDisplay(uint32_t width, uint32_t height,
                                         int32_t *format,
                                         hwc2_display_t *display) {
  *display = (hwc2_display_t)(virtual_display_index_ + HWC_DISPLAY_VIRTUAL +
                              VDS_OFFSET);
  std::unique_ptr<HwcDisplay> temp(new HwcDisplay());
  temp->InitVirtualDisplay(device_.CreateVirtualDisplay(virtual_display_index_),
                           width, height, virtual_display_index_,
                           disable_explicit_sync_);
  virtual_displays_.emplace_back(std::move(temp));
  virtual_display_index_++;

  if (*format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
    // fallback to RGBA_8888, align with framework requirement
    *format = HAL_PIXEL_FORMAT_RGBA_8888;
  }

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::DestroyVirtualDisplay(hwc2_display_t display) {
  if (display <= (hwc2_display_t)(HWC_DISPLAY_VIRTUAL + VDS_OFFSET)) {
    ALOGE("Not Virtual Display Type in DestroyVirtualDisplay");
    return HWC2::Error::BadDisplay;
  }

  device_.DestroyVirtualDisplay(display - HWC_DISPLAY_VIRTUAL - VDS_OFFSET);
  virtual_displays_.at(display - HWC_DISPLAY_VIRTUAL - VDS_OFFSET)
      .reset(nullptr);

  return HWC2::Error::None;
}

void IAHWC2::Dump(uint32_t *size, char *buffer) {
  // TODO: Implement dump
  unsupported(__func__, size, buffer);
}

uint32_t IAHWC2::GetMaxVirtualDisplayCount() {
  return 10;
}

HWC2::Error IAHWC2::RegisterCallback(int32_t descriptor,
                                     hwc2_callback_data_t data,
                                     hwc2_function_pointer_t function) {
  supported(__func__);
  auto callback = static_cast<HWC2::Callback>(descriptor);
  size_t size = extended_displays_.size();
  HWC2::Error error = HWC2::Error::None;

  switch (callback) {
    case HWC2::Callback::Hotplug: {
      primary_display_.RegisterHotPlugCallback(data, function);
      for (size_t i = 0; i < size; ++i) {
        IAHWC2::HwcDisplay *display = extended_displays_.at(i).get();
        display->RegisterHotPlugCallback(data, function);
      }

      break;
    }
    case HWC2::Callback::Vsync: {
      primary_display_.RegisterVsyncCallback(data, function);
      for (size_t i = 0; i < size; ++i) {
        IAHWC2::HwcDisplay *display = extended_displays_.at(i).get();
        display->RegisterVsyncCallback(data, function);
      }

      break;
    }
    case HWC2::Callback::Refresh: {
      primary_display_.RegisterRefreshCallback(data, function);
      for (size_t i = 0; i < size; ++i) {
        IAHWC2::HwcDisplay *display = extended_displays_.at(i).get();
        display->RegisterRefreshCallback(data, function);
      }

      break;
    }
    default:
      error = HWC2::Error::BadParameter;
      break;
  }
  return error;
}

IAHWC2::HwcDisplay::HwcDisplay()
    : display_(nullptr),
      handle_(0),
      type_(HWC2::DisplayType::Invalid),
      frame_no_(0),
      check_validate_display_(false),
      disable_explicit_sync_(false),
      enable_nested_display_compose_(false),
      scaling_mode_(0) {
  supported(__func__);
}

// This function will be called only for Virtual Display Init
HWC2::Error IAHWC2::HwcDisplay::InitVirtualDisplay(
    hwcomposer::NativeDisplay *display, uint32_t width, uint32_t height,
    uint32_t display_index, bool disable_explicit_sync) {
  supported(__func__);
  display_ = display;
  type_ = HWC2::DisplayType::Virtual;
  handle_ = display_index + HWC_DISPLAY_VIRTUAL + VDS_OFFSET;
  display_->InitVirtualDisplay(width, height);
  disable_explicit_sync_ = disable_explicit_sync;
  display_->SetExplicitSyncSupport(disable_explicit_sync_);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::Init(hwcomposer::NativeDisplay *display,
                                     int display_index,
                                     bool disable_explicit_sync,
                                     uint32_t scaling_mode) {
  supported(__func__);
  display_ = display;
  type_ = HWC2::DisplayType::Physical;
  handle_ = display_index;

  disable_explicit_sync_ = disable_explicit_sync;
  scaling_mode_ = scaling_mode;
  display_->SetExplicitSyncSupport(disable_explicit_sync_);
  display_->SetVideoScalingMode(scaling_mode_);

  if (!display_->IsConnected()) {
    return HWC2::Error::None;
  }
  // Fetch the number of modes from the display
  uint32_t num_configs;
  HWC2::Error err = GetDisplayConfigs(&num_configs, NULL);
  if (err != HWC2::Error::None || !num_configs)
    return err;

  // Grab the first mode, we'll choose this as the active mode
  hwc2_config_t default_config;
  num_configs = 1;
  err = GetDisplayConfigs(&num_configs, &default_config);
  if (err != HWC2::Error::None)
    return err;

  display_->InitializeLayerHashGenerator(32);

  return SetActiveConfig(default_config);
}

HWC2::Error IAHWC2::HwcDisplay::RegisterVsyncCallback(
    hwc2_callback_data_t data, hwc2_function_pointer_t func) {
  supported(__func__);
  auto callback = std::make_shared<IAVsyncCallback>(data, func);
  int ret = display_->RegisterVsyncCallback(std::move(callback),
                                            static_cast<int>(handle_));
  if (ret) {
    ALOGE("Failed to register callback d=%" PRIu64 " ret=%d", handle_, ret);
    return HWC2::Error::BadDisplay;
  }
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::RegisterRefreshCallback(
    hwc2_callback_data_t data, hwc2_function_pointer_t func) {
  supported(__func__);
  auto callback = std::make_shared<IARefreshCallback>(data, func);
  display_->RegisterRefreshCallback(std::move(callback),
                                    static_cast<int>(handle_));
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::RegisterHotPlugCallback(
    hwc2_callback_data_t data, hwc2_function_pointer_t func) {
  supported(__func__);
  auto callback = std::make_shared<IAHotPlugEventCallback>(data, func, this);
  display_->RegisterHotPlugCallback(std::move(callback),
                                    static_cast<int>(handle_));
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::AcceptDisplayChanges() {
  supported(__func__);
  if (!check_validate_display_) {
    ALOGV("AcceptChanges failed, not validated");
    return HWC2::Error::NotValidated;
  }

  for (std::pair<const hwc2_layer_t, IAHWC2::Hwc2Layer> &l : layers_)
    l.second.accept_type_change();

  // reset the value to false
  check_validate_display_ = false;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::CreateLayer(hwc2_layer_t *layer) {
  supported(__func__);
  uint64_t id = display_->AcquireId();
  layers_.emplace(static_cast<hwc2_layer_t>(id), IAHWC2::Hwc2Layer());
  layers_.at(id).XTranslateCoordinates(display_->GetXTranslation());
  layers_.at(id).YTranslateCoordinates(display_->GetYTranslation());
  *layer = static_cast<hwc2_layer_t>(id);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::DestroyLayer(hwc2_layer_t layer) {
  supported(__func__);
  if (layers_.empty())
    return HWC2::Error::None;

  if (layers_.erase(layer))
    display_->ReleaseId(layer);

  return HWC2::Error::None;
}

void IAHWC2::HwcDisplay::FreeAllLayers() {
  if (layers_.empty())
    return;

  display_->ResetLayerHashGenerator();
  std::map<hwc2_layer_t, Hwc2Layer>().swap(layers_);
}

HWC2::Error IAHWC2::HwcDisplay::GetActiveConfig(hwc2_config_t *config) {
  supported(__func__);
  IHOTPLUGEVENTTRACE("GetActiveConfig called for Display: %p \n", display_);

  if (!display_->GetActiveConfig(config))
    return HWC2::Error::BadConfig;

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetChangedCompositionTypes(
    uint32_t *num_elements, hwc2_layer_t *layers, int32_t *types) {
  supported(__func__);
  uint32_t num_changes = 0;
  for (std::pair<const hwc2_layer_t, IAHWC2::Hwc2Layer> &l : layers_) {
    if (l.second.type_changed()) {
      if (layers && num_changes < *num_elements)
        layers[num_changes] = l.first;
      if (types && num_changes < *num_elements)
        types[num_changes] = static_cast<int32_t>(l.second.validated_type());
      ++num_changes;
    }
  }
  if (!layers && !types)
    *num_elements = num_changes;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetClientTargetSupport(uint32_t width,
                                                       uint32_t height,
                                                       int32_t format,
                                                       int32_t dataspace) {
  if (width != display_->Width() || height != display_->Height()) {
    return HWC2::Error::Unsupported;
  }

  if (format == HAL_PIXEL_FORMAT_RGBA_8888 &&
      (dataspace == HAL_DATASPACE_UNKNOWN ||
       dataspace == HAL_DATASPACE_STANDARD_UNSPECIFIED)) {
    return HWC2::Error::None;
  } else {
    // Convert HAL to fourcc-based DRM formats
    uint32_t drm_format = GetDrmFormatFromHALFormat(format);
    if (display_->CheckPlaneFormat(drm_format) &&
        (dataspace == HAL_DATASPACE_UNKNOWN ||
         dataspace == HAL_DATASPACE_STANDARD_UNSPECIFIED))
      return HWC2::Error::None;
  }

  return HWC2::Error::Unsupported;
}

HWC2::Error IAHWC2::HwcDisplay::GetColorModes(uint32_t *num_modes,
                                              int32_t *modes) {
  supported(__func__);
  if (!modes)
    *num_modes = 1;

  if (modes)
    *modes = HAL_COLOR_MODE_NATIVE;

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetDisplayAttribute(hwc2_config_t config,
                                                    int32_t attribute_in,
                                                    int32_t *value) {
  supported(__func__);
  auto attribute = static_cast<HWC2::Attribute>(attribute_in);
  switch (attribute) {
    case HWC2::Attribute::Width:
      display_->GetDisplayAttribute(
          config, hwcomposer::HWCDisplayAttribute::kWidth, value);
      break;
    case HWC2::Attribute::Height:
      display_->GetDisplayAttribute(
          config, hwcomposer::HWCDisplayAttribute::kHeight, value);
      break;
    case HWC2::Attribute::VsyncPeriod:
      // in nanoseconds
      display_->GetDisplayAttribute(
          config, hwcomposer::HWCDisplayAttribute::kRefreshRate, value);
      break;
    case HWC2::Attribute::DpiX:
      // Dots per 1000 inches
      display_->GetDisplayAttribute(
          config, hwcomposer::HWCDisplayAttribute::kDpiX, value);
      break;
    case HWC2::Attribute::DpiY:
      // Dots per 1000 inches
      display_->GetDisplayAttribute(
          config, hwcomposer::HWCDisplayAttribute::kDpiY, value);
      break;
    default:
      *value = -1;
      return HWC2::Error::BadConfig;
  }
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                                  hwc2_config_t *configs) {
  supported(__func__);
  if (!display_->GetDisplayConfigs(num_configs, configs))
    return HWC2::Error::BadDisplay;

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetDisplayName(uint32_t *size, char *name) {
  supported(__func__);
  if (!display_->GetDisplayName(size, name))
    return HWC2::Error::BadDisplay;

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetDisplayRequests(int32_t *display_requests,
                                                   uint32_t *num_elements,
                                                   hwc2_layer_t *layers,
                                                   int32_t *layer_requests) {
  supported(__func__);
  // TODO: I think virtual display should request
  //      HWC2_DISPLAY_REQUEST_WRITE_CLIENT_TARGET_TO_OUTPUT here
  unsupported(__func__, display_requests, num_elements, layers, layer_requests);
  *num_elements = 0;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetDisplayType(int32_t *type) {
  supported(__func__);
  *type = static_cast<int32_t>(type_);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetDozeSupport(int32_t *support) {
  supported(__func__);
  *support = true;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetHdrCapabilities(
    uint32_t *num_types, int32_t * /*types*/, float * /*max_luminance*/,
    float * /*max_average_luminance*/, float * /*min_luminance*/) {
  supported(__func__);
  *num_types = 0;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::GetReleaseFences(uint32_t *num_elements,
                                                 hwc2_layer_t *layers,
                                                 int32_t *fences) {
  supported(__func__);
  if (layers == NULL || fences == NULL) {
    *num_elements = layers_.size();
    return HWC2::Error::None;
  }

  uint32_t num_layers = 0;
  for (std::pair<const hwc2_layer_t, IAHWC2::Hwc2Layer> &l : layers_) {
    ++num_layers;
    if (num_layers > *num_elements) {
      ALOGW("Overflow num_elements %d/%d", num_layers, *num_elements);
      return HWC2::Error::None;
    }

    layers[num_layers - 1] = l.first;
    fences[num_layers - 1] = l.second.GetLayer()->GetReleaseFence();
  }

  *num_elements = num_layers;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::PresentDisplay(int32_t *retire_fence) {
  supported(__func__);
  // order the layers by z-order
  bool use_client_layer = false;
  uint32_t client_z_order = 0;
  bool use_cursor_layer = false;
  uint32_t cursor_z_order = 0;
  IAHWC2::Hwc2Layer *cursor_layer;
  *retire_fence = -1;
  std::map<uint32_t, IAHWC2::Hwc2Layer *> z_map;

  // if the power mode is doze suspend then its the hint that the drawing
  // into the display has suspended and remain in the low power state and
  // continue displaying the current state and stop applying display
  // update from the client
  if (display_->PowerMode() == HWC2_POWER_MODE_DOZE_SUSPEND)
    return HWC2::Error::None;
  for (std::pair<const hwc2_layer_t, IAHWC2::Hwc2Layer> &l : layers_) {
    if (l.second.IsCursorLayer()) {
      use_cursor_layer = true;
      cursor_layer = &l.second;
      cursor_z_order = l.second.z_order();
      continue;
    }

    switch (l.second.validated_type()) {
      case HWC2::Composition::Device:
      case HWC2::Composition::SolidColor:
        z_map.emplace(std::make_pair(l.second.z_order(), &l.second));
        break;
      case HWC2::Composition::Client:
        // Place it at the z_order of the highest client layer
        use_client_layer = true;
        client_z_order = std::max(client_z_order, l.second.z_order());
        break;
      default:
        continue;
    }
  }
  if (use_client_layer && client_layer_.GetLayer() &&
      client_layer_.GetLayer()->GetNativeHandle() &&
      client_layer_.GetLayer()->GetNativeHandle()->handle_) {
    z_map.emplace(std::make_pair(client_z_order, &client_layer_));
  }

  // Place the cursor at the highest z-order
  if (use_cursor_layer) {
    if (z_map.size()) {
      if (z_map.rbegin()->second->z_order() > cursor_z_order)
        cursor_z_order = z_map.rbegin()->second->z_order() + 1;
      else if (client_z_order > cursor_z_order)
        cursor_z_order = client_z_order + 1;
    }
    z_map.emplace(std::make_pair(cursor_z_order, cursor_layer));
  }

  std::vector<hwcomposer::HwcLayer *> layers;
  // now that they're ordered by z, add them to the composition
  for (std::pair<const uint32_t, IAHWC2::Hwc2Layer *> &l : z_map) {
    layers.emplace_back(l.second->GetLayer());
  }

  if (layers.empty())
    return HWC2::Error::None;

  IHOTPLUGEVENTTRACE("PhysicalDisplay called for Display: %p \n", display_);

  bool success = display_->Present(layers, retire_fence);
  if (!success) {
    ALOGE("Failed to set layers in the composition");
    return HWC2::Error::BadLayer;
  }

  ++frame_no_;

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetActiveConfig(hwc2_config_t config) {
  supported(__func__);
  if (!display_->SetActiveConfig(config)) {
    ALOGE("Could not find active mode for %d", config);
    return HWC2::Error::BadConfig;
  }

  // Setup the client layer's dimensions
  hwc_rect_t display_frame = {.left = 0,
                              .top = 0,
                              .right = static_cast<int>(display_->Width()),
                              .bottom = static_cast<int>(display_->Height())};
  client_layer_.SetLayerDisplayFrame(display_frame);
  hwc_frect_t source_crop = {.left = 0.0f,
                             .top = 0.0f,
                             .right = display_->Width() + 0.0f,
                             .bottom = display_->Height() + 0.0f};
  client_layer_.SetLayerSourceCrop(source_crop);

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetClientTarget(buffer_handle_t target,
                                                int32_t acquire_fence,
                                                int32_t dataspace,
                                                hwc_region_t damage) {
  supported(__func__);
  client_layer_.set_buffer(target);
  client_layer_.set_acquire_fence(acquire_fence);
  client_layer_.SetLayerDataspace(dataspace);
  client_layer_.SetLayerSurfaceDamage(damage);

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetColorMode(int32_t mode) {
  supported(__func__);
  // TODO: Use the parameter mode to set the color mode for the display to be
  // used.

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetColorTransform(const float *matrix,
                                                  int32_t hint) {
  supported(__func__);
  // TODO: Force client composition if we get this

  if (hint != HAL_COLOR_TRANSFORM_IDENTITY &&
      hint != HAL_COLOR_TRANSFORM_ARBITRARY_MATRIX &&
      hint != HAL_COLOR_TRANSFORM_VALUE_INVERSE &&
      hint != HAL_COLOR_TRANSFORM_GRAYSCALE &&
      hint != HAL_COLOR_TRANSFORM_CORRECT_PROTANOPIA &&
      hint != HAL_COLOR_TRANSFORM_CORRECT_DEUTERANOPIA &&
      hint != HAL_COLOR_TRANSFORM_CORRECT_TRITANOPIA)
    return HWC2::Error::BadParameter;

  display_->SetColorTransform(matrix, (HWCColorTransform)hint);

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetOutputBuffer(buffer_handle_t buffer,
                                                int32_t release_fence) {
  supported(__func__);

  struct gralloc_handle *temp = new struct gralloc_handle();
  temp->handle_ = buffer;
  display_->SetOutputBuffer(temp, release_fence);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetPowerMode(int32_t mode_in) {
  supported(__func__);
  uint32_t power_mode = 0;
  auto mode = static_cast<HWC2::PowerMode>(mode_in);
  switch (mode) {
    case HWC2::PowerMode::Off:
      power_mode = HWC2_POWER_MODE_OFF;
      break;
    case HWC2::PowerMode::Doze:
      power_mode = HWC2_POWER_MODE_DOZE;
      break;
    case HWC2::PowerMode::DozeSuspend:
      power_mode = HWC2_POWER_MODE_DOZE_SUSPEND;
      break;
    case HWC2::PowerMode::On:
      power_mode = HWC2_POWER_MODE_ON;
      break;
    default:
      ALOGI("Power mode %d is unsupported\n", mode);
      return HWC2::Error::BadParameter;
  };

  display_->SetPowerMode(power_mode);

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::SetVsyncEnabled(int32_t enabled) {
  supported(__func__);
  switch (enabled) {
    case HWC2_VSYNC_ENABLE:
      display_->VSyncControl(true);
      break;
    case HWC2_VSYNC_DISABLE:
      display_->VSyncControl(false);
      break;
    default:
      ALOGE("SetVsyncEnabled called with invalid parameter");
      return HWC2::Error::BadParameter;
  }
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::HwcDisplay::ValidateDisplay(uint32_t *num_types,
                                                uint32_t *num_requests) {
  supported(__func__);
  *num_types = 0;
  *num_requests = 0;
  for (std::pair<const hwc2_layer_t, IAHWC2::Hwc2Layer> &l : layers_) {
    IAHWC2::Hwc2Layer &layer = l.second;

    switch (layer.sf_type()) {
      case HWC2::Composition::Sideband:
        layer.set_validated_type(HWC2::Composition::Client);
        ++*num_types;
        break;
      case HWC2::Composition::Cursor:
        layer.set_validated_type(HWC2::Composition::Device);
        ++*num_types;
        break;
      default:
        if (disable_explicit_sync_ ||
            display_->PowerMode() == HWC2_POWER_MODE_DOZE_SUSPEND) {
          layer.set_validated_type(HWC2::Composition::Client);
        } else {
          layer.set_validated_type(layer.sf_type());
        }
        break;
    }
  }

  check_validate_display_ = true;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetCursorPosition(int32_t /*x*/, int32_t /*y*/) {
  supported(__func__);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerBlendMode(int32_t mode) {
  supported(__func__);
  switch (static_cast<HWC2::BlendMode>(mode)) {
    case HWC2::BlendMode::None:
      hwc_layer_.SetBlending(hwcomposer::HWCBlending::kBlendingNone);
      break;
    case HWC2::BlendMode::Premultiplied:
      hwc_layer_.SetBlending(hwcomposer::HWCBlending::kBlendingPremult);
      break;
    case HWC2::BlendMode::Coverage:
      hwc_layer_.SetBlending(hwcomposer::HWCBlending::kBlendingCoverage);
      break;
    default:
      ALOGE("Unknown blending mode b=%d", mode);
      hwc_layer_.SetBlending(hwcomposer::HWCBlending::kBlendingNone);
      break;
  }
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerBuffer(buffer_handle_t buffer,
                                              int32_t acquire_fence) {
  supported(__func__);

  // The buffer and acquire_fence are handled elsewhere
  if (sf_type_ == HWC2::Composition::Client ||
      sf_type_ == HWC2::Composition::Sideband)
    return HWC2::Error::None;

  native_handle_.handle_ = buffer;
  hwc_layer_.SetNativeHandle(&native_handle_);
  if (acquire_fence > 0)
    hwc_layer_.SetAcquireFence(acquire_fence);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerColor(hwc_color_t color) {
  // We only support Opaque colors so far.
  if (color.r == 0 && color.g == 0 && color.b == 0 && color.a == 255) {
    sf_type_ = HWC2::Composition::SolidColor;
    hwc_layer_.SetLayerCompositionType(hwcomposer::Composition_SolidColor);
    return HWC2::Error::None;
  }

  sf_type_ = HWC2::Composition::Client;
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerCompositionType(int32_t type) {
  sf_type_ = static_cast<HWC2::Composition>(type);
  if (sf_type_ == HWC2::Composition::Cursor) {
    hwc_layer_.MarkAsCursorLayer();
  }

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerDataspace(int32_t dataspace) {
  supported(__func__);
  dataspace_ = static_cast<android_dataspace_t>(dataspace);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerDisplayFrame(hwc_rect_t frame) {
  supported(__func__);
  hwc_layer_.SetDisplayFrame(
      hwcomposer::HwcRect<int>(frame.left, frame.top, frame.right,
                               frame.bottom),
      x_translation_, y_translation_);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerPlaneAlpha(float alpha) {
  supported(__func__);
  hwc_layer_.SetAlpha(static_cast<uint8_t>(255.0f * alpha + 0.5f));
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerSidebandStream(
    const native_handle_t *stream) {
  supported(__func__);
  // TODO: We don't support sideband
  return unsupported(__func__, stream);
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerSourceCrop(hwc_frect_t crop) {
  supported(__func__);
  hwc_layer_.SetSourceCrop(
      hwcomposer::HwcRect<float>(crop.left, crop.top, crop.right, crop.bottom));
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerSurfaceDamage(hwc_region_t damage) {
  uint32_t num_rects = damage.numRects;
  hwcomposer::HwcRegion hwc_region;

  for (size_t rect = 0; rect < num_rects; ++rect) {
    hwc_region.emplace_back(damage.rects[rect].left, damage.rects[rect].top,
                            damage.rects[rect].right,
                            damage.rects[rect].bottom);
  }

  hwc_layer_.SetSurfaceDamage(hwc_region);

  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerTransform(int32_t transform) {
  supported(__func__);
  // 270* and 180* cannot be combined with flips. More specifically, they
  // already contain both horizontal and vertical flips, so those fields are
  // redundant in this case. 90* rotation can be combined with either horizontal
  // flip or vertical flip, so treat it differently
  int32_t temp = 0;
  if (transform == HWC_TRANSFORM_ROT_270) {
    temp = hwcomposer::HWCTransform::kTransform270;
  } else if (transform == HWC_TRANSFORM_ROT_180) {
    temp = hwcomposer::HWCTransform::kTransform180;
  } else {
    if (transform & HWC_TRANSFORM_FLIP_H)
      temp |= hwcomposer::HWCTransform::kReflectX;
    if (transform & HWC_TRANSFORM_FLIP_V)
      temp |= hwcomposer::HWCTransform::kReflectY;
    if (transform & HWC_TRANSFORM_ROT_90)
      temp |= hwcomposer::HWCTransform::kTransform90;
  }
  hwc_layer_.SetTransform(temp);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerVisibleRegion(hwc_region_t visible) {
  uint32_t num_rects = visible.numRects;
  hwcomposer::HwcRegion hwc_region;

  for (size_t rect = 0; rect < num_rects; ++rect) {
    hwc_region.emplace_back(visible.rects[rect].left, visible.rects[rect].top,
                            visible.rects[rect].right,
                            visible.rects[rect].bottom);
  }

  hwc_layer_.SetVisibleRegion(hwc_region);
  return HWC2::Error::None;
}

HWC2::Error IAHWC2::Hwc2Layer::SetLayerZOrder(uint32_t order) {
  supported(__func__);

  hwc_layer_.SetLayerZOrder(order);
  return HWC2::Error::None;
}

// static
int IAHWC2::HookDevClose(hw_device_t * /*dev*/) {
  unsupported(__func__);
  return 0;
}

// static
void IAHWC2::HookDevGetCapabilities(hwc2_device_t * /*dev*/,
                                    uint32_t *out_count,
                                    int32_t * /*out_capabilities*/) {
  supported(__func__);
  *out_count = 0;
}

// static
hwc2_function_pointer_t IAHWC2::HookDevGetFunction(struct hwc2_device * /*dev*/,
                                                   int32_t descriptor) {
  supported(__func__);
  auto func = static_cast<HWC2::FunctionDescriptor>(descriptor);
  switch (func) {
    // Device functions
    case HWC2::FunctionDescriptor::CreateVirtualDisplay:
      return ToHook<HWC2_PFN_CREATE_VIRTUAL_DISPLAY>(
          DeviceHook<int32_t, decltype(&IAHWC2::CreateVirtualDisplay),
                     &IAHWC2::CreateVirtualDisplay, uint32_t, uint32_t,
                     int32_t *, hwc2_display_t *>);
    case HWC2::FunctionDescriptor::DestroyVirtualDisplay:
      return ToHook<HWC2_PFN_DESTROY_VIRTUAL_DISPLAY>(
          DeviceHook<int32_t, decltype(&IAHWC2::DestroyVirtualDisplay),
                     &IAHWC2::DestroyVirtualDisplay, hwc2_display_t>);
    case HWC2::FunctionDescriptor::Dump:
      return ToHook<HWC2_PFN_DUMP>(DeviceHook<
          void, decltype(&IAHWC2::Dump), &IAHWC2::Dump, uint32_t *, char *>);
    case HWC2::FunctionDescriptor::GetMaxVirtualDisplayCount:
      return ToHook<HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT>(
          DeviceHook<uint32_t, decltype(&IAHWC2::GetMaxVirtualDisplayCount),
                     &IAHWC2::GetMaxVirtualDisplayCount>);
    case HWC2::FunctionDescriptor::RegisterCallback:
      return ToHook<HWC2_PFN_REGISTER_CALLBACK>(
          DeviceHook<int32_t, decltype(&IAHWC2::RegisterCallback),
                     &IAHWC2::RegisterCallback, int32_t, hwc2_callback_data_t,
                     hwc2_function_pointer_t>);

    // Display functions
    case HWC2::FunctionDescriptor::AcceptDisplayChanges:
      return ToHook<HWC2_PFN_ACCEPT_DISPLAY_CHANGES>(
          DisplayHook<decltype(&HwcDisplay::AcceptDisplayChanges),
                      &HwcDisplay::AcceptDisplayChanges>);
    case HWC2::FunctionDescriptor::CreateLayer:
      return ToHook<HWC2_PFN_CREATE_LAYER>(
          DisplayHook<decltype(&HwcDisplay::CreateLayer),
                      &HwcDisplay::CreateLayer, hwc2_layer_t *>);
    case HWC2::FunctionDescriptor::DestroyLayer:
      return ToHook<HWC2_PFN_DESTROY_LAYER>(
          DisplayHook<decltype(&HwcDisplay::DestroyLayer),
                      &HwcDisplay::DestroyLayer, hwc2_layer_t>);
    case HWC2::FunctionDescriptor::GetActiveConfig:
      return ToHook<HWC2_PFN_GET_ACTIVE_CONFIG>(
          DisplayHook<decltype(&HwcDisplay::GetActiveConfig),
                      &HwcDisplay::GetActiveConfig, hwc2_config_t *>);
    case HWC2::FunctionDescriptor::GetChangedCompositionTypes:
      return ToHook<HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES>(
          DisplayHook<decltype(&HwcDisplay::GetChangedCompositionTypes),
                      &HwcDisplay::GetChangedCompositionTypes, uint32_t *,
                      hwc2_layer_t *, int32_t *>);
    case HWC2::FunctionDescriptor::GetClientTargetSupport:
      return ToHook<HWC2_PFN_GET_CLIENT_TARGET_SUPPORT>(
          DisplayHook<decltype(&HwcDisplay::GetClientTargetSupport),
                      &HwcDisplay::GetClientTargetSupport, uint32_t, uint32_t,
                      int32_t, int32_t>);
    case HWC2::FunctionDescriptor::GetColorModes:
      return ToHook<HWC2_PFN_GET_COLOR_MODES>(
          DisplayHook<decltype(&HwcDisplay::GetColorModes),
                      &HwcDisplay::GetColorModes, uint32_t *, int32_t *>);
    case HWC2::FunctionDescriptor::GetDisplayAttribute:
      return ToHook<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(DisplayHook<
          decltype(&HwcDisplay::GetDisplayAttribute),
          &HwcDisplay::GetDisplayAttribute, hwc2_config_t, int32_t, int32_t *>);
    case HWC2::FunctionDescriptor::GetDisplayConfigs:
      return ToHook<HWC2_PFN_GET_DISPLAY_CONFIGS>(DisplayHook<
          decltype(&HwcDisplay::GetDisplayConfigs),
          &HwcDisplay::GetDisplayConfigs, uint32_t *, hwc2_config_t *>);
    case HWC2::FunctionDescriptor::GetDisplayName:
      return ToHook<HWC2_PFN_GET_DISPLAY_NAME>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayName),
                      &HwcDisplay::GetDisplayName, uint32_t *, char *>);
    case HWC2::FunctionDescriptor::GetDisplayRequests:
      return ToHook<HWC2_PFN_GET_DISPLAY_REQUESTS>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayRequests),
                      &HwcDisplay::GetDisplayRequests, int32_t *, uint32_t *,
                      hwc2_layer_t *, int32_t *>);
    case HWC2::FunctionDescriptor::GetDisplayType:
      return ToHook<HWC2_PFN_GET_DISPLAY_TYPE>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayType),
                      &HwcDisplay::GetDisplayType, int32_t *>);
    case HWC2::FunctionDescriptor::GetDozeSupport:
      return ToHook<HWC2_PFN_GET_DOZE_SUPPORT>(
          DisplayHook<decltype(&HwcDisplay::GetDozeSupport),
                      &HwcDisplay::GetDozeSupport, int32_t *>);
    case HWC2::FunctionDescriptor::GetHdrCapabilities:
      return ToHook<HWC2_PFN_GET_HDR_CAPABILITIES>(
          DisplayHook<decltype(&HwcDisplay::GetHdrCapabilities),
                      &HwcDisplay::GetHdrCapabilities, uint32_t *, int32_t *,
                      float *, float *, float *>);
    case HWC2::FunctionDescriptor::GetReleaseFences:
      return ToHook<HWC2_PFN_GET_RELEASE_FENCES>(
          DisplayHook<decltype(&HwcDisplay::GetReleaseFences),
                      &HwcDisplay::GetReleaseFences, uint32_t *, hwc2_layer_t *,
                      int32_t *>);
    case HWC2::FunctionDescriptor::PresentDisplay:
      return ToHook<HWC2_PFN_PRESENT_DISPLAY>(
          DisplayHook<decltype(&HwcDisplay::PresentDisplay),
                      &HwcDisplay::PresentDisplay, int32_t *>);
    case HWC2::FunctionDescriptor::SetActiveConfig:
      return ToHook<HWC2_PFN_SET_ACTIVE_CONFIG>(
          DisplayHook<decltype(&HwcDisplay::SetActiveConfig),
                      &HwcDisplay::SetActiveConfig, hwc2_config_t>);
    case HWC2::FunctionDescriptor::SetClientTarget:
      return ToHook<HWC2_PFN_SET_CLIENT_TARGET>(DisplayHook<
          decltype(&HwcDisplay::SetClientTarget), &HwcDisplay::SetClientTarget,
          buffer_handle_t, int32_t, int32_t, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetColorMode:
      return ToHook<HWC2_PFN_SET_COLOR_MODE>(
          DisplayHook<decltype(&HwcDisplay::SetColorMode),
                      &HwcDisplay::SetColorMode, int32_t>);
    case HWC2::FunctionDescriptor::SetColorTransform:
      return ToHook<HWC2_PFN_SET_COLOR_TRANSFORM>(
          DisplayHook<decltype(&HwcDisplay::SetColorTransform),
                      &HwcDisplay::SetColorTransform, const float *, int32_t>);
    case HWC2::FunctionDescriptor::SetOutputBuffer:
      return ToHook<HWC2_PFN_SET_OUTPUT_BUFFER>(
          DisplayHook<decltype(&HwcDisplay::SetOutputBuffer),
                      &HwcDisplay::SetOutputBuffer, buffer_handle_t, int32_t>);
    case HWC2::FunctionDescriptor::SetPowerMode:
      return ToHook<HWC2_PFN_SET_POWER_MODE>(
          DisplayHook<decltype(&HwcDisplay::SetPowerMode),
                      &HwcDisplay::SetPowerMode, int32_t>);
    case HWC2::FunctionDescriptor::SetVsyncEnabled:
      return ToHook<HWC2_PFN_SET_VSYNC_ENABLED>(
          DisplayHook<decltype(&HwcDisplay::SetVsyncEnabled),
                      &HwcDisplay::SetVsyncEnabled, int32_t>);
    case HWC2::FunctionDescriptor::ValidateDisplay:
      return ToHook<HWC2_PFN_VALIDATE_DISPLAY>(
          DisplayHook<decltype(&HwcDisplay::ValidateDisplay),
                      &HwcDisplay::ValidateDisplay, uint32_t *, uint32_t *>);

    // Layer functions
    case HWC2::FunctionDescriptor::SetCursorPosition:
      return ToHook<HWC2_PFN_SET_CURSOR_POSITION>(
          LayerHook<decltype(&Hwc2Layer::SetCursorPosition),
                    &Hwc2Layer::SetCursorPosition, int32_t, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerBlendMode:
      return ToHook<HWC2_PFN_SET_LAYER_BLEND_MODE>(
          LayerHook<decltype(&Hwc2Layer::SetLayerBlendMode),
                    &Hwc2Layer::SetLayerBlendMode, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerBuffer:
      return ToHook<HWC2_PFN_SET_LAYER_BUFFER>(
          LayerHook<decltype(&Hwc2Layer::SetLayerBuffer),
                    &Hwc2Layer::SetLayerBuffer, buffer_handle_t, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerColor:
      return ToHook<HWC2_PFN_SET_LAYER_COLOR>(
          LayerHook<decltype(&Hwc2Layer::SetLayerColor),
                    &Hwc2Layer::SetLayerColor, hwc_color_t>);
    case HWC2::FunctionDescriptor::SetLayerCompositionType:
      return ToHook<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(
          LayerHook<decltype(&Hwc2Layer::SetLayerCompositionType),
                    &Hwc2Layer::SetLayerCompositionType, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerDataspace:
      return ToHook<HWC2_PFN_SET_LAYER_DATASPACE>(
          LayerHook<decltype(&Hwc2Layer::SetLayerDataspace),
                    &Hwc2Layer::SetLayerDataspace, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerDisplayFrame:
      return ToHook<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(
          LayerHook<decltype(&Hwc2Layer::SetLayerDisplayFrame),
                    &Hwc2Layer::SetLayerDisplayFrame, hwc_rect_t>);
    case HWC2::FunctionDescriptor::SetLayerPlaneAlpha:
      return ToHook<HWC2_PFN_SET_LAYER_PLANE_ALPHA>(
          LayerHook<decltype(&Hwc2Layer::SetLayerPlaneAlpha),
                    &Hwc2Layer::SetLayerPlaneAlpha, float>);
    case HWC2::FunctionDescriptor::SetLayerSidebandStream:
      return ToHook<HWC2_PFN_SET_LAYER_SIDEBAND_STREAM>(LayerHook<
          decltype(&Hwc2Layer::SetLayerSidebandStream),
          &Hwc2Layer::SetLayerSidebandStream, const native_handle_t *>);
    case HWC2::FunctionDescriptor::SetLayerSourceCrop:
      return ToHook<HWC2_PFN_SET_LAYER_SOURCE_CROP>(
          LayerHook<decltype(&Hwc2Layer::SetLayerSourceCrop),
                    &Hwc2Layer::SetLayerSourceCrop, hwc_frect_t>);
    case HWC2::FunctionDescriptor::SetLayerSurfaceDamage:
      return ToHook<HWC2_PFN_SET_LAYER_SURFACE_DAMAGE>(
          LayerHook<decltype(&Hwc2Layer::SetLayerSurfaceDamage),
                    &Hwc2Layer::SetLayerSurfaceDamage, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetLayerTransform:
      return ToHook<HWC2_PFN_SET_LAYER_TRANSFORM>(
          LayerHook<decltype(&Hwc2Layer::SetLayerTransform),
                    &Hwc2Layer::SetLayerTransform, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerVisibleRegion:
      return ToHook<HWC2_PFN_SET_LAYER_VISIBLE_REGION>(
          LayerHook<decltype(&Hwc2Layer::SetLayerVisibleRegion),
                    &Hwc2Layer::SetLayerVisibleRegion, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetLayerZOrder:
      return ToHook<HWC2_PFN_SET_LAYER_Z_ORDER>(
          LayerHook<decltype(&Hwc2Layer::SetLayerZOrder),
                    &Hwc2Layer::SetLayerZOrder, uint32_t>);
    case HWC2::FunctionDescriptor::Invalid:
    default:
      return NULL;
  }
}

// static
int IAHWC2::HookDevOpen(const struct hw_module_t *module, const char *name,
                        struct hw_device_t **dev) {
  supported(__func__);
  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("Invalid module name- %s", name);
    return -EINVAL;
  }

  std::unique_ptr<IAHWC2> ctx(new IAHWC2());
  if (!ctx) {
    ALOGE("Failed to allocate IAHWC2");
    return -ENOMEM;
  }

  HWC2::Error err = ctx->Init();
  if (err != HWC2::Error::None) {
    ALOGE("Failed to initialize IAHWC2 err=%d\n", err);
    return -EINVAL;
  }

  ctx->common.module = const_cast<hw_module_t *>(module);
  *dev = &ctx->common;
  ctx.release();
  return 0;
}

hwcomposer::NativeDisplay *IAHWC2::GetExtendedDisplay(uint32_t dispIndex) {
  return extended_displays_.at(dispIndex)->GetDisplay();
}

hwcomposer::NativeDisplay *IAHWC2::GetPrimaryDisplay() {
  return primary_display_.GetDisplay();
}

hwcomposer::NativeDisplay *IAHWC2::HwcDisplay::GetDisplay() {
  return display_;
}

void IAHWC2::EnableHDCPSessionForDisplay(uint32_t display,
                                         EHwcsContentType content_type) {
  HWCContentType type = kCONTENT_TYPE0;
  if (content_type == HWCS_CP_CONTENT_TYPE1) {
    type = kCONTENT_TYPE1;
  }

  device_.EnableHDCPSessionForDisplay(display, type);
}

void IAHWC2::EnableHDCPSessionForAllDisplays(EHwcsContentType content_type) {
  HWCContentType type = kCONTENT_TYPE0;
  if (content_type == HWCS_CP_CONTENT_TYPE1) {
    type = kCONTENT_TYPE1;
  }
  device_.EnableHDCPSessionForAllDisplays(type);
}

void IAHWC2::DisableHDCPSessionForDisplay(uint32_t display) {
  device_.DisableHDCPSessionForDisplay(display);
}

void IAHWC2::DisableHDCPSessionForAllDisplays() {
  device_.DisableHDCPSessionForAllDisplays();
}

void IAHWC2::SetPAVPSessionStatus(bool enabled, uint32_t papv_session_id,
                                  uint32_t pavp_instance_id) {
  device_.SetPAVPSessionStatus(enabled, papv_session_id, pavp_instance_id);
}

void IAHWC2::SetHDCPSRMForAllDisplays(const int8_t *SRM, uint32_t SRMLength) {
  if (SRM == NULL) {
    ETRACE("Error:HDCP Set NULL SRM");
    return;
  }
  device_.SetHDCPSRMForAllDisplays(SRM, SRMLength);
}

void IAHWC2::SetHDCPSRMForDisplay(uint32_t display, const int8_t *SRM,
                                  uint32_t SRMLength) {
  if (SRM == NULL) {
    ETRACE("Error:HDCP Set NULL SRM");
    return;
  }
  device_.SetHDCPSRMForDisplay(display, SRM, SRMLength);
}

}  // namespace android

static struct hw_module_methods_t hwc2_module_methods = {
    .open = android::IAHWC2::HookDevOpen,
};

hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = HARDWARE_MODULE_API_VERSION(2, 0),
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "IA-Hardware-Composer",
    .author = "The Android Open Source Project",
    .methods = &hwc2_module_methods,
    .dso = NULL,
    .reserved = {0},
};
