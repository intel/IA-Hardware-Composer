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

#define LOG_TAG "HWCINTFC"

#include "HwchInterface.h"
#include "HwchSystem.h"
#include "HwcTestLog.h"
#include <hardware/hardware.h>
#include "HwcTestDefs.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

static uint32_t hwcApiVersion(const hwc2_device_t *hwc) {
  uint32_t hwcVersion = hwc->common.version;
  return hwcVersion & HARDWARE_API_VERSION_2_MAJ_MIN_MASK;
}

static uint32_t hwcHeaderVersion(const hwc2_device_t *hwc) {
  uint32_t hwcVersion = hwc->common.version;
  return hwcVersion & HARDWARE_API_VERSION_2_HEADER_MASK;
}

static bool hwcHasApiVersion(const hwc2_device_t *hwc, uint32_t version) {
  return hwcApiVersion(hwc) >= (version & HARDWARE_API_VERSION_2_MAJ_MIN_MASK);
}

Hwch::Interface::Interface()
    : hwc_composer_device(0),
      mDisplayNeedsUpdate(0),  // 0 is always connected
      mNumDisplays(0),
      mRepaintNeeded(false) {
  memset(mBlanked, 0, sizeof(mBlanked));
}

int Hwch::Interface::Initialise(void) {
  int r = 0;

  LoadHwcModule();

  HWCLOGI("hwc_composer_device = %p", (void *)hwc_composer_device);

  HWCLOGI("Using %s version %u.%u", HWC_HARDWARE_COMPOSER,
          (api_version() >> 24) & 0xff, (api_version() >> 16) & 0xff);

  return r;
}

// Load and prepare the hardware composer module.  Sets hwc_composer_device
void Hwch::Interface::LoadHwcModule() {
  hw_module_t const *module;

  if (hw_get_module(HWC_HARDWARE_MODULE_ID, &module) != 0) {
    ALOGE("%s module not found", HWC_HARDWARE_MODULE_ID);
    ALOG_ASSERT(0);
    return;
  }

  int err = hwc2_open(module, &hwc_composer_device);
  if (err) {
    ALOGE("%s device failed to initialize (%s)", HWC_HARDWARE_COMPOSER,
          strerror(-err));
    ALOG_ASSERT(0);
    return;
  }

  if (!hwcHasApiVersion(hwc_composer_device, HWC_DEVICE_API_VERSION_1_0) ||
#if MIN_HEADER_VERSION > 0
      hwcHeaderVersion(hwc_composer_device) < MIN_HWC_HEADER_VERSION ||
#endif
      hwcHeaderVersion(hwc_composer_device) > HWC_HEADER_VERSION) {
    ALOGE("%s device version %#x unsupported, will not be used",
          HWC_HARDWARE_COMPOSER, hwc_composer_device->common.version);
    hwc2_close(hwc_composer_device);
    hwc_composer_device = NULL;
    return;
  }
}

int Hwch::Interface::RegisterProcs(void) {
  HWCLOGD_COND(eLogHwchInterface, "RegisterProcs: hwc_composer_device = %p",
               (void *)hwc_composer_device);

  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_REGISTER_CALLBACK pfnRegisterCallBack =
      reinterpret_cast<HWC2_PFN_REGISTER_CALLBACK>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_REGISTER_CALLBACK));

  if (pfnRegisterCallBack) {
    hwc2_callback_data_t callbackData = NULL;

    pfnRegisterCallBack(hwc2_dvc, HWC2_CALLBACK_HOTPLUG, &callbackData,
                        (hwc2_function_pointer_t)&hook_hotplug);
    pfnRegisterCallBack(hwc2_dvc, HWC2_CALLBACK_VSYNC, &callbackData,
                        (hwc2_function_pointer_t)&hook_vsync);
    pfnRegisterCallBack(hwc2_dvc, HWC2_CALLBACK_REFRESH, &callbackData,
                        (hwc2_function_pointer_t)&hook_invalidate);
  }
  return 0;
}

int Hwch::Interface::GetDisplayAttributes() {
  for (int disp = 0; disp < 2; ++disp) {
    GetDisplayAttributes(disp);
  }
  return NO_ERROR;
}

