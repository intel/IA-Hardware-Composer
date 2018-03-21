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

#include "HwcTestState.h"
#include "HwcvalLogDisplay.h"

Hwcval::LogDisplayMapping::LogDisplayMapping()
    : mLogDisplayIx(eNoDisplayIx),
      mDisplayIx(eNoDisplayIx),
      mFlags(0),
      mSrcX(0),
      mSrcY(0),
      mSrcW(0),
      mSrcH(0),
      mDstX(0),
      mDstY(0),
      mDstW(0),
      mDstH(0) {
}

Hwcval::LogDisplayMapping::LogDisplayMapping(uint32_t logDisp, uint32_t disp,
                                             uint32_t flags, uint32_t sx,
                                             uint32_t sy, uint32_t sw,
                                             uint32_t sh, uint32_t dx,
                                             uint32_t dy, uint32_t dw,
                                             uint32_t dh)
    : mLogDisplayIx(logDisp),
      mDisplayIx(disp),
      mFlags(flags),
      mSrcX(sx),
      mSrcY(sy),
      mSrcW(sw),
      mSrcH(sh),
      mDstX(dx),
      mDstY(dy),
      mDstW(dw),
      mDstH(dh) {
}

void Hwcval::LogDisplayMapping::Log(const char* str) {
  HWCLOGD_COND(eLogMosaic, "%s %d %d,%d %dx%d -> %d %d,%d %dx%d", str,
               mLogDisplayIx, mSrcX, mSrcY, mSrcW, mSrcH, mDisplayIx, mDstX,
               mDstY, mDstW, mDstH);
}

Hwcval::LogDisplay::LogDisplay(uint32_t displayIx)
    : mVSyncPeriod(0),
      mWidth(0),
      mHeight(0),
      mXDPI(0),
      mYDPI(0),
      mConfigId(0),
      mDisplayIx(displayIx) {
}

void Hwcval::LogDisplay::SetConfigs(uint32_t* configs, uint32_t numConfigs) {
  mConfigs.clear();
  if(configs) {
    mConfigs.insert(mConfigs.begin(), configs, configs + numConfigs - 4);

    if (numConfigs > 0) {
      for (uint32_t i = 0; i < numConfigs; ++i) {
        if (configs[i] == mConfigId) {
          // Currently set config id is valid, so we can keep it
          HWCLOGD_COND(eLogHwcDisplayConfigs,
          "D%d: SetConfigs current config is still %x", mDisplayIx,
           mConfigId);
           return;
         }
       }

       mConfigId = configs[0];
       HWCLOGD_COND(eLogHwcDisplayConfigs,
       "D%d: SetConfigs current config is now %x", mDisplayIx,
        mConfigId);
    }
  }
}

void Hwcval::LogDisplay::SetActiveConfig(uint32_t configId) {
  if (configId != mConfigId) {
    HWCLOGD_COND(eLogHwcDisplayConfigs, "D%d: SetActiveConfig %x", mDisplayIx,
                 configId);
    mConfigId = configId;
    mWidth = 0;
    mHeight = 0;
    mVSyncPeriod = 0;
  }
}

void Hwcval::LogDisplay::SetDisplayAttributes(uint32_t configId,
                                              const int32_t attribute,
                                              int32_t* values) {
  if (configId == mConfigId) {
    HWCLOGD_COND(eLogHwcDisplayConfigs,
                 "D%d: SetDisplayAttributes, config %x is current", mDisplayIx,
                 configId);
      switch (attribute) {
        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
          mVSyncPeriod = *values;
          break;

        case HWC2_ATTRIBUTE_WIDTH:
          mWidth = *values;
          HWCLOGD_COND(eLogHwcDisplayConfigs, "D%d LogDisplay: set width to %d",
                       mDisplayIx, mWidth);
          break;

        case HWC2_ATTRIBUTE_HEIGHT:
          mHeight = *values;
          HWCLOGD_COND(eLogHwcDisplayConfigs,
                       "D%d LogDisplay: set height to %d", mDisplayIx, mHeight);
          break;

        case HWC_DISPLAY_DPI_X:
          mXDPI = *values;
          break;

        case HWC_DISPLAY_DPI_Y:
          mYDPI = *values;
          break;

        default:
          HWCLOGW("Unknown display attribute %d", attribute);
      };
  } else {
    HWCLOGD(
        "D%d: LogDisplay::SetDisplayAttributes: config %d is not current "
        "config %x",
        mDisplayIx, configId, mConfigId);
  }
}
