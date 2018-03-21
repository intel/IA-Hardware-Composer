/*
// Copyright (c) 2018 Intel Corporation
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

#ifndef __HWC_SHIM_H__
#define __HWC_SHIM_H__

#include <dlfcn.h>
#include <cutils/log.h>
#include <assert.h>

extern "C" {
#include "hardware/hardware.h"
#include "hardware/hwcomposer2.h"
#include <hardware/gralloc.h>
}

#include <xf86drm.h>      //< For structs and types.
#include <xf86drmMode.h>  //< For structs and types.

#include "HwcDrmShimCallback.h"
#include "HwcShimInitializer.h"
#include "HwcTestDefs.h"

#include <binder/IServiceManager.h>
#include <utils/String16.h>
#include <string>
#include <utils/SystemClock.h>
#include <utils/Vector.h>

#include "HwcvalHwc2.h"

// Real Hwc service and log
//#include <HwcService.h>
//#include <IDiagnostic.h>

//#include "hwc_shim_binder.h"

class HwcShimService;
class HwcTestState;
class HwcShim;
class CHwcFrameControl;

struct ShimHwcProcs {
  HwcShim *shim;
};

class HwcShim : hwc2_device, public HwcShimInitializer {
 public:
  /// TODO set this form test
  /// Max call time
  uint64_t callTimeThreshold;
  /// Used to store time before call
  uint64_t callTimeStart;

 private:
  /// Initialize Hwc shim
  int HwcShimInit(void);
  int HwcShimInitDrivers(HwcTestState *state);

  /// Constructor
  HwcShim(const hw_module_t *module);
  /// Destructor
  virtual ~HwcShim();

  // For hooks into real HWC
  /// TODO used?
  void *mLibHwcHandle;

  /// Pointer to hwc device struct
  hwc2_device *hwc_composer_device;
  /// pointer to hw_dev handle to hwc
  hw_device_t *hw_dev;

  /// HWC2 interface to test kernel
  Hwcval::Hwc2 *mHwc2;

  /// Cast pointer to device struct to HwcShim class
  static HwcShim *GetComposerShim(hwc2_device *dev) {
    // ALOG_ASSERT(dev);
    return static_cast<HwcShim *>(dev);
  }

  // Call time functions to check the call time of real HWC and drm
  // functions
  /// Record time before function call
  void StartCallTime(void);
  /// Check length of call time
  void EndCallTime(const char *function);

 public:
  /// Implementation of Real HWC - HookOpen
  static int HookOpen(const hw_module_t *module, const char *name,
                      hw_device_t **hw_device);
  /// Implementation of Real HWC - HookClose
  static int HookClose(struct hw_device_t *device);

  /// Implementation of Real HWC - OnEventControl
  int OnEventControl(int disp, int event, int enabled);

  int EnableVSync(int disp, bool enable);

 public:
  /// Complete initialization of shim in DRM mode
  virtual void HwcShimInitDrm(void);

 private:
  static hwc2_function_pointer_t HookDevGetFunction(struct hwc2_device *dev,
                                                    int32_t descriptor);

  /// Get pointer to functions in Real drm
  int GetFunctionPointer(void *LibHandle, const char *Symbol,
                         void **FunctionHandle, uint32_t debug);
  static int HookPresentDisplay(hwcval_display_contents_t* displays, hwc2_device_t *device, hwc2_display_t display,
                                int32_t *outPresentFence);
  static int HookCreateVirtualDisplay(
      hwc2_device_t *device, uint32_t width, uint32_t height,
      int32_t * /*android_pixel_format_t*/ format, hwc2_display_t *outDisplay);
  static int HookDestroyVirtualDisplay(hwc2_device_t *device,
                                       hwc2_display_t display);
  static int HookGetMaxVirtualDisplayCount(hwc2_device_t *device);
  static void HookDump(hwc2_device_t *device, uint32_t *outSize,
                       char *outBuffer);
  static int HookRegisterCallback(
      hwc2_device_t *device, int32_t /*hwc2_callback_descriptor_t*/ descriptor,
      hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer);
  static int HookAcceptDisplayChanges(hwc2_device_t *device,
                                      hwc2_display_t display);
  static int HookCreateLayer(hwc2_device_t *device, hwc2_display_t display,
                             hwc2_layer_t *outLayer);
  static int HookDestroyLayer(hwc2_device_t *device, hwc2_display_t display,
                              hwc2_layer_t Layer);
  static int HookGetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_config_t *outConfig);
  static int HookGetChangedCompositionType(
      hwc2_device_t *device, hwc2_display_t display, uint32_t *outNumElements,
      hwc2_layer_t *outLayers, int32_t * /*hwc2_composition_t*/ outTypes);

  static int HookGetClientTargetSupport(
      hwc2_device_t *device, hwc2_display_t display, uint32_t width,
      uint32_t height, int32_t /*android_pixel_format_t*/ format,
      int32_t /*android_dataspace_t*/ dataspace);
  static int HookGetColorMode(hwc2_device_t *device, hwc2_display_t display,
                              uint32_t *outNumModes,
                              int32_t * /*android_color_mode_t*/ outModes);
  static int HookGetDisplayAttribute(hwc2_device_t *device,
                                     hwc2_display_t display,
                                     hwc2_config_t config,
                                     int32_t /*hwc2_attribute_t*/ attribute,
                                     int32_t *outValue);
  static int HookGetDisplayConfig(hwc2_device_t *device, hwc2_display_t display,
                                  uint32_t *outNumConfigs,
                                  hwc2_config_t *outConfigs);
  static int HookGetDisplayName(hwc2_device_t *device, hwc2_display_t display,
                                uint32_t *outSize, char *outName);
  static int HookGetDisplayRequest(
      hwc2_device_t *device, hwc2_display_t display,
      int32_t * /*hwc2_display_request_t*/ outDisplayRequests,
      uint32_t *outNumElements, hwc2_layer_t *outLayers,
      int32_t * /*hwc2_layer_request_t*/ outLayerRequests);
  static int HookGetDisplayType(hwc2_device_t *device, hwc2_display_t display,
                                int32_t * /*hwc2_display_type_t*/ outType);
  static int HookGetDoseSupport(hwc2_device_t *device, hwc2_display_t display,
                                int32_t *outSupport);
  static int HookGetHDRCapabalities(
      hwc2_device_t *device, hwc2_display_t display, uint32_t *outNumTypes,
      int32_t * /*android_hdr_t*/ outTypes, float *outMaxLuminance,
      float *outMaxAverageLuminance, float *outMinLuminance);
  static int HookGetReleaseFences(hwc2_device_t *device, hwc2_display_t display,
                                  uint32_t *outNumElements,
                                  hwc2_layer_t *outLayers, int32_t *outFences);
  static int HookSetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_config_t config);
  static int HookSetClientTarget(hwc2_device_t *device, hwc2_display_t display,
                                 buffer_handle_t target, int32_t acquireFence,
                                 int32_t /*android_dataspace_t*/ dataspace,
                                 hwc_region_t damage);
  static int HookSetColorMode(hwc2_device_t *device, hwc2_display_t display,
                              int32_t /*android_color_mode_t*/ mode);
  static int HookSetColorTransform(hwc2_device_t *device,
                                   hwc2_display_t display, const float *matrix,
                                   int32_t /*android_color_transform_t*/ hint);
  static int HookSetOutputBuffer(hwc2_device_t *device, hwc2_display_t display,
                                 buffer_handle_t buffer, int32_t releaseFence);
  static int HookSetPowerMode(hwc2_device_t *device, hwc2_display_t display,
                              int32_t /*hwc2_power_mode_t*/ mode);
  static int HookSetVsyncEnabled(hwc2_device_t *device, hwc2_display_t display,
                                 int32_t /*hwc2_vsync_t*/ enabled);
  static int HookValidateDisplay(hwc2_device_t *device, hwc2_display_t display,
                                 uint32_t *outNumTypes,
                                 uint32_t *outNumRequests);
  static int HookSetCursorPosition(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_layer_t layer,
                                   int32_t x, int32_t y);
  static int HookSetLayerBuffer(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer, buffer_handle_t buffer,
                                int32_t acquireFence);
  static int HookSetSurfaceDamage(hwc2_device_t *device, hwc2_display_t display,
                                  hwc2_layer_t layer, hwc_region_t damage);
  static int HookSetLayerBlendMode(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_layer_t layer,
                                   int32_t /*hwc2_blend_mode_t*/ mode);
  static int HookSetLayerColor(hwc2_device_t *device, hwc2_display_t display,
                               hwc2_layer_t layer, hwc_color_t color);
  static int HookSetLayerCompositionType(hwc2_device_t *device,
                                         hwc2_display_t display,
                                         hwc2_layer_t layer,
                                         int32_t /*hwc2_composition_t*/ type);
  static int HookSetLayerDataSpace(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_layer_t layer,
                                   int32_t /*android_dataspace_t*/ dataspace);
  static int HookSetLayerDisplayFrame(hwc2_device_t *device,
                                      hwc2_display_t display,
                                      hwc2_layer_t layer, hwc_rect_t frame);
  static int HookSetLayerPlaneAlpha(hwc2_device_t *device,
                                    hwc2_display_t display, hwc2_layer_t layer,
                                    float alpha);
  static int HookSetLayerSideBandStream(hwc2_device_t *device,
                                        hwc2_display_t display,
                                        hwc2_layer_t layer,
                                        const native_handle_t *stream);
  static int HookSetLayerSourceCrop(hwc2_device_t *device,
                                    hwc2_display_t display, hwc2_layer_t layer,
                                    hwc_frect_t crop);
  static int HookSetLayerSourceTransform(hwc2_device_t *device,
                                         hwc2_display_t display,
                                         hwc2_layer_t layer,
                                         int32_t /*hwc_transform_t*/ transform);
  static int HookSetLayerVisibleRegion(hwc2_device_t *device,
                                       hwc2_display_t display,
                                       hwc2_layer_t layer,
                                       hwc_region_t visible);
  static int HookSetLayerZOrder(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer, uint32_t z);
  int OnCreateVirtualDisplay(hwc2_device_t *device, uint32_t width,
                             uint32_t height,
                             int32_t * /*android_pixel_format_t*/ format,
                             hwc2_display_t *outDisplay);
  int OnDestroyVirtualDisplay(hwc2_device_t *device, hwc2_display_t display);
  int OnGetMaxVirtualDisplayCount(hwc2_device_t *device);
  void OnDump(hwc2_device_t *device, uint32_t *outSize, char *outBuffer);
  int OnRegisterCallback(hwc2_device_t *device,
                         int32_t /*hwc2_callback_descriptor_t*/ descriptor,
                         hwc2_callback_data_t callbackData,
                         hwc2_function_pointer_t pointer);
  int OnAcceptDisplayChanges(hwc2_device_t *device, hwc2_display_t display);
  int OnCreateLayer(hwc2_device_t *device, hwc2_display_t display,
                    hwc2_layer_t *outLayer);
  int OnDestroyLayer(hwc2_device_t *device, hwc2_display_t display,
                     hwc2_layer_t Layer);
  int OnGetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                        hwc2_config_t *outConfig);
  int OnGetChangedCompositionType(hwc2_device_t *device, hwc2_display_t display,
                                  uint32_t *outNumElements,
                                  hwc2_layer_t *outLayers,
                                  int32_t * /*hwc2_composition_t*/ outTypes);

  int OnGetClientTargetSupport(hwc2_device_t *device, hwc2_display_t display,
                               uint32_t width, uint32_t height,
                               int32_t /*android_pixel_format_t*/ format,
                               int32_t /*android_dataspace_t*/ dataspace);
  int OnGetColorMode(hwc2_device_t *device, hwc2_display_t display,
                     uint32_t *outNumModes,
                     int32_t * /*android_color_mode_t*/ outModes);
  int OnGetDisplayAttribute(hwc2_device_t *device, hwc2_display_t display,
                            hwc2_config_t config,
                            int32_t /*hwc2_attribute_t*/ attribute,
                            int32_t *outValue);
  int OnGetDisplayConfig(hwc2_device_t *device, hwc2_display_t display,
                         uint32_t *outNumConfigs, hwc2_config_t *outConfigs);
  int OnGetDisplayName(hwc2_device_t *device, hwc2_display_t display,
                       uint32_t *outSize, char *outName);
  int OnGetDisplayRequest(
      hwc2_device_t *device, hwc2_display_t display,
      int32_t * /*hwc2_display_request_t*/ outDisplayRequests,
      uint32_t *outNumElements, hwc2_layer_t *outLayers,
      int32_t * /*hwc2_layer_request_t*/ outLayerRequests);
  int OnGetDisplayType(hwc2_device_t *device, hwc2_display_t display,
                       int32_t * /*hwc2_display_type_t*/ outType);
  int OnGetDoseSupport(hwc2_device_t *device, hwc2_display_t display,
                       int32_t *outSupport);
  int OnGetHDRCapabalities(hwc2_device_t *device, hwc2_display_t display,
                           uint32_t *outNumTypes,
                           int32_t * /*android_hdr_t*/ outTypes,
                           float *outMaxLuminance,
                           float *outMaxAverageLuminance,
                           float *outMinLuminance);
  int OnGetReleaseFences(hwc2_device_t *device, hwc2_display_t display,
                         uint32_t *outNumElements, hwc2_layer_t *outLayers,
                         int32_t *outFences);
  int OnSetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                        hwc2_config_t config);
  int OnSetClientTarget(hwc2_device_t *device, hwc2_display_t display,
                        buffer_handle_t target, int32_t acquireFence,
                        int32_t /*android_dataspace_t*/ dataspace,
                        hwc_region_t damage);
  int OnSetColorMode(hwc2_device_t *device, hwc2_display_t display,
                     int32_t /*android_color_mode_t*/ mode);
  int OnSetColorTransform(hwc2_device_t *device, hwc2_display_t display,
                          const float *matrix,
                          int32_t /*android_color_transform_t*/ hint);
  int OnSetOutputBuffer(hwc2_device_t *device, hwc2_display_t display,
                        buffer_handle_t buffer, int32_t releaseFence);
  int OnSetPowerMode(hwc2_device_t *device, hwc2_display_t display,
                     int32_t /*hwc2_power_mode_t*/ mode);
  int OnSetVsyncEnabled(hwc2_device_t *device, hwc2_display_t display,
                        int32_t /*hwc2_vsync_t*/ enabled);
  int OnValidateDisplay(hwc2_device_t *device, hwc2_display_t display,
                        uint32_t *outNumTypes, uint32_t *outNumRequests);
  int OnSetCursorPosition(hwc2_device_t *device, hwc2_display_t display,
                          hwc2_layer_t layer, int32_t x, int32_t y);
  int OnSetLayerBuffer(hwc2_device_t *device, hwc2_display_t display,
                       hwc2_layer_t layer, buffer_handle_t buffer,
                       int32_t acquireFence);
  int OnSetSurfaceDamage(hwc2_device_t *device, hwc2_display_t display,
                         hwc2_layer_t layer, hwc_region_t damage);
  int OnSetLayerBlendMode(hwc2_device_t *device, hwc2_display_t display,
                          hwc2_layer_t layer,
                          int32_t /*hwc2_blend_mode_t*/ mode);
  int OnSetLayerColor(hwc2_device_t *device, hwc2_display_t display,
                      hwc2_layer_t layer, hwc_color_t color);
  int OnSetLayerCompositionType(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer,
                                int32_t /*hwc2_composition_t*/ type);
  int OnSetLayerDataSpace(hwc2_device_t *device, hwc2_display_t display,
                          hwc2_layer_t layer,
                          int32_t /*android_dataspace_t*/ dataspace);
  int OnSetLayerDisplayFrame(hwc2_device_t *device, hwc2_display_t display,
                             hwc2_layer_t layer, hwc_rect_t frame);
  int OnSetLayerPlaneAlpha(hwc2_device_t *device, hwc2_display_t display,
                           hwc2_layer_t layer, float alpha);
  int OnSetLayerSideBandStream(hwc2_device_t *device, hwc2_display_t display,
                               hwc2_layer_t layer,
                               const native_handle_t *stream);
  int OnSetLayerSourceCrop(hwc2_device_t *device, hwc2_display_t display,
                           hwc2_layer_t layer, hwc_frect_t crop);
  int OnSetLayerSourceTransform(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer,
                                int32_t /*hwc_transform_t*/ transform);
  int OnSetLayerVisibleRegion(hwc2_device_t *device, hwc2_display_t display,
                              hwc2_layer_t layer, hwc_region_t visible);
  int OnSetLayerZOrder(hwc2_device_t *device, hwc2_display_t display,
                       hwc2_layer_t layer, uint32_t z);
  // Shim versions of HWC functions
  // function pointers for hwc functions

  /// Implementation of Real HWC - HookSet

  /// Implementation of Real HWC - HooEventControl
  static int HookEventControl(struct hwc2_device *dev, int disp, int event,
                              int enabled);
  /// Implementation of Real HWC - HookBlank
  static int HookBlank(struct hwc2_device *dev, int disp, int blank);

  /// Implementation of Real HWC - HookQuery
  static int HookQuery(struct hwc2_device *dev, int what, int *value);

  /// Implementation of Real HWC - HookDump
  // static void HookDump(struct hwc2_device *dev, char *buff, int buff_len);

  /// Implementation of Real HWC - HookGetDisplayConfigs
  static int HookGetDisplayConfigs(struct hwc2_device *dev, int disp,
                                   uint32_t *configs, size_t *numConfigs);

  /// Implementation of Real HWC - HookGetActiveConfig
  // static int HookGetActiveConfig(struct hwc2_device *dev, int disp);

  /// Implementation of Real HWC - HookGetDisplayAtrtributes
  static int HookGetDisplayAttributes(struct hwc2_device *dev, int disp,
                                      uint32_t config,
                                      const uint32_t *attributes,
                                      int32_t *values);

  // function pointers to real HWC composer
  /// Implementation of Real HWC - OnDump
  void OnDump(char *buff, int buff_len);

  /// Implementation of Real HWC - OnGetDisplayConfigs
  int OnGetDisplayConfigs(int disp, uint32_t *configs, size_t *numConfigs);

  /// Implementation of Real HWC - OnGetActiveConfig
  int OnGetActiveConfig(int disp);

  /// Implementation of Real HWC - OnGetDisplayAttributes
  int OnGetDisplayAttributes(int disp, uint32_t config, const int32_t attribute,
                             int32_t *values);

  int OnPresentDisplay(hwcval_display_contents_t *displays,
                       hwc2_device_t *device, hwc2_display_t display,
                       int32_t *outPresentFence);

 private:
  ShimHwcProcs mShimProcs;
  HwcDrmShimCallback mDrmShimCallback;
};

#endif
