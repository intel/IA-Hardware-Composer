/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef HWCVAL_H
#define HWCVAL_H
#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include "public/nativebufferhandler.h"

enum {
  /*
   * HWC_GEOMETRY_CHANGED is set by SurfaceFlinger to indicate that the list
   * passed to (*prepare)() has changed by more than just the buffer handles
   * and acquire fences.
   */
  TEMPHWC_GEOMETRY_CHANGED = 0x00000001,
};
typedef struct hwcval_layer {
  hwc2_layer_t hwc2_layer;
  int32_t compositionType;
  uint32_t hints;
  uint32_t flags;
  union {
    hwc_color_t backgroundColor;
    struct {
      union {
        HWCNativeHandle gralloc_handle;
        const native_handle_t *sidebandStream;
      };
      uint32_t transform;
      int32_t blending;
      hwc_frect_t sourceCropf;
      hwc_rect_t displayFrame;
      hwc_region_t visibleRegionScreen;
      int acquireFence;
      int releaseFence;
      uint8_t planeAlpha;
      uint8_t _pad[3];
      hwc_region_t surfaceDamage;
    };
  };
} hwcval_layer_t;

typedef struct hwcval_display_contents {
  /* These fields are used for virtual displays when the h/w composer
   * version is at least HWC_DEVICE_VERSION_1_3. */
  struct {
    HWCNativeHandle outbuf;
  };
  size_t numHwLayers;
  hwcval_layer_t hwLayers[10];
  hwc2_display_t *display;
  int32_t outPresentFence;
} hwcval_display_contents_t;

typedef int32_t /*hwc2_error_t*/ (*HWCVAL_PFN_PRESENT_DISPLAY)(hwcval_display_contents_t *display, hwc2_device_t* device, hwc2_display_t disp, int32_t* outPresentFence);
#endif /* HWCVAL_H */
