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

#ifndef OS_ANDROID_IAHWC2_H_
#define OS_ANDROID_IAHWC2_H_

#include <hardware/hwcomposer2.h>

#include <gpudevice.h>
#include <hwclayer.h>
#include <platformdefines.h>

#include <map>
#include <utility>

#include "hwcservice.h"

/*we have two extend displays,the seconde one's take over virtual
display ID slot.to simplify ID management,start the virtual display
ID from 4(HWC_DISPLAY_VIRTUAL+VDS_OFFSET)*/
#define VDS_OFFSET 2

namespace hwcomposer {
class GpuDevice;
class NativeDisplay;
}  // namespace hwcomposer

namespace android {
class HwcService;
class IAHWC2 : public hwc2_device_t {
 public:
  static int HookDevOpen(const struct hw_module_t *module, const char *name,
                         struct hw_device_t **dev);

  IAHWC2();

  HWC2::Error Init();
  hwcomposer::NativeDisplay *GetPrimaryDisplay();
  hwcomposer::NativeDisplay *GetExtendedDisplay(uint32_t);

  void EnableHDCPSessionForDisplay(uint32_t connector,
                                   EHwcsContentType content_type);

  void EnableHDCPSessionForAllDisplays(EHwcsContentType content_type);

  void DisableHDCPSessionForDisplay(uint32_t connector);

  void DisableHDCPSessionForAllDisplays();
#ifdef ENABLE_PANORAMA
  void TriggerPanorama(uint32_t hotplug_simulation);
  void ShutdownPanorama(uint32_t hotplug_simulation);
#endif

  void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                            uint32_t pavp_instance_id);
  void SetHDCPSRMForAllDisplays(const int8_t *SRM, uint32_t SRMLength);

  void SetHDCPSRMForDisplay(uint32_t connector, const int8_t *SRM,
                            uint32_t SRMLength);
  uint32_t GetDisplayIDFromConnectorID(const uint32_t connector_id);

  bool EnableDRMCommit(bool enable, uint32_t display_id);

  bool ResetDrmMaster(bool drop_master);

 public:
  class Hwc2Layer {
   public:
    HWC2::Composition sf_type() const {
      return sf_type_;
    }
    HWC2::Composition validated_type() const {
      return validated_type_;
    }
    void accept_type_change() {
      sf_type_ = validated_type_;
    }
    void set_validated_type(HWC2::Composition type) {
      validated_type_ = type;
    }
    bool type_changed() const {
      return sf_type_ != validated_type_;
    }

    uint32_t z_order() const {
      return hwc_layer_.GetZorder();
    }

    void set_buffer(buffer_handle_t buffer) {
      native_handle_.handle_ = buffer;
      hwc_layer_.SetNativeHandle(&native_handle_);
    }

    void XTranslateCoordinates(uint32_t x_translation) {
      x_translation_ = x_translation;
    }

    void YTranslateCoordinates(uint32_t y_translation) {
      y_translation_ = y_translation;
    }

    void set_acquire_fence(int acquire_fence) {
      if (acquire_fence > 0)
        hwc_layer_.SetAcquireFence(acquire_fence);
    }

    hwcomposer::HwcLayer *GetLayer() {
      return &hwc_layer_;
    }

    bool IsCursorLayer() const {
      return hwc_layer_.IsCursorLayer();
    }

    bool IsVideoLayer() const {
      return hwc_layer_.IsVideoLayer();
    }

    // Layer hooks
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
    HWC2::Error SetLayerColorTransform(const float *matrix);
    HWC2::Error SetLayerPerFrameMetadataBlobs(uint32_t numElements,
                                              const int32_t *keys,
                                              const uint32_t *sizes,
                                              const uint8_t *metadata);

   private:
    // sf_type_ stores the initial type given to us by surfaceflinger,
    // validated_type_ stores the type after running ValidateDisplay
    HWC2::Composition sf_type_ = HWC2::Composition::Invalid;
    HWC2::Composition validated_type_ = HWC2::Composition::Invalid;
    android_dataspace_t dataspace_ = HAL_DATASPACE_UNKNOWN;
    hwcomposer::HwcLayer hwc_layer_;
    struct gralloc_handle native_handle_;
    uint32_t x_translation_ = 0;
    uint32_t y_translation_ = 0;
  };