int Hwch::Interface::GetDisplayAttributes(uint32_t disp) {
  hwc2_device_t *hwc2_dvc;
  hwc2_config_t configs[100];
  uint32_t numConfigs = 0;  // sizeof(configs) / sizeof(uint32_t);
  Hwch::System &system = Hwch::System::getInstance();

  // Add the virtual display (if enabled on the command line)
  if (((system.GetDisplay(disp).IsVirtualDisplay()) &&
       (system.IsVirtualDisplayEmulationEnabled()))) {
    Hwch::Display::Attributes &att = system.GetDisplay(disp).mAttributes;
    att.width = system.GetVirtualDisplayWidth();
    att.height = system.GetVirtualDisplayHeight();
    att.vsyncPeriod = 0;

    HWCLOGI("GetDisplayAttributes: created virtual display on D%d", disp);
  }

  if (!(disp == HWCVAL_DISPLAY_ID_VIRTUAL)) {
    Hwch::Display &display = system.GetDisplay(disp);
    uint32_t nc = 0;  // sizeof(configs) / sizeof(uint32_t);
    int ret = -1;

    /* hwc2_device_t */ hwc2_dvc =
        reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
    HWC2_PFN_GET_DISPLAY_CONFIGS pfngetDisplayConfigs =
        reinterpret_cast<HWC2_PFN_GET_DISPLAY_CONFIGS>(
            hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_GET_DISPLAY_CONFIGS));

    if (pfngetDisplayConfigs) {
      ret = pfngetDisplayConfigs(hwc2_dvc, disp, &nc, NULL);
      if (ret == NO_ERROR && nc > 0) {
        ret = pfngetDisplayConfigs(hwc2_dvc, disp, &nc, configs);
      }else {
        return ret;
      }
    } else
      return ret;


    numConfigs = nc;

    if (ret != NO_ERROR) {
      Hwch::Display::Attributes &att = display.mAttributes;
      att.vsyncPeriod = 0;
      att.width = 0;
      att.height = 0;

      if (mNumDisplays > HWCH_MIN_DISPLAYLIST_SIZE) {
        if (disp == (mNumDisplays - 1)) {
          --mNumDisplays;
        }
      }
      return NO_ERROR;
    }

    if (numConfigs == 0) {
      display.SetConnected(false);
      return NO_ERROR;
    }

#ifdef HWCVAL_USE_GETACTIVECONFIG
    HWCLOGD("GetDisplayAttributes: getDisplayConfigs D%d returned %d configs",
            disp, nc);

    int activeConfig =
        hwc_composer_device->getActiveConfig(hwc_composer_device, disp);

    HWCCHECK(eCheckHwcInterface);
    if (activeConfig < 0) {
      HWCERROR(eCheckHwcInterface,
               "getActiveConfig for display %d returned invalid config %x",
               disp, activeConfig);
      return -1;
    }
#else
    // Config indexed 0 actually contains the current config.
    // This is important because:
    // 1. In current HWC as of 04/05/2016, I find that getActiveConfig does not
    // actually return the right number
    // 2. Apparently surfaceflinger does not call getActiveConfig anyway but
    // assumes that the correct config is in config 0.
    int activeConfig = 0;
#endif

    // Selected config is first in the list
    const uint32_t attributes[] = {// HWC2_ATTRIBUTE_INVALID,
                                   HWC2_ATTRIBUTE_VSYNC_PERIOD,
                                   HWC2_ATTRIBUTE_WIDTH, HWC2_ATTRIBUTE_HEIGHT,
                                   // HWC2_ATTRIBUTE_DPI_X,
                                   // HWC2_ATTRIBUTE_DPI_Y
    };
    HWC2_PFN_GET_DISPLAY_ATTRIBUTE pfngetDisplayAttribute =
        reinterpret_cast<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(hwc2_dvc->getFunction(
            hwc2_dvc, HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE));

    if (HwcTestState::getInstance()->IsOptionEnabled(eLogHwcDisplayConfigs)) {
      HWCLOGD("Logging HWC display configs for D%d", disp);
      int32_t v[sizeof(attributes)];
      int32_t ret = -1;
      for (uint32_t i = 0; i < numConfigs; ++i) {
        for (uint32_t j = 0; j < sizeof(attributes); ++j) {
          if (pfngetDisplayAttribute) {
            ret = pfngetDisplayAttribute(hwc2_dvc, disp, configs[i],
                                         attributes[j], v + j);
          }
        }

        if (ret < 0) {
          HWCLOGE("Config %d/%d %x ERROR %d", i, numConfigs, configs[i], ret);
        } else {
          HWCLOGD("Config %d/%d %x %dx%d@%d", i, numConfigs, configs[i], v[1],
                  v[2], v[0]);
        }
      }
    }

    HWCLOGD(
        "Hwch::Interface::GetDisplayAttributes Getting attributes for display "
        "%d config ix %d/%d %x",
        disp, activeConfig, numConfigs, configs[activeConfig]);
    int32_t *values = (int32_t *)(&(display.mAttributes));
    for (uint32_t j = 0; j < 3; ++j) {
      if (pfngetDisplayAttribute) {
        ret = pfngetDisplayAttribute(hwc2_dvc, disp, configs[activeConfig],
                                     attributes[j], values + j);
      }
    }

    if ((display.GetWidth() <= 1) && (display.GetHeight() <= 1)) {
      display.SetConnected(false);
    } else {
      display.SetConnected(true);
    }

    HWCLOGI("GetDisplayAttributes: Display %d width %d height %d", disp,
            display.GetWidth(), display.GetHeight());
  }

  if (disp >= mNumDisplays) {
    mNumDisplays = max(disp + 1, HWCH_MIN_DISPLAYLIST_SIZE);
  }
  return NO_ERROR;
}

