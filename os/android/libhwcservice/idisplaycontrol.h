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

#ifndef OS_ANDROID_HWC_IDISPLAY_CONTROL_H
#define OS_ANDROID_HWC_IDISPLAY_CONTROL_H

// #include <binder/IInterface.h>
// #include <binder/Parcel.h>

#include "idisplayscalingcontrol.h"
#include "idisplayoverscancontrol.h"
#include "idisplaymodecontrol.h"
#include "icolorcontrol.h"
#include "idisplayblankcontrol.h"


namespace hwcomposer {


using namespace android;


/**
 * Allows control of HDMI display
 * DEPRECATED: This is now a compataibilty layer over the supported API
 * and will be removed!  NO additional entry points should be added here.
 */
class IDisplayControl : public RefBase
{
public:
    IDisplayControl(uint32_t display)
     : mDisplay(display)
    {
        mHwcs = HwcService_Connect();
    }

    ~IDisplayControl()
    {
        HwcService_Disconnect(mHwcs);
    }

    /// restore default control (overscan, scale, ...)
    virtual status_t restoreAllDefaults()
    {
        // Unsupported in wrapper.
        return -1;
    }

    virtual sp<IDisplayOverscanControl> getOverscanControl()
    {
        return new IDisplayOverscanControl(mDisplay);
    }
    virtual sp<IDisplayScalingControl> getScalingControl()
    {
        return new IDisplayScalingControl(mDisplay);
    }
    virtual sp<IDisplayModeControl> getModeControl()
    {
        return new IDisplayModeControl(mDisplay);
    }
    virtual sp<IDisplayBlankControl> getBlankControl()
    {
        return new IDisplayBlankControl(mDisplay);
    }

#ifdef EXPERIMENTAL
    ///
    virtual sp<IColorControl> getBrightnessControl()
    {
        // Unsupported in wrapper
        return NULL;
    }
    virtual sp<IColorControl> getContrastControl()
    {
        // Unsupported in wrapper
        return NULL;
    }
    virtual sp<IColorControl> getGammaControl()
    {
        // Unsupported in wrapper
        return NULL;
    }
    virtual sp<IColorControl> getHueControl()
    {
        // Unsupported in wrapper
        return NULL;
    }
    virtual sp<IColorControl> getSaturationControl()
    {
        // Unsupported in wrapper
        return NULL;
    }
#else
    enum {
        COLOR_BRIGHTNESS = HWCS_COLOR_BRIGHTNESS,
        COLOR_CONTRAST = HWCS_COLOR_CONTRAST,
        COLOR_GAMMA = HWCS_COLOR_GAMMA,
        COLOR_SATURATION = HWCS_COLOR_SATURATION,
        COLOR_HUE = HWCS_COLOR_HUE,
    };
    virtual sp<IColorControl> getColorControl(int32_t)
    {
        // Unsupported in wrapper
        return NULL;
    }
#endif

#ifdef EXPERIMENTAL
    /// switch into power safe mode (soft disconnect?)
    virtual status_t powerOff(int off)
    {
        // Unsupported in wrapper
        return NULL;
    }
#endif
private:
    HWCSHANDLE mHwcs;
    uint32_t mDisplay;
};


} // namespace services

#endif // OS_ANDROID_HWC_IDISPLAY_CONTROL_H
