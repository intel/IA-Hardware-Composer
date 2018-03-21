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

/**
 * \page FrameworkDetails Framework Details
 *
 * This page describes the details of the HWC test framework.
 *
 * These details are not needed to run the test and are included to provide a
 * overview of how the framework works for future development and debug.
 *
 * The low level details are not describes as these may change. The purpose is
 * to give a understanding of how the framework fits together, is built and
 * runs. So that a reader may efficiently deal with the code.
 *
 * \section Terminology Terminology
 *
 * The term "real drm" and "real hwc" are used to refer to the drm and hwc
 * normally on the system. As these are replaced by the shims referring to file
 * names maybe confusing. The terms "drm shim and hwc shim" are used to reffer
 * to the shims.
 *
 *\section Overview Overview
 *
 * The purpose of the HWC test frame is to provide a mechanism for automated
 * testing of HWC. To achieved this the frame provides a validation version of
 * libdrm.so and the HWC composer shared library. This are loaded at run time
 * in preference to the real versions of these libraries, see \ref
 * UsingTheFramework for details of how this is done. The shims then
 * dynamically load the real libraries. Calls into the real library from
 * SurfaceFlinger to HWC go via the HWC shim at which point checks can occur on
 * these calls. Similarly calls from real HWC to drm pass through the drm shim.
 * It is possible this in some cases the call to drm is not passed on to the
 * real drm and are entirely handled by the shim.
 *
 * The checks in the shims are enabled by the test. The test also provide
 * surfaces to surface flinger.
 *
 */

/** \defgroup FutureWork Future Work
 *  @{
 *  Auto detect drm information. (there is drm class in the trest tree)
 *  A abstract way supporting different HW.
 *  @}
 */

extern "C" {
#include "intel_bufmgr.h"
}

#include "hardware/hwcomposer2.h"

#include "hwc_shim.h"

#include "HwcTestDefs.h"
#include "HwcTestState.h"
#include "HwcDrmShimCallback.h"
#include "HwcTestKernel.h"
#include "HwcTestUtil.h"
#include "HwcvalThreadTable.h"

#include <sys/stat.h>

#undef LOG_TAG
#define LOG_TAG "HWC_SHIM"

HwcShim::HwcShim(const hw_module_t *module) {
  common.tag = HARDWARE_DEVICE_TAG;
  common.module = const_cast<hw_module_t *>(module);
  common.close = HookClose;
  getFunction = HookDevGetFunction;

  // load real HWC
  HWCLOGI("HwcShim::HwcShim - loading real HWC");
  HwcShimInit();

  // TODO set from test
  // TODO set some sensible value here
  // nano seconds
  callTimeThreshold = 200000000;

  mShimProcs.shim = this;

  mHwc2 = new Hwcval::Hwc2();

  HWCLOGI("HwcShim::HwcShim - returning");
}

HwcShim::~HwcShim() {
  if (mHwc2) {
    delete mHwc2;
    mHwc2 = 0;
  }

  if (state)
    delete state;
}

// Load HWC library and get hooks
// TODO move everything that can occur at construction time to the ctor
// use this for post construction settings from the test maybe rename.
int HwcShim::HwcShimInit(void) {
  // TODO turn off some logging check android levels
  HWCLOGI("HwcShim Init");

  int rc = 0;

  // Get test state object
  state = HwcTestState::getInstance();

  state->SetRunningShim(HwcTestState::eHwcShim);

  int ret = 0;
  // Load HWC and get a pointer to logger function
  dlerror();
  mLibHwcHandle =
      dll_open(HWCVAL_VENDOR_LIBPATH "/hw/hwcomposer.real.so", RTLD_NOW);
  if (!mLibHwcHandle) {
    HWCLOGW("Can't find HWC in " HWCVAL_VENDOR_LIBPATH
            ", trying " HWCVAL_LIBPATH);
    dlerror();
    mLibHwcHandle = dll_open(HWCVAL_LIBPATH "/hw/hwcomposer.real.so", RTLD_NOW);

    if (!mLibHwcHandle) {
      ret = -1;
      HWCERROR(eCheckHwcBind, "In HwcShim Init Could not open real hwc");
      ALOG_ASSERT(0);
    } else {
      HWCLOGD("HWC opened at " HWCVAL_LIBPATH "/hw/hwcomposer.real.so");
    }
  } else {
    HWCLOGD("HWC opened at " HWCVAL_VENDOR_LIBPATH "/hw/hwcomposer.real.so");
  }

  char *libError = (char *)dlerror();
  if (libError != NULL) {
    ret |= -1;
    HWCERROR(eCheckHwcBind, "In HwcShim Init Error getting mLibHwcHandle %s",
             libError);
  }

  state->LoggingInit(mLibHwcHandle);

  ret = HwcShimInitDrivers(state);

  dlerror();
  const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
  hwc_module_t *pHwcModule = (hwc_module_t *)dlsym(mLibHwcHandle, sym);

  libError = (char *)dlerror();
  if (libError != NULL) {
    ret |= -1;
    HWCERROR(eCheckHwcBind, "In HwcShim Init Error getting symbol %s", sym);
  }

  pHwcModule->common.dso = mLibHwcHandle;

  hw_dev = new hw_device_t;
  hwc_composer_device = new hwc2_device;

  // Check libraries are compatible
  mDrmShimCallback.CheckVersion();
  rc = pHwcModule->common.methods->open(
      (const hw_module_t *)&pHwcModule->common, HWC_HARDWARE_COMPOSER, &hw_dev);

  if (rc != 0) {
    HWCLOGI("Bad return code from real hwc hook_open %d", rc);
  }

  hwc_composer_device = (hwc2_device *)hw_dev;
  common.version = hwc_composer_device->common.version;

  return ret;
}

int HwcShim::HwcShimInitDrivers(HwcTestState *state) {
  HWCLOGI("Open libDrmHandle");

  int ret = 0;

  // Open drm library - this is the drm shim
  dlerror();
  void *libDrmHandle = dll_open(HWCVAL_LIBPATH "/libdrm.so", RTLD_NOW);

  if (!libDrmHandle) {
    dlerror();
    libDrmHandle = dll_open(HWCVAL_VENDOR_LIBPATH "/libdrm.so", RTLD_NOW);

    if (!libDrmHandle) {
      HWCERROR(eCheckDrmShimBind, "Failed to open DRM shim in " HWCVAL_LIBPATH
                                  " or " HWCVAL_VENDOR_LIBPATH);
      ret = -1;
      return ret;
    }
  }

  // Get function function in drm shim that are not in real drm. As we link
  // against real drm to avoid issues with libraries names at run time.
  int rc = GetFunctionPointer(libDrmHandle, (char *)"drmShimInit",
                              (void **)&drmShimFunctions.fpDrmShimInit, 0);

  if (rc) {
    HWCERROR(eCheckDrmShimBind, "Error loading drmShimInit");
    ret = -1;
  }

  HWCLOGI("fpDrmShimInit %p", drmShimFunctions.fpDrmShimInit);
  HWCLOGI("Load drm shim");
  drmShimFunctions.fpDrmShimInit(true, false);

  rc = GetFunctionPointer(
      libDrmHandle, (char *)"drmShimEnableVSyncInterception",
      (void **)&drmShimFunctions.fpDrmShimEnableVSyncInterception, 0);

  if (rc) {
    HWCERROR(eCheckDrmShimBind, "Error loading drmShimEnableVSyncInterception");
    ret = -1;
  } else {
    HWCLOGD("Got drmShimEnableVSyncInterception %p",
            drmShimFunctions.fpDrmShimEnableVSyncInterception);
  }

  rc = GetFunctionPointer(libDrmHandle, (char *)"drmShimRegisterCallback",
                          (void **)&drmShimFunctions.fpDrmShimRegisterCallback,
                          0);

  if (rc) {
    HWCERROR(eCheckDrmShimBind, "Error loading drmShimRegisterCallback");
    ret = -1;
  } else {
    HWCLOGD("Got drmShimRegisterCallback %p",
            drmShimFunctions.fpDrmShimRegisterCallback);
  }

  // load drm shim
  HwcShimInitDrm();
  state->TestStateInit(this);

  return ret;
}

