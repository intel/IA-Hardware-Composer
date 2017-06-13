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

#ifndef OS_ANDROID_HWC_ICONTROLS_H_
#define OS_ANDROID_HWC_ICONTROLS_H_

#include "hwcserviceapi.h"
#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace hwcomposer {

class IControls : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(Controls);

  virtual status_t displaySetOverscan(uint32_t display, int32_t xoverscan,
                                      int32_t yoverscan) = 0;
  virtual status_t displayGetOverscan(uint32_t display, int32_t *xoverscan,
                                      int32_t *yoverscan) = 0;
  virtual status_t displaySetScaling(uint32_t display,
                                     EHwcsScalingMode eScalingMode) = 0;
  virtual status_t displayGetScaling(uint32_t display,
                                     EHwcsScalingMode *eScalingMode) = 0;
  virtual status_t displayEnableBlank(uint32_t display, bool blank) = 0;
  virtual status_t displayRestoreDefaultColorParam(uint32_t display,
                                                   EHwcsColorControl color) = 0;
  virtual status_t displayGetColorParam(uint32_t display,
                                        EHwcsColorControl color, float *value,
                                        float *startvalue, float *endvalue) = 0;
  virtual status_t displaySetColorParam(uint32_t display,
                                        EHwcsColorControl color,
                                        float value) = 0;

  virtual android::Vector<HwcsDisplayModeInfo> displayModeGetAvailableModes(
      uint32_t display) = 0;
  virtual status_t displayModeGetMode(uint32_t display,
                                      HwcsDisplayModeInfo *pMode) = 0;
  virtual status_t displayModeSetMode(uint32_t display,
                                      const HwcsDisplayModeInfo *pMode) = 0;

  virtual status_t videoEnableEncryptedSession(uint32_t sessionID,
                                               uint32_t instanceID) = 0;
  virtual status_t videoDisableEncryptedSession(uint32_t sessionID) = 0;
  virtual status_t videoDisableAllEncryptedSessions() = 0;
  virtual bool videoIsEncryptedSessionEnabled(uint32_t sessionID,
                                              uint32_t instanceID) = 0;
  virtual status_t videoSetOptimizationMode(EHwcsOptimizationMode mode) = 0;

  virtual status_t mdsUpdateVideoState(int64_t videoSessionID,
                                       bool isPrepared) = 0;
  virtual status_t mdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps) = 0;
  virtual status_t mdsUpdateInputState(bool state) = 0;

  virtual status_t widiGetSingleDisplay(bool *pEnabled) = 0;
  virtual status_t widiSetSingleDisplay(bool enable) = 0;
};

class BnControls : public android::BnInterface<IControls> {
 public:
  virtual status_t onTransact(uint32_t, const android::Parcel &,
                              android::Parcel *, uint32_t);
};

}  // namespace hwcomposer

#endif  // OS_ANDROID_HWC_ICONTROLS_H_