int Hwch::Interface::CreateLayer(hwc2_display_t disp, hwc2_layer_t *outLayer) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_CREATE_LAYER pfnCreateLayer =
      reinterpret_cast<HWC2_PFN_CREATE_LAYER>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_CREATE_LAYER));

  if (pfnCreateLayer) {
    ret = pfnCreateLayer(hwc2_dvc, disp, outLayer);
  }
  return ret;
}

int Hwch::Interface::setLayerCompositionType(hwc2_display_t disp,
                                             hwc2_layer_t layer, int32_t type) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_COMPOSITION_TYPE pfnsetLayerCompositionType =
      reinterpret_cast<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(
          hwc2_dvc->getFunction(hwc2_dvc,
                                HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE));

  if (pfnsetLayerCompositionType) {
    ret = pfnsetLayerCompositionType(hwc2_dvc, disp, layer, type);
  }
  return ret;
}

int Hwch::Interface::setLayerBuffer(hwc2_display_t disp, hwc2_layer_t layer,
                                    buffer_handle_t buffer,
                                    int32_t acquireFence) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_BUFFER pfnsetLayerBuffer =
      reinterpret_cast<HWC2_PFN_SET_LAYER_BUFFER>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_SET_LAYER_BUFFER));

  if (pfnsetLayerBuffer) {
    ret = pfnsetLayerBuffer(hwc2_dvc, disp, layer, buffer, acquireFence);
  }
  return ret;
}

int Hwch::Interface::setLayerBlendMode(hwc2_display_t disp, hwc2_layer_t layer,
                                       int32_t mode) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_BLEND_MODE pfnsetLayerBlendMode =
      reinterpret_cast<HWC2_PFN_SET_LAYER_BLEND_MODE>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_SET_LAYER_BLEND_MODE));

  if (pfnsetLayerBlendMode) {
    ret = pfnsetLayerBlendMode(hwc2_dvc, disp, layer, mode);
  }
  return ret;
}

int Hwch::Interface::setLayerTransform(hwc2_display_t disp, hwc2_layer_t layer,
                                       int32_t transform) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_TRANSFORM pfnsetLayerTransform =
      reinterpret_cast<HWC2_PFN_SET_LAYER_TRANSFORM>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_SET_LAYER_TRANSFORM));

  if (pfnsetLayerTransform) {
    ret = pfnsetLayerTransform(hwc2_dvc, disp, layer, transform);
  }
  return ret;
}

int Hwch::Interface::setLayerSourceCrop(hwc2_display_t disp, hwc2_layer_t layer,
                                        hwcomposer::HwcRect<float> crop) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_SOURCE_CROP pfnsetLayerSourceCrop =
      reinterpret_cast<HWC2_PFN_SET_LAYER_SOURCE_CROP>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_SET_LAYER_SOURCE_CROP));

  if (pfnsetLayerSourceCrop) {
    hwc_frect_t rect;
    rect.left = crop.left;
    rect.top = crop.top;
    rect.right = crop.right;
    rect.bottom = crop.bottom;
    ret = pfnsetLayerSourceCrop(hwc2_dvc, disp, layer, rect);
  }
  return ret;
}