void HwcShim::HwcShimInitDrm() {
  HWCLOGI("Load drm shim");
  drmShimFunctions.fpDrmShimInit(true, true);

  if (drmShimFunctions.fpDrmShimEnableVSyncInterception) {
    // This MUST happen before HWC initialization
    bool enableVSync =
        HwcTestState::getInstance()->IsOptionEnabled(eOptVSyncInterception);
    ;
    HWCLOGI("Set up DRM fd and %s VSync Interception",
            enableVSync ? "enable" : "disable");
    drmShimFunctions.fpDrmShimEnableVSyncInterception(enableVSync);
  }

  // This will enable registration for callbacks from the DRM Shim
  if (drmShimFunctions.fpDrmShimRegisterCallback) {
    drmShimFunctions.fpDrmShimRegisterCallback((void *)&mDrmShimCallback);
  }
}

int HwcShim::HookPresentDisplay(hwcval_display_contents_t* displays, hwc2_device_t *device, hwc2_display_t display,
                                int32_t *outPresentFence) {
  int ret = -1;
  ret = GetComposerShim(device)->OnPresentDisplay(displays, device, display,
                                                  outPresentFence);
  return ret;
}

int HwcShim::OnPresentDisplay(hwcval_display_contents_t* displays, hwc2_device_t *device, hwc2_display_t display,
                              int32_t *outPresentFence) {
  int ret = -1;
  mHwc2->CheckPresentDisplayEnter(displays, display);

  HWC2_PFN_PRESENT_DISPLAY pfnPresentDisplay =
      reinterpret_cast<HWC2_PFN_PRESENT_DISPLAY>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_PRESENT_DISPLAY));
  if (pfnPresentDisplay) {
    ret = pfnPresentDisplay(hwc_composer_device, display, outPresentFence);
  }

  mHwc2->CheckPresentDisplayExit(displays, display, outPresentFence);
  return ret;
}

int HwcShim::HookCreateVirtualDisplay(
    hwc2_device_t *device, uint32_t width, uint32_t height,
    int32_t * /*android_pixel_format_t*/ format, hwc2_display_t *outDisplay) {
  int ret = -1;
  ret = GetComposerShim(device)->OnCreateVirtualDisplay(device, width, height,
                                                        format, outDisplay);
  return ret;
}

int HwcShim::OnCreateVirtualDisplay(hwc2_device_t *device, uint32_t width,
                                    uint32_t height,
                                    int32_t * /*android_pixel_format_t*/ format,
                                    hwc2_display_t *outDisplay) {
  int ret = -1;

  HWC2_PFN_CREATE_VIRTUAL_DISPLAY pfnCreateVirtualDisplay =
      reinterpret_cast<HWC2_PFN_CREATE_VIRTUAL_DISPLAY>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY));
  if (pfnCreateVirtualDisplay) {
    ret = pfnCreateVirtualDisplay(hwc_composer_device, width, height, format,
                                  outDisplay);
  }

  return ret;
}

int HwcShim::HookDestroyVirtualDisplay(hwc2_device_t *device,
                                       hwc2_display_t display) {
  int ret = -1;
  ret = GetComposerShim(device)->OnDestroyVirtualDisplay(device, display);
  return ret;
}

int HwcShim::OnDestroyVirtualDisplay(hwc2_device_t *device,
                                     hwc2_display_t display) {
  int ret = -1;

  HWC2_PFN_DESTROY_VIRTUAL_DISPLAY pfnDestroyVirtualDisplay =
      reinterpret_cast<HWC2_PFN_DESTROY_VIRTUAL_DISPLAY>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY));
  if (pfnDestroyVirtualDisplay) {
    ret = pfnDestroyVirtualDisplay(hwc_composer_device, display);
  }

  return ret;
}

int HwcShim::HookGetMaxVirtualDisplayCount(hwc2_device_t *device) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetMaxVirtualDisplayCount(device);
  return ret;
}

int HwcShim::OnGetMaxVirtualDisplayCount(hwc2_device_t *device) {
  int ret = -1;

  HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT pfnGetMaxVirtualDisplayCount =
      reinterpret_cast<HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT>(
          hwc_composer_device->getFunction(
              hwc_composer_device,
              HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT));
  if (pfnGetMaxVirtualDisplayCount) {
    ret = pfnGetMaxVirtualDisplayCount(hwc_composer_device);
  }

  return ret;
}

void HwcShim::HookDump(hwc2_device_t *device, uint32_t *outSize,
                       char *outBuffer) {
  GetComposerShim(device)->OnDump(device, outSize, outBuffer);
}

void HwcShim::OnDump(hwc2_device_t *device, uint32_t *outSize,
                     char *outBuffer) {
  HWC2_PFN_DUMP pfnDump =
      reinterpret_cast<HWC2_PFN_DUMP>(hwc_composer_device->getFunction(
          hwc_composer_device, HWC2_FUNCTION_DUMP));
  if (pfnDump) {
    pfnDump(hwc_composer_device, outSize, outBuffer);
  }
}

int HwcShim::HookRegisterCallback(
    hwc2_device_t *device, int32_t /*hwc2_callback_descriptor_t*/ descriptor,
    hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer) {
  int ret = -1;
  ret = GetComposerShim(device)->OnRegisterCallback(device, descriptor,
                                                    callbackData, pointer);
  return ret;
}

int HwcShim::OnRegisterCallback(
    hwc2_device_t *device, int32_t /*hwc2_callback_descriptor_t*/ descriptor,
    hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer) {
  int ret = -1;
  HWC2_PFN_REGISTER_CALLBACK pfnRegisterCallback =
      reinterpret_cast<HWC2_PFN_REGISTER_CALLBACK>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_REGISTER_CALLBACK));
  if (pfnRegisterCallback) {
  ret = pfnRegisterCallback(hwc_composer_device, descriptor, callbackData, pointer);
  }
  return ret;
}

