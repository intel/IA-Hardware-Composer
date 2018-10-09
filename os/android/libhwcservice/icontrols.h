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

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include "hwcserviceapi.h"

namespace hwcomposer {

class IControls : public android::IInterface {
 public:
  DECLARE_META_INTERFACE(Controls);

  virtual status_t DisplaySetOverscan(uint32_t display, int32_t xoverscan,
                                      int32_t yoverscan) = 0;
  virtual status_t DisplayGetOverscan(uint32_t display, int32_t *xoverscan,
                                      int32_t *yoverscan) = 0;
  virtual status_t DisplaySetScaling(uint32_t display,
                                     EHwcsScalingMode eScalingMode) = 0;
  virtual status_t DisplayGetScaling(uint32_t display,
                                     EHwcsScalingMode *eScalingMode) = 0;
  virtual status_t DisplayEnableBlank(uint32_t display, bool blank) = 0;
  virtual status_t DisplayRestoreDefaultColorParam(uint32_t display,
                                                   EHwcsColorControl color) = 0;
  virtual status_t DisplayRestoreDefaultDeinterlaceParam(uint32_t display) = 0;
  virtual status_t DisplayGetColorParam(uint32_t display,
                                        EHwcsColorControl color, float *value,
                                        float *startvalue, float *endvalue) = 0;
  virtual status_t DisplaySetColorParam(uint32_t display,
                                        EHwcsColorControl color,
                                        float value) = 0;
  virtual status_t DisplaySetDeinterlaceParam(uint32_t display,
                                              EHwcsDeinterlaceControl mode) = 0;

  virtual std::vector<HwcsDisplayModeInfo> DisplayModeGetAvailableModes(
      uint32_t display) = 0;
  virtual status_t DisplayModeGetMode(uint32_t display,
                                      HwcsDisplayModeInfo *pMode) = 0;
  virtual status_t DisplayModeSetMode(uint32_t display,
                                      const uint32_t config) = 0;

  virtual status_t EnableHDCPSessionForDisplay(
      uint32_t display, EHwcsContentType content_type) = 0;

  virtual status_t EnableHDCPSessionForAllDisplays(
      EHwcsContentType content_type) = 0;

  virtual status_t DisableHDCPSessionForDisplay(uint32_t display) = 0;

  virtual status_t DisableHDCPSessionForAllDisplays() = 0;

  virtual status_t SetHDCPSRMForAllDisplays(const int8_t *SRM,
                                            uint32_t SRMLength) = 0;

  virtual status_t SetHDCPSRMForDisplay(uint32_t display, const int8_t *SRM,
                                        uint32_t SRMLength) = 0;

  virtual status_t VideoEnableEncryptedSession(uint32_t sessionID,
                                               uint32_t instanceID) = 0;
  virtual status_t VideoDisableAllEncryptedSessions(uint32_t sessionID) = 0;
  virtual status_t VideoDisableAllEncryptedSessions() = 0;
  virtual bool VideoIsEncryptedSessionEnabled(uint32_t sessionID,
                                              uint32_t instanceID) = 0;
  virtual status_t VideoSetOptimizationMode(EHwcsOptimizationMode mode) = 0;

  virtual status_t MdsUpdateVideoState(int64_t videoSessionID,
                                       bool isPrepared) = 0;
  virtual status_t MdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps) = 0;
  virtual status_t MdsUpdateInputState(bool state) = 0;

  virtual status_t WidiGetSingleDisplay(bool *pEnabled) = 0;
  virtual status_t WidiSetSingleDisplay(bool enable) = 0;
};

class BnControls : public android::BnInterface<IControls> {
 public:
  status_t onTransact(uint32_t, const android::Parcel &, android::Parcel *,
                      uint32_t) override;
};

}  // namespace hwcomposer

#endif  // OS_ANDROID_HWC_ICONTROLS_H_