int Hwch::Interface::setLayerDisplayFrame(hwc2_display_t disp,
                                          hwc2_layer_t layer,
                                          hwcomposer::HwcRect<int> frame) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_DISPLAY_FRAME pfnsetLayerDisplayFrame =
      reinterpret_cast<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(hwc2_dvc->getFunction(
          hwc2_dvc, HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME));

  if (pfnsetLayerDisplayFrame) {
    hwc_rect_t rect;
    rect.left = frame.left;
    rect.top = frame.top;
    rect.right = frame.right;
    rect.bottom = frame.bottom;
    ret = pfnsetLayerDisplayFrame(hwc2_dvc, disp, layer, rect);
  }
  return ret;
}

int Hwch::Interface::setLayerPlaneAlpha(hwc2_display_t disp, hwc2_layer_t layer,
                                        float alpha) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_PLANE_ALPHA pfnsetLayerPlaneAlpha =
      reinterpret_cast<HWC2_PFN_SET_LAYER_PLANE_ALPHA>(
          hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA));

  if (pfnsetLayerPlaneAlpha) {
    ret = pfnsetLayerPlaneAlpha(hwc2_dvc, disp, layer, alpha);
  }
  return ret;
}

int Hwch::Interface::setLayerVisibleRegion(hwc2_display_t disp,
                                           hwc2_layer_t layer,
                                           hwc_region_t visible) {
  int ret = -1;
  hwc2_device_t *hwc2_dvc =
      reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
  HWC2_PFN_SET_LAYER_VISIBLE_REGION pfnsetLayerVisibleRegion =
      reinterpret_cast<HWC2_PFN_SET_LAYER_VISIBLE_REGION>(hwc2_dvc->getFunction(
          hwc2_dvc, HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION));

  if (pfnsetLayerVisibleRegion) {
    ret = pfnsetLayerVisibleRegion(hwc2_dvc, disp, layer, visible);
  }
  return ret;
}

int Hwch::Interface::ValidateDisplay(hwc2_display_t display,
                                     uint32_t *outNumTypes,
                                     uint32_t *outNumRequests) {
  int ret = -1;
  if (hwc_composer_device) {
    hwc2_device_t *hwc2_dvc =
        reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
    HWC2_PFN_VALIDATE_DISPLAY pfnValidateDisplay =
        reinterpret_cast<HWC2_PFN_VALIDATE_DISPLAY>(
            hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_VALIDATE_DISPLAY));
    if (pfnValidateDisplay) {
      ret = pfnValidateDisplay(hwc2_dvc, display, outNumTypes, outNumRequests);
    }
  }
  return ret;  // ERROR
}

int Hwch::Interface::PresentDisplay(hwcval_display_contents_t *display, hwc2_display_t disp,
                                    int32_t *outPresentFence) {
  int ret = -1;

  if (hwc_composer_device) {
    hwc2_device_t *hwc2_dvc =
        reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
    HWCVAL_PFN_PRESENT_DISPLAY pfnPresentDisplay =
        reinterpret_cast<HWCVAL_PFN_PRESENT_DISPLAY>(
            hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_PRESENT_DISPLAY));
    if (pfnPresentDisplay) {
      ret = pfnPresentDisplay(display, hwc2_dvc, disp, outPresentFence);
    }
  }

  return ret;  // ERROR
}

int Hwch::Interface::GetReleaseFences(hwc2_display_t display, uint32_t* outNumElements,
        hwc2_layer_t* outLayers, int32_t* outFences) {
  int ret = -1;
  if (hwc_composer_device) {
    hwc2_device_t *hwc2_dvc =
        reinterpret_cast<hwc2_device_t *>(hwc_composer_device);
    HWC2_PFN_GET_RELEASE_FENCES pfngetReleaseFences =
        reinterpret_cast<HWC2_PFN_GET_RELEASE_FENCES>(
            hwc2_dvc->getFunction(hwc2_dvc, HWC2_FUNCTION_GET_RELEASE_FENCES));
    if (pfngetReleaseFences) {
      ret = pfngetReleaseFences(hwc2_dvc, display, outNumElements, outLayers, outFences);
    }
  }
  return ret;  // ERROR
}


int Hwch::Interface::EventControl(uint32_t disp, uint32_t event,
                                  uint32_t enable) {
  if (hwc_composer_device) {
  }
  return -1;  // ERROR
}

