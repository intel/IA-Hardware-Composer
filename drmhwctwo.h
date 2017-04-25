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

#include "drmdisplaycompositor.h"
#include "drmhwcomposer.h"
#include "drmresources.h"
#include "platform.h"
#include "vsyncworker.h"

#include <hardware/hwcomposer2.h>

#include <map>

namespace android {

class DrmHwcTwo : public hwc2_device_t {
 public:
  static int HookDevOpen(const struct hw_module_t *module, const char *name,
                         struct hw_device_t **dev);

  DrmHwcTwo();

  HWC2::Error Init();

 private:
  class HwcLayer {
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
      return z_order_;
    }

    buffer_handle_t buffer() {
      return buffer_;
    }
    void set_buffer(buffer_handle_t buffer) {
      buffer_ = buffer;
    }

    int take_acquire_fence() {
      return acquire_fence_.Release();
    }
    void set_acquire_fence(int acquire_fence) {
      acquire_fence_.Set(dup(acquire_fence));
    }

    int release_fence() {
      return release_fence_.get();
    }
    int take_release_fence() {
      return release_fence_.Release();
    }
    void manage_release_fence() {
      release_fence_.Set(release_fence_raw_);
      release_fence_raw_ = -1;
    }
    OutputFd release_fence_output() {
      return OutputFd(&release_fence_raw_);
    }

    void PopulateDrmLayer(DrmHwcLayer *layer);

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

   private:
    // sf_type_ stores the initial type given to us by surfaceflinger,
    // validated_type_ stores the type after running ValidateDisplay
    HWC2::Composition sf_type_ = HWC2::Composition::Invalid;
    HWC2::Composition validated_type_ = HWC2::Composition::Invalid;

    HWC2::BlendMode blending_ = HWC2::BlendMode::None;
    buffer_handle_t buffer_;
    UniqueFd acquire_fence_;
    int release_fence_raw_ = -1;
    UniqueFd release_fence_;
    hwc_rect_t display_frame_;
    float alpha_ = 1.0f;
    hwc_frect_t source_crop_;
    HWC2::Transform transform_ = HWC2::Transform::None;
    uint32_t z_order_ = 0;
    android_dataspace_t dataspace_ = HAL_DATASPACE_UNKNOWN;
  };

  struct HwcCallback {
    HwcCallback(hwc2_callback_data_t d, hwc2_function_pointer_t f)
        : data(d), func(f) {
    }
    hwc2_callback_data_t data;
    hwc2_function_pointer_t func;
  };

  class HwcDisplay {
   public:
    HwcDisplay(DrmResources *drm, std::shared_ptr<Importer> importer,
               const gralloc_module_t *gralloc, hwc2_display_t handle,
               HWC2::DisplayType type);
    HwcDisplay(const HwcDisplay &) = delete;
    HWC2::Error Init(std::vector<DrmPlane *> *planes);

    HWC2::Error RegisterVsyncCallback(hwc2_callback_data_t data,
                                      hwc2_function_pointer_t func);

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
    HWC2::Error SetColorTransform(const float *matrix, int32_t hint);
    HWC2::Error SetOutputBuffer(buffer_handle_t buffer, int32_t release_fence);
    HWC2::Error SetPowerMode(int32_t mode);
    HWC2::Error SetVsyncEnabled(int32_t enabled);
    HWC2::Error ValidateDisplay(uint32_t *num_types, uint32_t *num_requests);
    HwcLayer &get_layer(hwc2_layer_t layer) {
      return layers_.at(layer);
    }

   private:
    void AddFenceToRetireFence(int fd);

    DrmResources *drm_;
    DrmDisplayCompositor compositor_;
    std::shared_ptr<Importer> importer_;
    std::unique_ptr<Planner> planner_;
    const gralloc_module_t *gralloc_;

    std::vector<DrmPlane *> primary_planes_;
    std::vector<DrmPlane *> overlay_planes_;

    VSyncWorker vsync_worker_;
    DrmConnector *connector_ = NULL;
    DrmCrtc *crtc_ = NULL;
    hwc2_display_t handle_;
    HWC2::DisplayType type_;
    uint32_t layer_idx_ = 0;
    std::map<hwc2_layer_t, HwcLayer> layers_;
    HwcLayer client_layer_;
    UniqueFd retire_fence_;
    UniqueFd next_retire_fence_;

    uint32_t frame_no_ = 0;
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

  DrmResources drm_;
  std::shared_ptr<Importer> importer_;  // Shared with HwcDisplay
  const gralloc_module_t *gralloc_;
  std::map<hwc2_display_t, HwcDisplay> displays_;
  std::map<HWC2::Callback, HwcCallback> callbacks_;
};
}