int HwcShim::HookAcceptDisplayChanges(hwc2_device_t *device,
                                      hwc2_display_t display) {
  int ret = -1;
  ret = GetComposerShim(device)->OnAcceptDisplayChanges(device, display);
  return ret;
}

int HwcShim::OnAcceptDisplayChanges(hwc2_device_t *device,
                                    hwc2_display_t display) {
  int ret = -1;
  HWC2_PFN_ACCEPT_DISPLAY_CHANGES pfnAcceptDisplayChanges =
      reinterpret_cast<HWC2_PFN_ACCEPT_DISPLAY_CHANGES>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES));
  if (pfnAcceptDisplayChanges) {
    ret = pfnAcceptDisplayChanges(hwc_composer_device, display);
  }
  return ret;
}

int HwcShim::HookCreateLayer(hwc2_device_t *device, hwc2_display_t display,
                             hwc2_layer_t *outLayer) {
  int ret = -1;
  ret = GetComposerShim(device)->OnCreateLayer(device, display, outLayer);
  return ret;
}

int HwcShim::OnCreateLayer(hwc2_device_t *device, hwc2_display_t display,
                           hwc2_layer_t *outLayer) {
  int ret = -1;

  HWC2_PFN_CREATE_LAYER pfnCreateLayer =
      reinterpret_cast<HWC2_PFN_CREATE_LAYER>(hwc_composer_device->getFunction(
          hwc_composer_device, HWC2_FUNCTION_CREATE_LAYER));
  if (pfnCreateLayer) {
    ret = pfnCreateLayer(hwc_composer_device, display, outLayer);
  }
  return ret;
}

int HwcShim::HookDestroyLayer(hwc2_device_t *device, hwc2_display_t display,
                              hwc2_layer_t layer) {
  int ret = -1;
  ret = GetComposerShim(device)->OnDestroyLayer(device, display, layer);
  return ret;
}

int HwcShim::OnDestroyLayer(hwc2_device_t *device, hwc2_display_t display,
                            hwc2_layer_t layer) {
  int ret = -1;

  HWC2_PFN_DESTROY_LAYER pfnDestroyLayer =
      reinterpret_cast<HWC2_PFN_DESTROY_LAYER>(hwc_composer_device->getFunction(
          hwc_composer_device, HWC2_FUNCTION_DESTROY_LAYER));
  if (pfnDestroyLayer) {
    ret = pfnDestroyLayer(hwc_composer_device, display, layer);
  }
  return ret;
}

int HwcShim::HookGetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_config_t *outConfig) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetActiveConfig(device, display, outConfig);
  return ret;
}

int HwcShim::OnGetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                               hwc2_config_t *outConfig) {
  int ret = -1;

  HWC2_PFN_GET_ACTIVE_CONFIG pfnOnGetActiveConfig =
      reinterpret_cast<HWC2_PFN_GET_ACTIVE_CONFIG>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_ACTIVE_CONFIG));
  if (pfnOnGetActiveConfig) {
    ret = pfnOnGetActiveConfig(hwc_composer_device, display, outConfig);
  }
  return ret;
}

int HwcShim::HookGetChangedCompositionType(
    hwc2_device_t *device, hwc2_display_t display, uint32_t *outNumElements,
    hwc2_layer_t *outLayers, int32_t * /*hwc2_composition_t*/ outTypes) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetChangedCompositionType(
      device, display, outNumElements, outLayers, outTypes);
  return ret;
}

int HwcShim::OnGetChangedCompositionType(
    hwc2_device_t *device, hwc2_display_t display, uint32_t *outNumElements,
    hwc2_layer_t *outLayers, int32_t * /*hwc2_composition_t*/ outTypes) {
  int ret = -1;

  HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES pfnOnGetChangedCompositionType =
      reinterpret_cast<HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES>(
          hwc_composer_device->getFunction(
              hwc_composer_device,
              HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES));
  if (pfnOnGetChangedCompositionType) {
    ret = pfnOnGetChangedCompositionType(hwc_composer_device, display, outNumElements,
                                   outLayers, outTypes);
  }
  return ret;
}

int HwcShim::HookGetClientTargetSupport(
    hwc2_device_t *device, hwc2_display_t display, uint32_t width,
    uint32_t height, int32_t /*android_pixel_format_t*/ format,
    int32_t /*android_dataspace_t*/ dataspace) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetClientTargetSupport(
      device, display, width, height, format, dataspace);

  return ret;
}

int HwcShim::OnGetClientTargetSupport(
    hwc2_device_t *device, hwc2_display_t display, uint32_t width,
    uint32_t height, int32_t /*android_pixel_format_t*/ format,
    int32_t /*android_dataspace_t*/ dataspace) {
  int ret = -1;

  HWC2_PFN_GET_CLIENT_TARGET_SUPPORT pfnGetClientTargetSupport =
      reinterpret_cast<HWC2_PFN_GET_CLIENT_TARGET_SUPPORT>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT));
  if (pfnGetClientTargetSupport) {
    ret = pfnGetClientTargetSupport(hwc_composer_device, display, width, height,
                                    format, dataspace);
  }

  return ret;
}

int HwcShim::HookGetColorMode(hwc2_device_t *device, hwc2_display_t display,
                              uint32_t *outNumModes,
                              int32_t * /*android_color_mode_t*/ outModes) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetColorMode(device, display, outNumModes,
                                                outModes);

  return ret;
}

int HwcShim::OnGetColorMode(hwc2_device_t *device, hwc2_display_t display,
                            uint32_t *outNumModes,
                            int32_t * /*android_color_mode_t*/ outModes) {
  int ret = -1;

  HWC2_PFN_GET_COLOR_MODES pfnOnGetColorMode =
      reinterpret_cast<HWC2_PFN_GET_COLOR_MODES>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_COLOR_MODES));
  if (pfnOnGetColorMode) {
    ret =
        pfnOnGetColorMode(hwc_composer_device, display, outNumModes, outModes);
  }

  return ret;
}

int HwcShim::HookGetDisplayAttribute(hwc2_device_t *device,
                                     hwc2_display_t display,
                                     hwc2_config_t config,
                                     int32_t /*hwc2_attribute_t*/ attribute,
                                     int32_t *outValue) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetDisplayAttribute(device, display, config,
                                                       attribute, outValue);
  return ret;
}

int HwcShim::OnGetDisplayAttribute(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_config_t config,
                                   int32_t /*hwc2_attribute_t*/ attribute,
                                   int32_t *outValue) {
  int ret = -1;

  HWC2_PFN_GET_DISPLAY_ATTRIBUTE pfnDisplayAttribute =
      reinterpret_cast<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE));
  if (pfnDisplayAttribute) {
    ret = pfnDisplayAttribute(hwc_composer_device, display, config, attribute,
                              outValue);
  }
  mHwc2->GetDisplayAttributesExit(display, config, attribute, outValue);
  return ret;
}