  class HwcDisplay {
   public:
    HwcDisplay();
    HwcDisplay(const HwcDisplay &) = delete;
    HwcDisplay &operator=(const HwcDisplay &) = delete;

    uint32_t numCap_ = 1;  // at least support the doze
    uint32_t maxNumCap_ = HWC2_DISPLAY_CAPABILITY_DOZE -
                          HWC2_DISPLAY_CAPABILITY_INVALID; /* last - first */

    uint32_t getNumCapabilities() {
      return numCap_;
    }

    void setNumCapabilities(uint32_t num) {
      numCap_ = num;
    }

    uint32_t num_intents_ = 1;  // at least support the COLORIMETRIC
    uint32_t GetNumRenderIntents() {
      return num_intents_;
    }

    HWC2::Error Init(hwcomposer::NativeDisplay *display, int display_index,
                     bool disable_explicit_sync, uint32_t scaling_mode);
    HWC2::Error InitVirtualDisplay(hwcomposer::NativeDisplay *display,
                                   uint32_t width, uint32_t height,
                                   uint32_t display_index,
                                   bool disable_explicit_sync);

    HWC2::Error RegisterVsyncCallback(hwc2_callback_data_t data,
                                      hwc2_function_pointer_t func);

    HWC2::Error RegisterRefreshCallback(hwc2_callback_data_t data,
                                        hwc2_function_pointer_t func);

    HWC2::Error RegisterHotPlugCallback(hwc2_callback_data_t data,
                                        hwc2_function_pointer_t func);

    // HWC Hooks
    void FreeAllLayers();
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
    HWC2::Error GetHdrCapabilities(uint32_t *num_types, int32_t *types,
                                   float *max_luminance,
                                   float *max_average_luminance,
                                   float *min_luminance);
    HWC2::Error GetReleaseFences(uint32_t *num_elements, hwc2_layer_t *layers,
                                 int32_t *fences);
    HWC2::Error PresentDisplay(int32_t *retire_fence);
    HWC2::Error SetActiveConfig(hwc2_config_t config);
    HWC2::Error SetClientTarget(buffer_handle_t target, int32_t acquire_fence,
                                int32_t dataspace, hwc_region_t damage);
    HWC2::Error SetColorMode(int32_t mode);
    HWC2::Error SetColorModeWithRenderIntent(int32_t mode, int32_t intent);
    HWC2::Error GetRenderIntents(int32_t mode, uint32_t *outNumIntents,
                                 int32_t *outIntents);

    HWC2::Error SetColorTransform(const float *matrix, int32_t hint);
    HWC2::Error SetOutputBuffer(buffer_handle_t buffer, int32_t release_fence);
    HWC2::Error SetPowerMode(int32_t mode);
    HWC2::Error SetVsyncEnabled(int32_t enabled);
    HWC2::Error ValidateDisplay(uint32_t *num_types, uint32_t *num_requests);
    HWC2::Error GetDisplayIdentificationData(uint8_t *outPort,
                                             uint32_t *outDataSize,
                                             uint8_t *outData);
    HWC2::Error GetDisplayCapabilities(uint32_t *outNumCapabilities,
                                       uint32_t *outCapabilities);
    HWC2::Error GetDisplayedContentSamplingAttributes(
        int32_t *format, int32_t *dataspace, uint8_t *supported_components);
    HWC2::Error SetDisplayedContentSamplingEnabled(int32_t enabled,
                                                   uint8_t component_mask,
                                                   uint64_t max_frames);
    HWC2::Error GetDisplayedContentSample(uint64_t max_frames,
                                          uint64_t timestamp,
                                          uint64_t *frame_count,
                                          int32_t *samples_size,
                                          uint64_t **samples);

    Hwc2Layer &get_layer(hwc2_layer_t layer) {
      return layers_.at(layer);
    }
    hwcomposer::NativeDisplay *GetDisplay();

   private:
    hwcomposer::NativeDisplay *display_;
    hwc2_display_t handle_;
    HWC2::DisplayType type_;
    std::map<hwc2_layer_t, Hwc2Layer> layers_;
    Hwc2Layer client_layer_;

