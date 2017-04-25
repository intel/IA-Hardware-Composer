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
#define LOG_TAG "hwc-drm-two"

#include "drmhwctwo.h"

#include <cutils/log.h>
#include <hardware/hwcomposer2.h>

namespace android {

DrmHwcTwo::DrmHwcTwo() {
  common.close = HookDevClose;
  getCapabilities = HookDevGetCapabilities;
  getFunction = HookDevGetFunction;
}

template <typename... Args>
static inline HWC2::Error unsupported(char const *func, Args... /*args*/) {
  ALOGV("Unsupported function: %s", func);
  return HWC2::Error::Unsupported;
}

HWC2::Error DrmHwcTwo::CreateVirtualDisplay(uint32_t width, uint32_t height,
                                            int32_t *format,
                                            hwc2_display_t *display) {
  // TODO: Implement virtual display
  return unsupported(__func__, width, height, display);
}

HWC2::Error DrmHwcTwo::DestroyVirtualDisplay(hwc2_display_t display) {
  return unsupported(__func__, display);
}

void DrmHwcTwo::Dump(uint32_t *size, char *buffer) {
  unsupported(__func__, size, buffer);
}

uint32_t DrmHwcTwo::GetMaxVirtualDisplayCount() {
  unsupported(__func__);
  return 0;
}

HWC2::Error DrmHwcTwo::RegisterCallback(int32_t descriptor,
                                         hwc2_callback_data_t data,
                                         hwc2_function_pointer_t function) {
  return unsupported(__func__, descriptor, data, function);
}

HWC2::Error DrmHwcTwo::HwcDisplay::AcceptDisplayChanges() {
  return unsupported(__func__);
}

HWC2::Error DrmHwcTwo::HwcDisplay::CreateLayer(hwc2_layer_t *layer) {
  return unsupported(__func__, layer);
}

HWC2::Error DrmHwcTwo::HwcDisplay::DestroyLayer(hwc2_layer_t layer) {
  return unsupported(__func__, layer);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetActiveConfig(hwc2_config_t *config) {
  return unsupported(__func__, config);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetChangedCompositionTypes(
    uint32_t *num_elements, hwc2_layer_t *layers, int32_t *types) {
  return unsupported(__func__, num_elements, layers, types);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetClientTargetSupport(uint32_t width,
                                                           uint32_t height,
                                                           int32_t format,
                                                           int32_t dataspace) {
  return unsupported(__func__, width, height, format, dataspace);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetColorModes(uint32_t *num_modes,
                                                  int32_t *modes) {
  return unsupported(__func__, num_modes, modes);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayAttribute(hwc2_config_t config,
                                                        int32_t attribute,
                                                        int32_t *value) {
  return unsupported(__func__, config, attribute, value);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                                     hwc2_config_t *configs) {
  return unsupported(__func__, num_configs, configs);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayName(uint32_t *size, char *name) {
  return unsupported(__func__, size, name);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayRequests(
    int32_t *display_requests, uint32_t *num_elements, hwc2_layer_t *layers,
    int32_t *layer_requests) {
  return unsupported(__func__, display_requests, num_elements, layers,
                     layer_requests);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayType(int32_t *type) {
  return unsupported(__func__, type);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDozeSupport(int32_t *support) {
  return unsupported(__func__, support);
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetReleaseFences(uint32_t *num_elements,
                                                     hwc2_layer_t *layers,
                                                     int32_t *fences) {
  return unsupported(__func__, num_elements, layers, fences);
}

HWC2::Error DrmHwcTwo::HwcDisplay::PresentDisplay(int32_t *retire_fence) {
  return unsupported(__func__, retire_fence);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetActiveConfig(hwc2_config_t config) {
  return unsupported(__func__, config);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetClientTarget(buffer_handle_t target,
                                                   int32_t acquire_fence,
                                                   int32_t dataspace,
                                                   hwc_region_t damage) {
  return unsupported(__func__, target, acquire_fence, dataspace, damage);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetColorMode(int32_t mode) {
  return unsupported(__func__, mode);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetColorTransform(const float *matrix,
                                                      int32_t hint) {
  return unsupported(__func__, matrix, hint);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetOutputBuffer(buffer_handle_t buffer,
                                                    int32_t release_fence) {
  return unsupported(__func__, buffer, release_fence);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetPowerMode(int32_t mode) {
  return unsupported(__func__, mode);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetVsyncEnabled(int32_t enabled) {
  return unsupported(__func__, enabled);
}

HWC2::Error DrmHwcTwo::HwcDisplay::ValidateDisplay(uint32_t *num_types,
                                                    uint32_t *num_requests) {
  return unsupported(__func__, num_types, num_requests);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetCursorPosition(int32_t x, int32_t y) {
  return unsupported(__func__, x, y);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerBlendMode(int32_t mode) {
  return unsupported(__func__, mode);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerBuffer(buffer_handle_t buffer,
                                                 int32_t acquire_fence) {
  return unsupported(__func__, buffer, acquire_fence);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerColor(hwc_color_t color) {
  return unsupported(__func__, color);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerCompositionType(int32_t type) {
  return unsupported(__func__, type);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerDataspace(int32_t dataspace) {
  return unsupported(__func__, dataspace);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerDisplayFrame(hwc_rect_t frame) {
  return unsupported(__func__, frame);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerPlaneAlpha(float alpha) {
  return unsupported(__func__, alpha);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerSidebandStream(
    const native_handle_t *stream) {
  return unsupported(__func__, stream);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerSourceCrop(hwc_frect_t crop) {
  return unsupported(__func__, crop);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerSurfaceDamage(hwc_region_t damage) {
  return unsupported(__func__, damage);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerTransform(int32_t transform) {
  return unsupported(__func__, transform);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerVisibleRegion(hwc_region_t visible) {
  return unsupported(__func__, visible);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerZOrder(uint32_t z) {
  return unsupported(__func__, z);
}

// static
int DrmHwcTwo::HookDevClose(hw_device_t * /*dev*/) {
  unsupported(__func__);
  return 0;
}

// static
void DrmHwcTwo::HookDevGetCapabilities(hwc2_device_t * /*dev*/,
                                        uint32_t *out_count,
                                        int32_t * /*out_capabilities*/) {
  unsupported(__func__);
  *out_count = 0;
}

// static
hwc2_function_pointer_t DrmHwcTwo::HookDevGetFunction(struct hwc2_device *dev,
                                                       int32_t descriptor) {
  DrmHwcTwo *hwc = toDrmHwcTwo(dev);
  HWC2::FunctionDescriptor func = (HWC2::FunctionDescriptor)descriptor;

  switch (func) {
    // Device functions
    case HWC2::FunctionDescriptor::CreateVirtualDisplay:
      return ToHook<HWC2_PFN_CREATE_VIRTUAL_DISPLAY>(
          DeviceHook<int32_t, decltype(&DrmHwcTwo::CreateVirtualDisplay),
                     &DrmHwcTwo::CreateVirtualDisplay, uint32_t, uint32_t,
                     int32_t*, hwc2_display_t *>);
    case HWC2::FunctionDescriptor::DestroyVirtualDisplay:
      return ToHook<HWC2_PFN_DESTROY_VIRTUAL_DISPLAY>(
          DeviceHook<int32_t, decltype(&DrmHwcTwo::DestroyVirtualDisplay),
                     &DrmHwcTwo::DestroyVirtualDisplay, hwc2_display_t>);
    case HWC2::FunctionDescriptor::Dump:
      return ToHook<HWC2_PFN_DUMP>(
          DeviceHook<void, decltype(&DrmHwcTwo::Dump), &DrmHwcTwo::Dump,
                     uint32_t *, char *>);
    case HWC2::FunctionDescriptor::GetMaxVirtualDisplayCount:
      return ToHook<HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT>(
          DeviceHook<uint32_t, decltype(&DrmHwcTwo::GetMaxVirtualDisplayCount),
                     &DrmHwcTwo::GetMaxVirtualDisplayCount>);
    case HWC2::FunctionDescriptor::RegisterCallback:
      return ToHook<HWC2_PFN_REGISTER_CALLBACK>(
          DeviceHook<int32_t, decltype(&DrmHwcTwo::RegisterCallback),
                     &DrmHwcTwo::RegisterCallback, int32_t,
                     hwc2_callback_data_t, hwc2_function_pointer_t>);

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
          LayerHook<decltype(&HwcLayer::SetCursorPosition),
                    &HwcLayer::SetCursorPosition, int32_t, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerBlendMode:
      return ToHook<HWC2_PFN_SET_LAYER_BLEND_MODE>(
          LayerHook<decltype(&HwcLayer::SetLayerBlendMode),
                    &HwcLayer::SetLayerBlendMode, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerBuffer:
      return ToHook<HWC2_PFN_SET_LAYER_BUFFER>(
          LayerHook<decltype(&HwcLayer::SetLayerBuffer),
                    &HwcLayer::SetLayerBuffer, buffer_handle_t, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerColor:
      return ToHook<HWC2_PFN_SET_LAYER_COLOR>(
          LayerHook<decltype(&HwcLayer::SetLayerColor),
                    &HwcLayer::SetLayerColor, hwc_color_t>);
    case HWC2::FunctionDescriptor::SetLayerCompositionType:
      return ToHook<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(
          LayerHook<decltype(&HwcLayer::SetLayerCompositionType),
                    &HwcLayer::SetLayerCompositionType, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerDataspace:
      return ToHook<HWC2_PFN_SET_LAYER_DATASPACE>(
          LayerHook<decltype(&HwcLayer::SetLayerDataspace),
                    &HwcLayer::SetLayerDataspace, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerDisplayFrame:
      return ToHook<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(
          LayerHook<decltype(&HwcLayer::SetLayerDisplayFrame),
                    &HwcLayer::SetLayerDisplayFrame, hwc_rect_t>);
    case HWC2::FunctionDescriptor::SetLayerPlaneAlpha:
      return ToHook<HWC2_PFN_SET_LAYER_PLANE_ALPHA>(
          LayerHook<decltype(&HwcLayer::SetLayerPlaneAlpha),
                    &HwcLayer::SetLayerPlaneAlpha, float>);
    case HWC2::FunctionDescriptor::SetLayerSidebandStream:
      return ToHook<HWC2_PFN_SET_LAYER_SIDEBAND_STREAM>(LayerHook<
          decltype(&HwcLayer::SetLayerSidebandStream),
          &HwcLayer::SetLayerSidebandStream, const native_handle_t *>);
    case HWC2::FunctionDescriptor::SetLayerSourceCrop:
      return ToHook<HWC2_PFN_SET_LAYER_SOURCE_CROP>(
          LayerHook<decltype(&HwcLayer::SetLayerSourceCrop),
                    &HwcLayer::SetLayerSourceCrop, hwc_frect_t>);
    case HWC2::FunctionDescriptor::SetLayerSurfaceDamage:
      return ToHook<HWC2_PFN_SET_LAYER_SURFACE_DAMAGE>(
          LayerHook<decltype(&HwcLayer::SetLayerSurfaceDamage),
                    &HwcLayer::SetLayerSurfaceDamage, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetLayerTransform:
      return ToHook<HWC2_PFN_SET_LAYER_TRANSFORM>(
          LayerHook<decltype(&HwcLayer::SetLayerTransform),
                    &HwcLayer::SetLayerTransform, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerVisibleRegion:
      return ToHook<HWC2_PFN_SET_LAYER_VISIBLE_REGION>(
          LayerHook<decltype(&HwcLayer::SetLayerVisibleRegion),
                    &HwcLayer::SetLayerVisibleRegion, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetLayerZOrder:
      return ToHook<HWC2_PFN_SET_LAYER_Z_ORDER>(
          LayerHook<decltype(&HwcLayer::SetLayerZOrder),
                    &HwcLayer::SetLayerZOrder, uint32_t>);
    default:
      return NULL;
  }
}
}