int HwcShim::HookGetDisplayConfig(hwc2_device_t *device, hwc2_display_t display,
                                  uint32_t *outNumConfigs,
                                  hwc2_config_t *outConfigs) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetDisplayConfig(device, display,
                                                    outNumConfigs, outConfigs);
  return ret;
}

int HwcShim::OnGetDisplayConfig(hwc2_device_t *device, hwc2_display_t display,
                                uint32_t *outNumConfigs,
                                hwc2_config_t *outConfigs) {
  int ret = -1;

  HWC2_PFN_GET_DISPLAY_CONFIGS pfnGetDisplayConfig =
      reinterpret_cast<HWC2_PFN_GET_DISPLAY_CONFIGS>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_DISPLAY_CONFIGS));
  if (pfnGetDisplayConfig) {
    ret = pfnGetDisplayConfig(hwc_composer_device, display, outNumConfigs,
                              outConfigs);
  }
  mHwc2->GetDisplayConfigsExit(display, outConfigs, *outNumConfigs);
  return ret;
}

int HwcShim::HookGetDisplayName(hwc2_device_t *device, hwc2_display_t display,
                                uint32_t *outSize, char *outName) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetDisplayName(device, display, outSize,
                                                  outName);
  return ret;
}

int HwcShim::OnGetDisplayName(hwc2_device_t *device, hwc2_display_t display,
                              uint32_t *outSize, char *outName) {
  int ret = -1;

  HWC2_PFN_GET_DISPLAY_NAME pfnGetDisplayName =
      reinterpret_cast<HWC2_PFN_GET_DISPLAY_NAME>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_DISPLAY_NAME));
  if (pfnGetDisplayName) {
    ret = pfnGetDisplayName(hwc_composer_device, display, outSize, outName);
  }

  return ret;
}

int HwcShim::HookGetDisplayRequest(
    hwc2_device_t *device, hwc2_display_t display,
    int32_t * /*hwc2_display_request_t*/ outDisplayRequests,
    uint32_t *outNumElements, hwc2_layer_t *outLayers,
    int32_t * /*hwc2_layer_request_t*/ outLayerRequests) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetDisplayRequest(
      device, display, outDisplayRequests, outNumElements, outLayers,
      outLayerRequests);
  return ret;
}

int HwcShim::OnGetDisplayRequest(
    hwc2_device_t *device, hwc2_display_t display,
    int32_t * /*hwc2_display_request_t*/ outDisplayRequests,
    uint32_t *outNumElements, hwc2_layer_t *outLayers,
    int32_t * /*hwc2_layer_request_t*/ outLayerRequests) {
  int ret = -1;

  HWC2_PFN_GET_DISPLAY_REQUESTS pfnGetDisplayRequest =
      reinterpret_cast<HWC2_PFN_GET_DISPLAY_REQUESTS>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_DISPLAY_REQUESTS));
  if (pfnGetDisplayRequest) {
    ret = pfnGetDisplayRequest(hwc_composer_device, display, outDisplayRequests,
                               outNumElements, outLayers, outLayerRequests);
  }

  return ret;
}

int HwcShim::HookGetDisplayType(hwc2_device_t *device, hwc2_display_t display,
                                int32_t * /*hwc2_display_type_t*/ outType) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetDisplayType(device, display, outType);
  return ret;
}

int HwcShim::OnGetDisplayType(hwc2_device_t *device, hwc2_display_t display,
                              int32_t * /*hwc2_display_type_t*/ outType) {
  int ret = -1;

  HWC2_PFN_GET_DISPLAY_TYPE pfnGetDisplayType =
      reinterpret_cast<HWC2_PFN_GET_DISPLAY_TYPE>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_DISPLAY_TYPE));
  if (pfnGetDisplayType) {
    ret = pfnGetDisplayType(hwc_composer_device, display, outType);
  }

  return ret;
}

int HwcShim::HookGetDoseSupport(hwc2_device_t *device, hwc2_display_t display,
                                int32_t *outSupport) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetDoseSupport(device, display, outSupport);
  return ret;
}

int HwcShim::OnGetDoseSupport(hwc2_device_t *device, hwc2_display_t display,
                              int32_t *outSupport) {
  int ret = -1;

  HWC2_PFN_GET_DOZE_SUPPORT pfnGetDoseSupport =
      reinterpret_cast<HWC2_PFN_GET_DOZE_SUPPORT>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_PRESENT_DISPLAY));
  if (pfnGetDoseSupport) {
    ret = pfnGetDoseSupport(hwc_composer_device, display, outSupport);
  }

  return ret;
}

int HwcShim::HookGetHDRCapabalities(
    hwc2_device_t *device, hwc2_display_t display, uint32_t *outNumTypes,
    int32_t * /*android_hdr_t*/ outTypes, float *outMaxLuminance,
    float *outMaxAverageLuminance, float *outMinLuminance) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetHDRCapabalities(
      device, display, outNumTypes, outTypes, outMaxLuminance,
      outMaxAverageLuminance, outMinLuminance);
  return ret;
}

int HwcShim::OnGetHDRCapabalities(hwc2_device_t *device, hwc2_display_t display,
                                  uint32_t *outNumTypes,
                                  int32_t * /*android_hdr_t*/ outTypes,
                                  float *outMaxLuminance,
                                  float *outMaxAverageLuminance,
                                  float *outMinLuminance) {
  int ret = -1;

  HWC2_PFN_GET_HDR_CAPABILITIES pfnGetHDRCapabalities =
      reinterpret_cast<HWC2_PFN_GET_HDR_CAPABILITIES>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_HDR_CAPABILITIES));
  if (pfnGetHDRCapabalities) {
    ret = pfnGetHDRCapabalities(hwc_composer_device, display, outNumTypes,
                                outTypes, outMaxLuminance,
                                outMaxAverageLuminance, outMinLuminance);
  }

  return ret;
}

int HwcShim::HookGetReleaseFences(hwc2_device_t *device, hwc2_display_t display,
                                  uint32_t *outNumElements,
                                  hwc2_layer_t *outLayers, int32_t *outFences) {
  int ret = -1;
  ret = GetComposerShim(device)->OnGetReleaseFences(
      device, display, outNumElements, outLayers, outFences);
  return ret;
}

int HwcShim::OnGetReleaseFences(hwc2_device_t *device, hwc2_display_t display,
                                uint32_t *outNumElements,
                                hwc2_layer_t *outLayers, int32_t *outFences) {
  int ret = -1;

  HWC2_PFN_GET_RELEASE_FENCES pfnGetReleaseFences =
      reinterpret_cast<HWC2_PFN_GET_RELEASE_FENCES>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_GET_RELEASE_FENCES));
  if (pfnGetReleaseFences) {
    ret = pfnGetReleaseFences(hwc_composer_device, display, outNumElements,
                              outLayers, outFences);
  }

  return ret;
}

int HwcShim::HookSetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_config_t config) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetActiveConfig(device, display, config);
  return ret;
}

