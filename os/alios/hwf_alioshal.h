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

#ifndef _ALIOS_HWC1_H_
#define _ALIOS_HWC1_H_

#include <cutils/hwflinger.h>
#include <cutils/hwflinger_defs.h>

#include <fcntl.h>
#include <gpudevice.h>
#include <hwclayer.h>
#include <nativedisplay.h>

#include <Hal.h>

#include <memory>

#define USER_FENCE_SYNC 0

namespace hwcomposer {
typedef struct HwfLayer {
  ~HwfLayer() {
    delete hwc_layer_;
    hwc_layer_ = NULL;
  }

  HwfLayer() = default;

  HwfLayer(const HwfLayer& rhs) = delete;
  HwfLayer& operator=(const HwfLayer& rhs) = delete;

  struct yalloc_handle native_handle_;
  hwcomposer::HwcLayer* hwc_layer_ = NULL;
  uint32_t index_ = 0;

  int InitFromHwcLayer(hwf_layer_t* sf_layer);
} HwfLayer;

typedef struct HwfDisplay {
  hwcomposer::NativeDisplay* display_ = NULL;
  bool gl_composition_ = false;
} HwfDisplay;

struct HwfDevice {
  hwf_device_t base;

  ~HwfDevice(){};

  hwcomposer::GpuDevice device_;
  std::vector<HwfDisplay> extended_displays_;
  HwfDisplay primary_display_;
  HwfDisplay virtual_display_;
  bool disable_explicit_sync_ = false;

  HwfDisplay* GetDisplay(int display);

  static int detect(struct hwf_device_t* device, int dispCount,
                    hwf_display_t** displays);

  static int flip(struct hwf_device_t* device, int dispCount,
                  hwf_display_t** displays);

  static int setEventState(struct hwf_device_t* device, int disp, int event,
                           int enabled);

  static int setDisplayState(struct hwf_device_t* device, int disp, int state);

  static int lookup(struct hwf_device_t* device, int what, int* value);

  static void registerCallback(struct hwf_device_t* device,
                               hwf_callback_t const* callback);

  static int queryDispConfigs(struct hwf_device_t* device, int disp,
                              uint32_t* configs, int* numConfigs);

  static int queryDispAttribs(struct hwf_device_t* device, int disp,
                              uint32_t config, const uint32_t* attributes,
                              int32_t* values);

  static void dump(struct hwf_device_t* device, char* buff, int buff_len);
};
}

#endif
