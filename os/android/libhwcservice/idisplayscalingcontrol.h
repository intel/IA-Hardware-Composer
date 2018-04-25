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

#ifndef OS_ANDROID_IDISPLAY_SCALING_CONTROL_H_
#define OS_ANDROID_IDISPLAY_SCALING_CONTROL_H_

#include <utils/RefBase.h>
#include "hwcservicehelper.h"

namespace hwcomposer {

/**
 * Allows control of HDMI scaling for content
 * that does not match the native display resolution.
 */
class IDisplayScalingControl : public android::RefBase {
 public:
  IDisplayScalingControl(uint32_t display) : mDisplay(display) {
  }

  enum EScalingMode {
    SCALE_CENTRE = HWCS_SCALE_CENTRE,    // Present the content centred at 1:1
                                         // source resolution.
    SCALE_STRETCH = HWCS_SCALE_STRETCH,  // Do not preserve aspect ratio - scale
                                         // to fill the display without
                                         // cropping.
    SCALE_FIT = HWCS_SCALE_FIT,    // Preserve aspect ratio - scale to closest
                                   // edge (may be letterboxed or pillarboxed).
    SCALE_FILL = HWCS_SCALE_FILL,  // Preserve aspect ratio - scale to fill the
                                   // display (may crop the content).
    SCALE_MAX_ENUM = HWCS_SCALE_MAX_ENUM  // End of enum.
  };

  /// Set scaling to one of EScalingMode.
  // Returns OK if succesful.
  status_t setScaling(EScalingMode eScalingMode) {
    return HwcService_Display_SetScaling(mHwcConn, mDisplay,
                                         (EHwcsScalingMode)eScalingMode);
  }

  // Get last set scaling.
  // Returns OK if succesful.
  // Returns INVALID_OPERATION if scaling has not been set and eScalingMode is
  // untouched.
  status_t getScaling(EScalingMode *eScalingMode) {
    return HwcService_Display_GetScaling(mHwcConn, mDisplay,
                                         (EHwcsScalingMode *)eScalingMode);
  }

 private:
  HwcServiceConnection mHwcConn;
  uint32_t mDisplay;
};

}  // namespace hwcomposer

#endif  // OS_ANDROID_IDISPLAY_SCALING_CONTROL_H_
