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

#ifndef __HWCHINTERFACE_H__
#define __HWCHINTERFACE_H__

#include <dlfcn.h>
#include "Hwcval.h"
#include <hardware/hwcomposer2.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/StrongPointer.h>
#include <utils/Timers.h>
#include <utils/Vector.h>
#include <utils/BitSet.h>
#include <string>

#include "HwchDefs.h"

#define MIN_HWC_HEADER_VERSION 0

using namespace android;

namespace Hwch {
class Interface {
 public:
  Interface();
  hwcomposer::NativeBufferHandler *bufHandler;
  int Initialise(void);
  void LoadHwcModule();
  int RegisterProcs(void);
  int GetDisplayAttributes();
  int GetDisplayAttributes(uint32_t disp);
  int Prepare(size_t numDisplays, hwc2_display_t **displays);
  int Set(size_t numDisplays, hwc2_display_t **displays);
  int EventControl(uint32_t disp, uint32_t event, uint32_t enable);
  int Blank(int disp, int blank);
  int IsBlanked(int disp);

  /*HWC2 functions*/
  int ValidateDisplay(hwc2_display_t display, uint32_t *outNumTypes,
                      uint32_t *outNumRequests);
  int PresentDisplay(hwcval_display_contents_t *display, hwc2_display_t disp, int32_t *outPresentFence);
  int CreateLayer(hwc2_display_t disp, hwc2_layer_t *outLayer);
  int setLayerCompositionType(hwc2_display_t disp, hwc2_layer_t layer,
                              int32_t type);
  int setLayerBuffer(hwc2_display_t disp, hwc2_layer_t layer,
                     buffer_handle_t buffer, int32_t acquireFence);
  int setLayerBlendMode(hwc2_display_t disp, hwc2_layer_t layer, int32_t mode);
  int setLayerTransform(hwc2_display_t disp, hwc2_layer_t layer,
                        int32_t transform);
  int setLayerSourceCrop(hwc2_display_t disp, hwc2_layer_t layer,
                         hwcomposer::HwcRect<float> crop);
  int setLayerDisplayFrame(hwc2_display_t disp, hwc2_layer_t layer,
                           hwcomposer::HwcRect<int> frame);
  int setLayerPlaneAlpha(hwc2_display_t disp, hwc2_layer_t layer, float alpha);
  int setLayerVisibleRegion(hwc2_display_t disp, hwc2_layer_t layer,
                            hwc_region_t visible);
  int GetReleaseFences(hwc2_display_t display, uint32_t* outNumElements,
        hwc2_layer_t* outLayers, int32_t* outFences);

  void UpdateDisplays(uint32_t hwcAcquireDelay);
  uint32_t NumDisplays();

  hwc2_device *GetDevice(void);

  bool IsRepaintNeeded();
  void ClearRepaintNeeded();

 private:
  uint32_t api_version(void);
  bool has_api_version(uint32_t version);

  // RegisterProcs
  static void hook_invalidate(const struct hwc_procs *procs);
  static void hook_vsync(hwc2_callback_data_t callbackData, int disp,
                         int64_t timestamp);
  static void hook_hotplug(const struct hwc_procs *procs, int disp,
                           int connected);
  void invalidate(void);
  void vsync(int disp, int64_t timestamp);
  void hotplug(int disp, int connected);

 private:
  struct cb_context;

  hwc2_device *hwc_composer_device;

  uint32_t mDisplayNeedsUpdate;  // Hotplug received on this display and not
                                 // processed yet
  uint32_t mNumDisplays;         // (Index of last connected display) + 1
  bool mRepaintNeeded;
  int mBlanked[MAX_DISPLAYS];
};
}

#endif  // __HWCHINTERFACE_H__