int HwcShim::OnSetActiveConfig(hwc2_device_t *device, hwc2_display_t display,
                               hwc2_config_t config) {
  int ret = -1;

  HWC2_PFN_SET_ACTIVE_CONFIG pfnSetActiveConfig =
      reinterpret_cast<HWC2_PFN_SET_ACTIVE_CONFIG>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_ACTIVE_CONFIG));
  if (pfnSetActiveConfig) {
    ret = pfnSetActiveConfig(hwc_composer_device, display, config);
  }

  return ret;
}

int HwcShim::HookSetClientTarget(hwc2_device_t *device, hwc2_display_t display,
                                 buffer_handle_t target, int32_t acquireFence,
                                 int32_t /*android_dataspace_t*/ dataspace,
                                 hwc_region_t damage) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetClientTarget(
      device, display, target, acquireFence, dataspace, damage);
  return ret;
}

int HwcShim::OnSetClientTarget(hwc2_device_t *device, hwc2_display_t display,
                               buffer_handle_t target, int32_t acquireFence,
                               int32_t /*android_dataspace_t*/ dataspace,
                               hwc_region_t damage) {
  int ret = -1;

  HWC2_PFN_SET_CLIENT_TARGET pfnSetClientTarget =
      reinterpret_cast<HWC2_PFN_SET_CLIENT_TARGET>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_CLIENT_TARGET));
  if (pfnSetClientTarget) {
    ret = pfnSetClientTarget(hwc_composer_device, display, target, acquireFence,
                             dataspace, damage);
  }

  return ret;
}

int HwcShim::HookSetColorMode(hwc2_device_t *device, hwc2_display_t display,
                              int32_t /*android_color_mode_t*/ mode) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetColorMode(device, display, mode);
  return ret;
}

int HwcShim::OnSetColorMode(hwc2_device_t *device, hwc2_display_t display,
                            int32_t /*android_color_mode_t*/ mode) {
  int ret = -1;

  HWC2_PFN_SET_COLOR_MODE pfnSetColorMode =
      reinterpret_cast<HWC2_PFN_SET_COLOR_MODE>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_COLOR_MODE));
  if (pfnSetColorMode) {
    ret = pfnSetColorMode(hwc_composer_device, display, mode);
  }

  return ret;
}

int HwcShim::HookSetColorTransform(hwc2_device_t *device,
                                   hwc2_display_t display, const float *matrix,
                                   int32_t /*android_color_transform_t*/ hint) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetColorTransform(device, display, matrix,
                                                     hint);
  return ret;
}

int HwcShim::OnSetColorTransform(hwc2_device_t *device, hwc2_display_t display,
                                 const float *matrix,
                                 int32_t /*android_color_transform_t*/ hint) {
  int ret = -1;

  HWC2_PFN_SET_COLOR_TRANSFORM pfnSetColorTransform =
      reinterpret_cast<HWC2_PFN_SET_COLOR_TRANSFORM>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_COLOR_TRANSFORM));
  if (pfnSetColorTransform) {
    ret = pfnSetColorTransform(hwc_composer_device, display, matrix, hint);
  }

  return ret;
}

int HwcShim::HookSetOutputBuffer(hwc2_device_t *device, hwc2_display_t display,
                                 buffer_handle_t buffer, int32_t releaseFence) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetOutputBuffer(device, display, buffer,
                                                   releaseFence);
  return ret;
}

int HwcShim::OnSetOutputBuffer(hwc2_device_t *device, hwc2_display_t display,
                               buffer_handle_t buffer, int32_t releaseFence) {
  int ret = -1;

  HWC2_PFN_SET_OUTPUT_BUFFER pfnSetOutputBuffer =
      reinterpret_cast<HWC2_PFN_SET_OUTPUT_BUFFER>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_OUTPUT_BUFFER));
  if (pfnSetOutputBuffer) {
    ret =
        pfnSetOutputBuffer(hwc_composer_device, display, buffer, releaseFence);
  }

  return ret;
}

int HwcShim::HookSetPowerMode(hwc2_device_t *device, hwc2_display_t display,
                              int32_t /*hwc2_power_mode_t*/ mode) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetPowerMode(device, display, mode);
  return ret;
}

int HwcShim::OnSetPowerMode(hwc2_device_t *device, hwc2_display_t display,
                            int32_t /*hwc2_power_mode_t*/ mode) {
  int ret = -1;

  HWC2_PFN_SET_POWER_MODE pfnSetPowerMode =
      reinterpret_cast<HWC2_PFN_SET_POWER_MODE>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_POWER_MODE));
  if (pfnSetPowerMode) {
    ret = pfnSetPowerMode(hwc_composer_device, display, mode);
  }

  return ret;
}

int HwcShim::HookSetVsyncEnabled(hwc2_device_t *device, hwc2_display_t display,
                                 int32_t /*hwc2_vsync_t*/ enabled) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetPowerMode(device, display, enabled);
  return ret;
}

int HwcShim::OnSetVsyncEnabled(hwc2_device_t *device, hwc2_display_t display,
                               int32_t /*hwc2_vsync_t*/ enabled) {
  int ret = -1;

  HWC2_PFN_SET_VSYNC_ENABLED pfnSetVsyncEnabled =
      reinterpret_cast<HWC2_PFN_SET_VSYNC_ENABLED>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_VSYNC_ENABLED));
  if (pfnSetVsyncEnabled) {
    ret = pfnSetVsyncEnabled(hwc_composer_device, display, enabled);
  }

  return ret;
}

int HwcShim::HookValidateDisplay(hwc2_device_t *device, hwc2_display_t display,
                                 uint32_t *outNumTypes,
                                 uint32_t *outNumRequests) {
  int ret = -1;
  ret = GetComposerShim(device)->OnValidateDisplay(device, display, outNumTypes,
                                                   outNumRequests);
  return ret;
}

int HwcShim::OnValidateDisplay(hwc2_device_t *device, hwc2_display_t display,
                               uint32_t *outNumTypes,
                               uint32_t *outNumRequests) {
  int ret = -1;
  StartCallTime();
  mHwc2->CheckValidateDisplayEntry(display);
  HWC2_PFN_VALIDATE_DISPLAY pfnValidateDisplay =
      reinterpret_cast<HWC2_PFN_VALIDATE_DISPLAY>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_VALIDATE_DISPLAY));
  if (pfnValidateDisplay) {
    ret = pfnValidateDisplay(hwc_composer_device, display, outNumTypes,
                             outNumRequests);
  }
  mHwc2->CheckValidateDisplayExit();
  EndCallTime("Validate()");
  return ret;
}

int HwcShim::HookSetCursorPosition(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_layer_t layer,
                                   int32_t x, int32_t y) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetCursorPosition(device, display, layer, x,
                                                     y);
  return ret;
}

