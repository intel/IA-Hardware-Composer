/*
// Copyright (c) 2017 Intel Corporation
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

#ifndef OS_ANDROID_IDISPLAY_OVERSCAN_CONTROL_H_
#define OS_ANDROID_IDISPLAY_OVERSCAN_CONTROL_H_

#include <utils/RefBase.h>
#include "hwcservicehelper.h"

namespace hwcomposer {

/**
 * Allows control of HDMI overscan
 */
class IDisplayOverscanControl : public android::RefBase {
 public:
  IDisplayOverscanControl(uint32_t display) : mDisplay(display) {
  }

  enum {
    MAX_OVERSCAN = HWCS_MAX_OVERSCAN,  // The limit of the control parameters
                                       // are +/-MAX_OVERSCAN inclusive.
    RANGE = HWCS_OVERSCAN_RANGE,  // RANGE describes the % of the display size a
                                  // max control setting will adjust by.
  };

  /// Set overscan in the range +/-MAX_OVERSCAN inclusive.
  // -ve : zoom/crop the image  (increase display overscan).
  // +ve : shrink the image (decrease display overscan).
  status_t setOverscan(int32_t xoverscan, int32_t yoverscan) {
    return HwcService_Display_SetOverscan(mHwcConn, mDisplay, xoverscan,
                                          yoverscan);
  }

  // Get last set overscan.
  // Returns INVALID_OPERATION if overscan has not been set and
  // xoverscan/yoverscan are untouched.
  status_t getOverscan(int32_t *xoverscan, int32_t *yoverscan) {
    return HwcService_Display_GetOverscan(mHwcConn, mDisplay, xoverscan,
                                          yoverscan);
  }

 private:
  HwcServiceConnection mHwcConn;
  uint32_t mDisplay;
};

}  // namespace hwcomposer

#endif  // OS_ANDROID_IDISPLAY_OVERSCAN_CONTROL_H_
