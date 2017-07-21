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

#ifndef OS_ANDROID_HWC_IDISPLAY_BLANK_CONTROL_H
#define OS_ANDROID_HWC_IDISPLAY_BLANK_CONTROL_H

#include "hwcservicehelper.h"
#include <utils/RefBase.h>

namespace hwcomposer {

/**
 * Allows control of HDMI scaling for content
 * that does not match the native display resolution.
 * DEPRECATED: This is now a compataibilty layer over the supported API
 * and will be removed!  NO additional entry points should be added here.
 */
class IDisplayBlankControl : public android::RefBase
{
public:
    IDisplayBlankControl(uint32_t display) : mDisplay(display) {}

    // Enable blank, true---blank, false---unblank
    // Returns OK if succesful.
    status_t enableBlank(bool blank)
    {
        return HwcService_Display_EnableBlank(mHwcConn, mDisplay, blank ? HWCS_TRUE : HWCS_FALSE);
    }

private:
    HwcServiceConnection mHwcConn;
    uint32_t mDisplay;
};


} // namespace services

#endif // OS_ANDROID_HWC_IDISPLAY_BLANK_CONTROL_H