    uint32_t frame_no_;
    // True after validateDisplay
    bool check_validate_display_;
    bool disable_explicit_sync_;
    bool enable_nested_display_compose_;
    uint32_t scaling_mode_;
    uint32_t planes_;
  };

  HWC2::Error BadDisplay() {
    return HWC2::Error::BadDisplay;
  }

  static IAHWC2 *toIAHWC2(hwc2_device_t *dev) {
    return static_cast<IAHWC2 *>(dev);
  }

  template <typename PFN, typename T>
  static hwc2_function_pointer_t ToHook(T function) {
    static_assert(std::is_same<PFN, T>::value, "Incompatible fn pointer");
    return reinterpret_cast<hwc2_function_pointer_t>(function);
  }

  template <typename T, typename HookType, HookType func, typename... Args>
  static T DeviceHook(hwc2_device_t *dev, Args... args) {
    IAHWC2 *hwc = toIAHWC2(dev);
    return static_cast<T>(((*hwc).*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t DisplayHook(hwc2_device_t *dev, hwc2_display_t display_handle,
                             Args... args) {
    IAHWC2 *hwc = toIAHWC2(dev);

    if (~(uint32_t)display_handle == 0) {
      return static_cast<int32_t>(hwc->BadDisplay());
    }

    if (display_handle == HWC_DISPLAY_PRIMARY) {
      HwcDisplay &display = hwc->primary_display_;
      return static_cast<int32_t>((display.*func)(std::forward<Args>(args)...));
    }

    if (display_handle >= (HWC_DISPLAY_VIRTUAL + VDS_OFFSET)) {
      HwcDisplay *display =
          hwc->virtual_displays_
              .at(display_handle - HWC_DISPLAY_VIRTUAL - VDS_OFFSET)
              .get();

      return static_cast<int32_t>(
          (display->*func)(std::forward<Args>(args)...));
    }

    if (display_handle == HWC_DISPLAY_EXTERNAL) {
      HwcDisplay *display = hwc->extended_displays_.at(0).get();
      return static_cast<int32_t>(
          (display->*func)(std::forward<Args>(args)...));
    }

    HwcDisplay *display = hwc->extended_displays_.at(1).get();
    return static_cast<int32_t>((display->*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t LayerHook(hwc2_device_t *dev, hwc2_display_t display_handle,
                           hwc2_layer_t layer_handle, Args... args) {
    IAHWC2 *hwc = toIAHWC2(dev);

    if (display_handle == HWC_DISPLAY_PRIMARY) {
      HwcDisplay &display = hwc->primary_display_;
      Hwc2Layer &layer = display.get_layer(layer_handle);
      return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
    }

    if (display_handle >= (HWC_DISPLAY_VIRTUAL + VDS_OFFSET)) {
      HwcDisplay *display =
          hwc->virtual_displays_
              .at(display_handle - HWC_DISPLAY_VIRTUAL - VDS_OFFSET)
              .get();
      Hwc2Layer &layer = display->get_layer(layer_handle);
      return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
    }

    if (display_handle == HWC_DISPLAY_EXTERNAL) {
      HwcDisplay *display = hwc->extended_displays_.at(0).get();
      Hwc2Layer &layer = display->get_layer(layer_handle);
      return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
    }

    HwcDisplay *display = hwc->extended_displays_.at(1).get();
    Hwc2Layer &layer = display->get_layer(layer_handle);
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
                                   int32_t *format, hwc2_display_t *display);
  HWC2::Error DestroyVirtualDisplay(hwc2_display_t display);
  void Dump(uint32_t *size, char *buffer);
  uint32_t GetMaxVirtualDisplayCount();
  HWC2::Error RegisterCallback(int32_t descriptor, hwc2_callback_data_t data,
                               hwc2_function_pointer_t function);

  hwcomposer::GpuDevice &device_ = GpuDevice::getInstance();
  std::vector<std::unique_ptr<HwcDisplay>> extended_displays_;
  HwcDisplay primary_display_;
  std::map<uint32_t, std::unique_ptr<HwcDisplay>> virtual_displays_;
  uint32_t virtual_display_index_ = 0;

  bool disable_explicit_sync_ = false;
  android::HwcService hwcService_;
  uint32_t scaling_mode_ = 0;
};
}  // namespace android

#endif  // OS_ANDROID_IAHWC2_H_