int HwcShim::OnSetCursorPosition(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_layer_t layer, int32_t x, int32_t y) {
  int ret = -1;

  HWC2_PFN_SET_CURSOR_POSITION pfnSetCursorPosition =
      reinterpret_cast<HWC2_PFN_SET_CURSOR_POSITION>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_CURSOR_POSITION));
  if (pfnSetCursorPosition) {
    ret = pfnSetCursorPosition(hwc_composer_device, display, layer, x, y);
  }

  return ret;
}

int HwcShim::HookSetLayerBuffer(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer, buffer_handle_t buffer,
                                int32_t acquireFence) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerBuffer(device, display, layer,
                                                  buffer, acquireFence);
  return ret;
}

int HwcShim::OnSetLayerBuffer(hwc2_device_t *device, hwc2_display_t display,
                              hwc2_layer_t layer, buffer_handle_t buffer,
                              int32_t acquireFence) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_BUFFER pfnSetLayerBuffer =
      reinterpret_cast<HWC2_PFN_SET_LAYER_BUFFER>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_LAYER_BUFFER));
  if (pfnSetLayerBuffer) {
    ret = pfnSetLayerBuffer(hwc_composer_device, display, layer, buffer,
                            acquireFence);
  }

  return ret;
}

int HwcShim::HookSetSurfaceDamage(hwc2_device_t *device, hwc2_display_t display,
                                  hwc2_layer_t layer, hwc_region_t damage) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetSurfaceDamage(device, display, layer,
                                                    damage);
  return ret;
}

int HwcShim::OnSetSurfaceDamage(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer, hwc_region_t damage) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_SURFACE_DAMAGE pfnSetSurfaceDamage =
      reinterpret_cast<HWC2_PFN_SET_LAYER_SURFACE_DAMAGE>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE));
  if (pfnSetSurfaceDamage) {
    ret = pfnSetSurfaceDamage(hwc_composer_device, display, layer, damage);
  }

  return ret;
}

int HwcShim::HookSetLayerBlendMode(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_layer_t layer,
                                   int32_t /*hwc2_blend_mode_t*/ mode) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerBlendMode(device, display, layer,
                                                     mode);
  return ret;
}

int HwcShim::OnSetLayerBlendMode(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_layer_t layer,
                                 int32_t /*hwc2_blend_mode_t*/ mode) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_BLEND_MODE pfnSetLayerBlendMode =
      reinterpret_cast<HWC2_PFN_SET_LAYER_BLEND_MODE>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_LAYER_BLEND_MODE));
  if (pfnSetLayerBlendMode) {
    ret = pfnSetLayerBlendMode(hwc_composer_device, display, layer, mode);
  }

  return ret;
}

int HwcShim::HookSetLayerColor(hwc2_device_t *device, hwc2_display_t display,
                               hwc2_layer_t layer, hwc_color_t color) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerColor(device, display, layer, color);
  return ret;
}

int HwcShim::OnSetLayerColor(hwc2_device_t *device, hwc2_display_t display,
                             hwc2_layer_t layer, hwc_color_t color) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_COLOR pfnSetLayerColor =
      reinterpret_cast<HWC2_PFN_SET_LAYER_COLOR>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_LAYER_COLOR));
  if (pfnSetLayerColor) {
    ret = pfnSetLayerColor(hwc_composer_device, display, layer, color);
  }

  return ret;
}

int HwcShim::HookSetLayerCompositionType(hwc2_device_t *device,
                                         hwc2_display_t display,
                                         hwc2_layer_t layer,
                                         int32_t /*hwc2_composition_t*/ type) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerCompositionType(device, display,
                                                           layer, type);
  return ret;
}

int HwcShim::OnSetLayerCompositionType(hwc2_device_t *device,
                                       hwc2_display_t display,
                                       hwc2_layer_t layer,
                                       int32_t /*hwc2_composition_t*/ type) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_COMPOSITION_TYPE pfnSetLayerCompositionType =
      reinterpret_cast<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE));
  if (pfnSetLayerCompositionType) {
    ret = pfnSetLayerCompositionType(hwc_composer_device, display, layer, type);
  }

  return ret;
}

int HwcShim::HookSetLayerDataSpace(hwc2_device_t *device,
                                   hwc2_display_t display, hwc2_layer_t layer,
                                   int32_t /*android_dataspace_t*/ dataspace) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerDataSpace(device, display, layer,
                                                     dataspace);
  return ret;
}

int HwcShim::OnSetLayerDataSpace(hwc2_device_t *device, hwc2_display_t display,
                                 hwc2_layer_t layer,
                                 int32_t /*android_dataspace_t*/ dataspace) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_DATASPACE pfnSetLayerDataSpace =
      reinterpret_cast<HWC2_PFN_SET_LAYER_DATASPACE>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_LAYER_DATASPACE));
  if (pfnSetLayerDataSpace) {
    ret = pfnSetLayerDataSpace(hwc_composer_device, display, layer, dataspace);
  }

  return ret;
}

int HwcShim::HookSetLayerDisplayFrame(hwc2_device_t *device,
                                      hwc2_display_t display,
                                      hwc2_layer_t layer, hwc_rect_t frame) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerDisplayFrame(device, display, layer,
                                                        frame);
  return ret;
}

int HwcShim::OnSetLayerDisplayFrame(hwc2_device_t *device,
                                    hwc2_display_t display, hwc2_layer_t layer,
                                    hwc_rect_t frame) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_DISPLAY_FRAME pfnSetLayerDisplayFrame =
      reinterpret_cast<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME));
  if (pfnSetLayerDisplayFrame) {
    ret = pfnSetLayerDisplayFrame(hwc_composer_device, display, layer, frame);
  }

  return ret;
}

int HwcShim::HookSetLayerPlaneAlpha(hwc2_device_t *device,
                                    hwc2_display_t display, hwc2_layer_t layer,
                                    float alpha) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerPlaneAlpha(device, display, layer,
                                                      alpha);
  return ret;
}

int HwcShim::OnSetLayerPlaneAlpha(hwc2_device_t *device, hwc2_display_t display,
                                  hwc2_layer_t layer, float alpha) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_PLANE_ALPHA pfnSetLayerPlaneAlpha =
      reinterpret_cast<HWC2_PFN_SET_LAYER_PLANE_ALPHA>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA));
  if (pfnSetLayerPlaneAlpha) {
    ret = pfnSetLayerPlaneAlpha(hwc_composer_device, display, layer, alpha);
  }

  return ret;
}

int HwcShim::HookSetLayerSideBandStream(hwc2_device_t *device,
                                        hwc2_display_t display,
                                        hwc2_layer_t layer,
                                        const native_handle_t *stream) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerSideBandStream(device, display,
                                                          layer, stream);
  return ret;
}

int HwcShim::OnSetLayerSideBandStream(hwc2_device_t *device,
                                      hwc2_display_t display,
                                      hwc2_layer_t layer,
                                      const native_handle_t *stream) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_SIDEBAND_STREAM pfnSetLayerSideBandStream =
      reinterpret_cast<HWC2_PFN_SET_LAYER_SIDEBAND_STREAM>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM));
  if (pfnSetLayerSideBandStream) {
    ret =
        pfnSetLayerSideBandStream(hwc_composer_device, display, layer, stream);
  }

  return ret;
}

