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

#include <hardware/hwcomposer2.h>

#include <map>

namespace android {

class DrmHwcTwo : public hwc2_device_t {
 public:
  DrmHwcTwo();

 private:
  class HwcLayer {
   public:
    HWC2::Error SetCursorPosition(int32_t x, int32_t y);
    HWC2::Error SetLayerBlendMode(int32_t mode);
    HWC2::Error SetLayerBuffer(buffer_handle_t buffer, int32_t acquire_fence);
    HWC2::Error SetLayerColor(hwc_color_t color);
    HWC2::Error SetLayerCompositionType(int32_t type);
    HWC2::Error SetLayerDataspace(int32_t dataspace);
    HWC2::Error SetLayerDisplayFrame(hwc_rect_t frame);
    HWC2::Error SetLayerPlaneAlpha(float alpha);
    HWC2::Error SetLayerSidebandStream(const native_handle_t *stream);
    HWC2::Error SetLayerSourceCrop(hwc_frect_t crop);
    HWC2::Error SetLayerSurfaceDamage(hwc_region_t damage);
    HWC2::Error SetLayerTransform(int32_t transform);
    HWC2::Error SetLayerVisibleRegion(hwc_region_t visible);
    HWC2::Error SetLayerZOrder(uint32_t z);
  };

  class HwcDisplay {
   public:
    // HWC Hooks
    HWC2::Error AcceptDisplayChanges();
    HWC2::Error CreateLayer(hwc2_layer_t *layer);
    HWC2::Error DestroyLayer(hwc2_layer_t layer);
    HWC2::Error GetActiveConfig(hwc2_config_t *config);
    HWC2::Error GetChangedCompositionTypes(uint32_t *num_elements,
                                           hwc2_layer_t *layers,
                                           int32_t *types);
    HWC2::Error GetClientTargetSupport(uint32_t width, uint32_t height,
                                       int32_t format, int32_t dataspace);
    HWC2::Error GetColorModes(uint32_t *num_modes, int32_t *modes);
    HWC2::Error GetDisplayAttribute(hwc2_config_t config, int32_t attribute,
                                    int32_t *value);
    HWC2::Error GetDisplayConfigs(uint32_t *num_configs,
                                  hwc2_config_t *configs);
    HWC2::Error GetDisplayName(uint32_t *size, char *name);
    HWC2::Error GetDisplayRequests(int32_t *display_requests,
                                   uint32_t *num_elements, hwc2_layer_t *layers,
                                   int32_t *layer_requests);
    HWC2::Error GetDisplayType(int32_t *type);
    HWC2::Error GetDozeSupport(int32_t *support);
    HWC2::Error GetReleaseFences(uint32_t *num_elements, hwc2_layer_t *layers,
                                 int32_t *fences);
    HWC2::Error PresentDisplay(int32_t *retire_fence);
    HWC2::Error SetActiveConfig(hwc2_config_t config);
    HWC2::Error SetClientTarget(buffer_handle_t target, int32_t acquire_fence,
                                int32_t dataspace, hwc_region_t damage);
    HWC2::Error SetColorMode(int32_t mode);
    HWC2::Error SetColorTransform(const float *matrix, int32_t hint);
    HWC2::Error SetOutputBuffer(buffer_handle_t buffer, int32_t release_fence);
    HWC2::Error SetPowerMode(int32_t mode);
    HWC2::Error SetVsyncEnabled(int32_t enabled);
    HWC2::Error ValidateDisplay(uint32_t *num_types, uint32_t *num_requests);
    HwcLayer &get_layer(hwc2_layer_t layer) {
      return layers_.at(layer);
    }

   private:
    std::map<hwc2_layer_t, HwcLayer> layers_;
  };

  static DrmHwcTwo *toDrmHwcTwo(hwc2_device_t *dev) {
    return static_cast<DrmHwcTwo *>(dev);
  }

  template <typename PFN, typename T>
  static hwc2_function_pointer_t ToHook(T function) {
    static_assert(std::is_same<PFN, T>::value, "Incompatible fn pointer");
    return reinterpret_cast<hwc2_function_pointer_t>(function);
  }

  template <typename T, typename HookType, HookType func, typename... Args>
  static T DeviceHook(hwc2_device_t *dev, Args... args) {
    DrmHwcTwo *hwc = toDrmHwcTwo(dev);
    return static_cast<T>(((*hwc).*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t DisplayHook(hwc2_device_t *dev, hwc2_display_t display_handle,
                             Args... args) {
    DrmHwcTwo *hwc = toDrmHwcTwo(dev);
    HwcDisplay &display = hwc->displays_.at(display_handle);
    return static_cast<int32_t>((display.*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t LayerHook(hwc2_device_t *dev, hwc2_display_t display_handle,
                           hwc2_layer_t layer_handle, Args... args) {
    DrmHwcTwo *hwc = toDrmHwcTwo(dev);
    HwcDisplay &display = hwc->displays_.at(display_handle);
    HwcLayer &layer = display.get_layer(layer_handle);
    return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
  }

  // hwc2_device_t hooks
  static int HookDevClose(hw_device_t *dev);
  static void HookDevGetCapabilities(hwc2_device_t *dev, uint32_t *out_count,
                                     int32_t *out_capabilities);
  static hwc2_function_pointer_t HookDevGetFunction(struct hwc2_device *device,
                                                    int32_t descriptor);

  // Device functions
  HWC2::Error CreateVirtualDisplay(uint32_t width, uint32_t height,
                                   int32_t *format,
                                   hwc2_display_t *display);
  HWC2::Error DestroyVirtualDisplay(hwc2_display_t display);
  void Dump(uint32_t *size, char *buffer);
  uint32_t GetMaxVirtualDisplayCount();
  HWC2::Error RegisterCallback(int32_t descriptor, hwc2_callback_data_t data,
                               hwc2_function_pointer_t function);

  std::map<hwc2_display_t, HwcDisplay> displays_;
};
}