int Hwch::Interface::Blank(int disp, int blank) {
  if (hwc_composer_device) {
    if (disp < MAX_DISPLAYS) {
      mBlanked[disp] = blank;
    }

#if defined(HWC_DEVICE_API_VERSION_1_4)
    if (hwc_composer_device->common.version >= HWC_DEVICE_API_VERSION_1_4) {
      // return hwc_composer_device->setPowerMode(hwc_composer_device, disp,
      // blank ? HWC_POWER_MODE_OFF : HWC_POWER_MODE_NORMAL);
    } else
#endif
    {
      // return hwc_composer_device->blank(hwc_composer_device, disp,
      // blank);
    }
  }
  return -1;  // ERROR
}

int Hwch::Interface::IsBlanked(int disp) {
  if (disp < MAX_DISPLAYS) {
    return mBlanked[disp];
  } else {
    return 0;
  }
}

hwc2_device_t *Hwch::Interface::GetDevice(void) {
  return hwc_composer_device;
}

// private member functions

uint32_t Hwch::Interface::api_version(void) {
  uint32_t version = 0;
  if (hwc_composer_device) {
    version = hwc_composer_device->common.version;

    if ((MIN_HWC_HEADER_VERSION == 0) &&
        ((version & HARDWARE_API_VERSION_2_MAJ_MIN_MASK) == 0)) {
      // legacy version encoding
      version <<= 16;
    }
  }
  return version & HARDWARE_API_VERSION_2_MAJ_MIN_MASK;
}

bool Hwch::Interface::has_api_version(uint32_t version) {
  return api_version() >= (version & HARDWARE_API_VERSION_2_MAJ_MIN_MASK);
}

// RegisterProcs

void Hwch::Interface::hook_invalidate(const struct hwc_procs *procs) {
  HWCLOGD_COND(eLogHwchInterface, "hook_invalidate:");
}

void Hwch::Interface::hook_vsync(hwc2_callback_data_t callbackData, int disp,
                                 int64_t timestamp) {
  ALOGE("Called hook_vsync:");
  HWCLOGD_COND(eLogHwchInterface, "hook_vsync:");
  Hwch::System::getInstance().GetVSync().Signal(disp);
}

void Hwch::Interface::hook_hotplug(const struct hwc_procs *procs, int disp,
                                   int connected) {
  HWCLOGD_COND(eLogHwchInterface, "hook_hotplug:");
}

/////////////////////////

void Hwch::Interface::invalidate() {
  HWCLOGD_COND(eLogHwchInterface, "invalidate:");
  mRepaintNeeded = true;
}

void Hwch::Interface::vsync(int disp, int64_t timestamp) {
  HWCLOGD_COND(eLogHwchInterface, "vsync: disp=%d timestamp=%llu", disp,
               timestamp);
  Hwch::System::getInstance().GetVSync().Signal(disp);
}

void Hwch::Interface::hotplug(int disp, int connected) {
  HWCLOGD_COND(eLogHwchInterface, "hotplug: disp=%d connected=%d", disp,
               connected);

  mDisplayNeedsUpdate = disp;
}

void Hwch::Interface::UpdateDisplays(uint32_t hwcAcquireDelay) {
  if (mDisplayNeedsUpdate >= MAX_DISPLAYS) {
    HWCERROR(eCheckFrameworkProgError,
             "GetDisplayAttributes requested for invalid display %d",
             mDisplayNeedsUpdate);
  } else if (mDisplayNeedsUpdate > 0) {
    HWCLOGD_COND(eLogHarness, "Updating Display %d", mDisplayNeedsUpdate);
    GetDisplayAttributes(mDisplayNeedsUpdate);
    Hwch::System &system = Hwch::System::getInstance();
    Hwch::Display &display = system.GetDisplay(mDisplayNeedsUpdate);
    display.CreateFramebufferTarget();

    if (display.IsConnected()) {
      display.GetFramebufferTarget().SetHwcAcquireDelay(hwcAcquireDelay);
    }

    mDisplayNeedsUpdate = 0;
  }
}

uint32_t Hwch::Interface::NumDisplays() {
  return mNumDisplays;
}

bool Hwch::Interface::IsRepaintNeeded() {
  return mRepaintNeeded;
}

void Hwch::Interface::ClearRepaintNeeded() {
  mRepaintNeeded = false;
}