int HwcShim::HookSetLayerSourceCrop(hwc2_device_t *device,
                                    hwc2_display_t display, hwc2_layer_t layer,
                                    hwc_frect_t crop) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerSourceCrop(device, display, layer,
                                                      crop);
  return ret;
}

int HwcShim::OnSetLayerSourceCrop(hwc2_device_t *device, hwc2_display_t display,
                                  hwc2_layer_t layer, hwc_frect_t crop) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_SOURCE_CROP pfnSetLayerSourceCrop =
      reinterpret_cast<HWC2_PFN_SET_LAYER_SOURCE_CROP>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_SOURCE_CROP));
  if (pfnSetLayerSourceCrop) {
    ret = pfnSetLayerSourceCrop(hwc_composer_device, display, layer, crop);
  }

  return ret;
}

int HwcShim::HookSetLayerSourceTransform(
    hwc2_device_t *device, hwc2_display_t display, hwc2_layer_t layer,
    int32_t /*hwc_transform_t*/ transform) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerSourceTransform(device, display,
                                                           layer, transform);
  return ret;
}

int HwcShim::OnSetLayerSourceTransform(hwc2_device_t *device,
                                       hwc2_display_t display,
                                       hwc2_layer_t layer,
                                       int32_t /*hwc_transform_t*/ transform) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_TRANSFORM pfnSetLayerSourceTransform =
      reinterpret_cast<HWC2_PFN_SET_LAYER_TRANSFORM>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_LAYER_TRANSFORM));
  if (pfnSetLayerSourceTransform) {
    ret = pfnSetLayerSourceTransform(hwc_composer_device, display, layer,
                                     transform);
  }

  return ret;
}

int HwcShim::HookSetLayerVisibleRegion(hwc2_device_t *device,
                                       hwc2_display_t display,
                                       hwc2_layer_t layer,
                                       hwc_region_t visible) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerVisibleRegion(device, display, layer,
                                                         visible);
  return ret;
}

int HwcShim::OnSetLayerVisibleRegion(hwc2_device_t *device,
                                     hwc2_display_t display, hwc2_layer_t layer,
                                     hwc_region_t visible) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_VISIBLE_REGION pfnSetLayerVisibleRegion =
      reinterpret_cast<HWC2_PFN_SET_LAYER_VISIBLE_REGION>(
          hwc_composer_device->getFunction(
              hwc_composer_device, HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION));
  if (pfnSetLayerVisibleRegion) {
    ret =
        pfnSetLayerVisibleRegion(hwc_composer_device, display, layer, visible);
  }

  return ret;
}

int HwcShim::HookSetLayerZOrder(hwc2_device_t *device, hwc2_display_t display,
                                hwc2_layer_t layer, uint32_t z) {
  int ret = -1;
  ret = GetComposerShim(device)->OnSetLayerZOrder(device, display, layer, z);
  return ret;
}

int HwcShim::OnSetLayerZOrder(hwc2_device_t *device, hwc2_display_t display,
                              hwc2_layer_t layer, uint32_t z) {
  int ret = -1;

  HWC2_PFN_SET_LAYER_Z_ORDER pfnSetLayerZOrder =
      reinterpret_cast<HWC2_PFN_SET_LAYER_Z_ORDER>(
          hwc_composer_device->getFunction(hwc_composer_device,
                                           HWC2_FUNCTION_SET_LAYER_Z_ORDER));
  if (pfnSetLayerZOrder) {
    ret = pfnSetLayerZOrder(hwc_composer_device, display, layer, z);
  }

  return ret;
}

hwc2_function_pointer_t HwcShim::HookDevGetFunction(struct hwc2_device *dev,
                                                    int32_t descriptor) {
  switch (descriptor) {
    case HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookCreateVirtualDisplay);
    case HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookDestroyVirtualDisplay);
    case HWC2_FUNCTION_DUMP:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookDump);
    case HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookGetMaxVirtualDisplayCount);
    case HWC2_FUNCTION_REGISTER_CALLBACK:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookRegisterCallback);
    case HWC2_FUNCTION_CREATE_LAYER:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookCreateLayer);
    case HWC2_FUNCTION_DESTROY_LAYER:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookDestroyLayer);
    case HWC2_FUNCTION_GET_ACTIVE_CONFIG:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetActiveConfig);
    case HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookGetChangedCompositionType);
    case HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookGetClientTargetSupport);
    case HWC2_FUNCTION_GET_COLOR_MODES:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetColorMode);
    case HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookGetDisplayAttribute);
    case HWC2_FUNCTION_GET_DISPLAY_CONFIGS:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetDisplayConfig);
    case HWC2_FUNCTION_GET_DISPLAY_NAME:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetDisplayName);
    case HWC2_FUNCTION_GET_DISPLAY_REQUESTS:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetDisplayRequest);
    case HWC2_FUNCTION_GET_DISPLAY_TYPE:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetDisplayType);
    case HWC2_FUNCTION_GET_DOZE_SUPPORT:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetDoseSupport);
    case HWC2_FUNCTION_GET_HDR_CAPABILITIES:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetHDRCapabalities);
    case HWC2_FUNCTION_GET_RELEASE_FENCES:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookGetReleaseFences);
    case HWC2_FUNCTION_PRESENT_DISPLAY:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookPresentDisplay);
    case HWC2_FUNCTION_SET_ACTIVE_CONFIG:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetActiveConfig);
    case HWC2_FUNCTION_SET_CLIENT_TARGET:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetClientTarget);
    case HWC2_FUNCTION_SET_COLOR_MODE:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetColorMode);
    case HWC2_FUNCTION_SET_COLOR_TRANSFORM:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetColorTransform);
    case HWC2_FUNCTION_SET_OUTPUT_BUFFER:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetOutputBuffer);
    case HWC2_FUNCTION_SET_POWER_MODE:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetPowerMode);
    case HWC2_FUNCTION_SET_VSYNC_ENABLED:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetVsyncEnabled);
    case HWC2_FUNCTION_VALIDATE_DISPLAY:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookValidateDisplay);
    case HWC2_FUNCTION_SET_CURSOR_POSITION:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetCursorPosition);
    case HWC2_FUNCTION_SET_LAYER_BUFFER:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerBuffer);
    case HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetSurfaceDamage);
    case HWC2_FUNCTION_SET_LAYER_BLEND_MODE:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerBlendMode);
    case HWC2_FUNCTION_SET_LAYER_COLOR:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerColor);
    case HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookSetLayerCompositionType);
    case HWC2_FUNCTION_SET_LAYER_DATASPACE:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerDataSpace);
    case HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookSetLayerDisplayFrame);
    case HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerPlaneAlpha);
    case HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookSetLayerSideBandStream);
    case HWC2_FUNCTION_SET_LAYER_SOURCE_CROP:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerSourceCrop);
    case HWC2_FUNCTION_SET_LAYER_TRANSFORM:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookSetLayerSourceTransform);
    case HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION:
      return reinterpret_cast<hwc2_function_pointer_t>(
          &HookSetLayerVisibleRegion);
    case HWC2_FUNCTION_SET_LAYER_Z_ORDER:
      return reinterpret_cast<hwc2_function_pointer_t>(&HookSetLayerZOrder);
    default:
      return NULL;
  }
  return NULL;
}

