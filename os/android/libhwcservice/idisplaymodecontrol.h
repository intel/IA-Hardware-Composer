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

#ifndef OS_ANDROID_HWC_IDISPLAY_MODE_CONTROL_H
#define OS_ANDROID_HWC_IDISPLAY_MODE_CONTROL_H

#include "hwcservicehelper.h"
#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace hwcomposer {


/**
 * Allows change of HDMI display mode.
 * DEPRECATED: This is now a compataibilty layer over the supported API
 * and will be removed!  NO additional entry points should be added here.
 */
class IDisplayModeControl : public android::RefBase
{
public:
    IDisplayModeControl(uint32_t display) : mDisplay(display) {}

    struct Info
    {
        uint32_t width;
        uint32_t height;
        uint32_t refresh;
        uint32_t flags;
        uint32_t ratio;

        enum {
            FLAG_NONE           = HWCS_MODE_FLAG_NONE,
            FLAG_PREFERRED      = HWCS_MODE_FLAG_PREFERRED,
            FLAG_SECURE         = HWCS_MODE_FLAG_SECURE,
            FLAG_INTERLACED     = HWCS_MODE_FLAG_INTERLACED,
            FLAG_CURRENT        = HWCS_MODE_FLAG_CURRENT,
        };
    };

    /// Enumerations for common aspect ratios
    /// Any ratio can be supported, with the upper 16 bits containing one dimension,
    /// the lower 16 bits contains the lower dimension
    enum {
        ASPECT_RATIO_ANY    = HWCS_MODE_ASPECT_RATIO_ANY,
        ASPECT_RATIO_4_3    = HWCS_MODE_ASPECT_RATIO_4_3,
        ASPECT_RATIO_16_9   = HWCS_MODE_ASPECT_RATIO_16_9,
    };

    /// restore default mode
    status_t restorePreferredMode()
    {
        android::Vector<Info> modes = getAvailableModes();
        uint32_t mode = 0;
        for (; mode < modes.size(); ++mode)
        {
            if (modes[mode].flags & Info::FLAG_PREFERRED)
            {
                break;
            }
        }
        if ((mode >= modes.size()) || (modes.size() == 0))
        {
            return -1;
        }

        HwcsDisplayModeInfo info;
        info.width = modes[mode].width;
        info.height = modes[mode].height;
        info.refresh = modes[mode].refresh;
        info.flags = modes[mode].flags;
        info.ratio = modes[mode].ratio;
        return HwcService_DisplayMode_SetMode(mHwcConn, mDisplay, &info);
    }

    /// query all available modes
    android::Vector<Info> getAvailableModes()
    {
        android::Vector<Info> outModes;
        status_t status = HwcService_DisplayMode_GetAvailableModes(mHwcConn, mDisplay, 0, NULL);
        if (status > 0)
        {
            outModes.resize(status);
            // Somewhat naughty.
            // Should really not assume that Info is the same as HwcsDisplayModeInfo!
            HwcService_DisplayMode_GetAvailableModes(mHwcConn, mDisplay, status, (HwcsDisplayModeInfo*)outModes.editArray());
        }
        return outModes;
    }

    /// get current mode
    status_t getMode(uint32_t *width, uint32_t *height, uint32_t *refresh, uint32_t *flags, uint32_t *ratio)
    {
        HwcsDisplayModeInfo info;
        status_t status = HwcService_DisplayMode_GetMode(mHwcConn, mDisplay, &info);
        if (status == android::OK)
        {
            *width   = info.width;
            *height  = info.height;
            *refresh = info.refresh;
            *flags   = info.flags;
            *ratio   = info.ratio;
        }
        return status;
    }

    /// set mode
    status_t setMode(uint32_t width, uint32_t height, uint32_t refresh, uint32_t flags, uint32_t ratio)
    {
        HwcsDisplayModeInfo info;
        info.width = width;
        info.height = height;
        info.refresh = refresh;
        info.flags = flags;
        info.ratio = ratio;
        return HwcService_DisplayMode_SetMode(mHwcConn, mDisplay, &info);
    }

#ifdef EXPERIMENTAL
    // scaling
    enum {
        SCALE_KEEP_ASPECT_RATIO = 0,
        SCALE_CENTER            = 1,
        SCALE_FULLSCREEN        = 2,
    };
    virtual status_t getScaleMode(uint32_t *)
    {
        return INVALID_OPERATION;
    }
    virtual status_t setScaleMode(uint32_t)
    {
        return INVALID_OPERATION;
    }
#endif

private:
    HwcServiceConnection mHwcConn;
    uint32_t mDisplay;
};


} // namespace services

#endif // OS_ANDROID_HWC_IDISPLAY_MODE_CONTROL_H