int HwcShim::GetFunctionPointer(void *LibHandle, const char *Symbol,
                                void **FunctionHandle, uint32_t debug) {
  HWCVAL_UNUSED(debug);

  int rc = 0;

  const char *error = NULL;
  dlerror();

  uint32_t *tmp = (uint32_t *)dlsym(LibHandle, Symbol);
  *FunctionHandle = (void *)tmp;
  error = dlerror();

  if ((tmp == 0) && (error != NULL)) {
    rc = -1;
    HWCLOGI("GetFunctionPointer %s %s", error, Symbol);
    *FunctionHandle = NULL;
  }

  return rc;
}

int HwcShim::HookOpen(const hw_module_t *module, const char *name,
                      hw_device_t **device) {
  // Real HWC hook_open is called in init called in the constructor so a real
  // HEC instance should already exist at this point

  ATRACE_CALL();
  HWCLOGV("HwcShim::HookOpen");

  if (!module || !name || !device) {
    HWCERROR(eCheckHwcParams,
             "HwcShim::HookOpen - Invalid arguments passed to HookOpen");
  }

  if (strcmp(name, HWC_HARDWARE_COMPOSER) == 0) {
    static HwcShim *hwcShim = new HwcShim(module);
    HWCLOGI("HwcShim::HookOpen - Created HwcShim @ %p", hwcShim);

    if (!hwcShim) {
      HWCERROR(eCheckInternalError,
               "HwcShim::HookOpen - Failed to create HWComposer object");
      return -ENOMEM;
    }

    *device = &hwcShim->common;
    // ALOG_ASSERT((void*)*device == (void*)hwcShim);
    HWCLOGI("HwcShim::HookOpen - Intel HWComposer was loaded successfully.");

    return 0;
  }

  return -EINVAL;
}

int HwcShim::HookClose(struct hw_device_t *device) {
  ATRACE_CALL();
  HWCLOGV("HwcShim::HookClose");

  delete static_cast<HwcShim *>(static_cast<void *>(device));
  return device ? 0 : -ENOENT;
}

void HwcShim::StartCallTime(void) {
  if (state->IsCheckEnabled(eCheckOnSetLatency)) {
    callTimeStart = android::elapsedRealtimeNano();
  }
  //    HWCLOGI("LOgged start time %d", (int) (callTimeStart/ 1000));
}

void HwcShim::EndCallTime(const char *function) {
  uint64_t callTimeDuration = 0;
  if (state->IsCheckEnabled(eCheckOnSetLatency)) {
    callTimeDuration = (android::elapsedRealtimeNano() - callTimeStart);

    HWCCHECK(eCheckOnSetLatency);
    if (callTimeDuration > callTimeThreshold) {
      HWCERROR(eCheckOnSetLatency, "Call Time Error %s time was %fms", function,
               ((double)callTimeDuration) / 1000000.0);
    }
  }

  //    HWCLOGI("Logged end time %d", (int) (callTimeDuration / 1000));
}

int HwcShim::OnEventControl(int disp, int event, int enabled) {
  int status;

  ALOG_ASSERT(disp < HWC_NUM_DISPLAY_TYPES,
              "HwcShim::OnEventControl - disp[%d] exceeds maximum[%d]", disp,
              HWC_NUM_DISPLAY_TYPES);
  if (event == HWC_EVENT_VSYNC) {
    status = EnableVSync(disp, enabled);
  } else {
    // status = hwc_composer_device->eventControl(hwc_composer_device, disp,
    // event, enabled);
  }

  HWCLOGV("HwcShim::OnEventControl returning status=%d", status);
  return status;
}

int HwcShim::EnableVSync(int disp, bool enable) {
  HWCLOGI("HwcShim::EnableVSync - HWC_EVENT_VSYNC: disp[%d] %s VSYNC event",
          disp, enable ? "enabling" : "disabling");
  return -1;  // hwc_composer_device->eventControl(hwc_composer_device, disp,
              // HWC_EVENT_VSYNC, enable);
}

void HwcShim::OnDump(char *buff, int buff_len) {
  // hwc_composer_device->dump(hwc_composer_device, buff, buff_len);
}

int HwcShim::OnGetDisplayConfigs(int disp, uint32_t *configs,
                                 size_t *numConfigs) {
  int ret =
      true;

  HWCLOGD("HwcShim::OnGetDisplayConfigs enter disp %d", disp);
  if (disp != 0)
    return false;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_GET_DISPLAY_CONFIGS temp =
      reinterpret_cast<HWC2_PFN_GET_DISPLAY_CONFIGS>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_GET_DISPLAY_CONFIGS));
  hwc2_config_t *numConfig2s;
  temp(hwc2_dvc, disp, configs, numConfig2s);
  numConfigs = numConfigs;

  HWCLOGD("HwcShim::OnGetDisplayConfigs D%d %d configs returned", disp,
          *numConfigs);
  mHwc2->GetDisplayConfigsExit(disp, configs, *numConfigs);
  return ret;
}

int HwcShim::OnGetDisplayAttributes(int disp, uint32_t config,
                                    const int32_t attribute, int32_t *values) {
  HWCLOGV_COND(eLogHwcDisplayConfigs,
               "HwcShim::OnGetDisplayAttributes D%d config %d", disp, config);
  int ret;
  {
    Hwcval::PushThreadState ts("getDisplayAttributes");
    if (disp != 0)
      return false;
    hwc2_device_t *hwc2_dvc =
        reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
    HWC2_PFN_GET_DISPLAY_ATTRIBUTE temp =
        reinterpret_cast<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(hwc2_dvc->getFunction(
            hwc2_dvc, HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE));
    temp(hwc2_dvc, disp, config, attribute, values);
  }
  mHwc2->GetDisplayAttributesExit(disp, config, attribute, values);
  return ret;
}

// FROM iahwc2.cpp
/*
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */

static struct hw_module_methods_t methods = {.open = HwcShim::HookOpen};

#pragma GCC visibility push(default)
hwc_module_t HAL_MODULE_INFO_SYM = {
    .common = {.tag = HARDWARE_MODULE_TAG,
               .module_api_version = HARDWARE_MODULE_API_VERSION(2, 0),
               .hal_api_version = HARDWARE_HAL_API_VERSION,
               .id = HWC_HARDWARE_MODULE_ID,
               .name = "IA-Hardware-Composer",
               .author = "The Android Open Source Project",
               .methods = &methods,
               .dso = NULL,
               .reserved = {0}}};

// END FROM iahwc2.cpp
