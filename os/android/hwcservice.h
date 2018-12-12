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
#ifndef OS_ANDROID_HWCSERVICE_H_
#define OS_ANDROID_HWCSERVICE_H_

#include <list>

#include <binder/IInterface.h>
#include <utils/String8.h>
#include "libhwcservice/icontrols.h"
#include "libhwcservice/idiagnostic.h"
#include "libhwcservice/iservice.h"
#include "spinlock.h"
#include "utils_android.h"

namespace android {
class IAHWC2;
using namespace hwcomposer;

class HwcService : public BnService {
 public:
  class Diagnostic : public BnDiagnostic {
   public:
    Diagnostic(IAHWC2& IAHWC2) : mHwc(IAHWC2) {
      HWC_UNUSED(mHwc);
    }

    status_t ReadLogParcel(Parcel* parcel) override;
    void EnableDisplay(uint32_t d) override;
    void DisableDisplay(uint32_t d, bool bBlank) override;
    void MaskLayer(uint32_t d, uint32_t layer, bool bHide) override;
    void DumpFrames(uint32_t d, int32_t frames, bool bSync) override;

   private:
    IAHWC2& mHwc;
  };

  bool Start(IAHWC2& hwc);

  sp<IDiagnostic> GetDiagnostic();
  sp<IControls> GetControls();

  android::String8 GetHwcVersion();

  void DumpOptions(void);
  status_t SetOption(android::String8 option, android::String8 optionValue);
  status_t EnableLogviewToLogcat(bool enable = true);

  class Controls : public BnControls {
   public:
    Controls(IAHWC2& hwc, HwcService& hwcService);
    virtual ~Controls();

    status_t DisplaySetOverscan(uint32_t display, int32_t xoverscan,
                                int32_t yoverscan);
    status_t DisplayGetOverscan(uint32_t display, int32_t* xoverscan,
                                int32_t* yoverscan);
    status_t DisplaySetScaling(uint32_t display, EHwcsScalingMode eScalingMode);
    status_t DisplayGetScaling(uint32_t display,
                               EHwcsScalingMode* eScalingMode);
    status_t DisplayEnableBlank(uint32_t display, bool blank);
    status_t DisplayRestoreDefaultColorParam(uint32_t display,
                                             EHwcsColorControl color);
    status_t DisplayRestoreDefaultDeinterlaceParam(uint32_t display);
    status_t DisplayGetColorParam(uint32_t display, EHwcsColorControl color,
                                  float* value, float* startvalue,
                                  float* endvalue);
    status_t DisplaySetColorParam(uint32_t display, EHwcsColorControl color,
                                  float value);
    status_t DisplaySetDeinterlaceParam(uint32_t display,
                                        EHwcsDeinterlaceControl mode);

    std::vector<HwcsDisplayModeInfo> DisplayModeGetAvailableModes(
        uint32_t display);
    status_t DisplayModeGetMode(uint32_t display, HwcsDisplayModeInfo* pMode);
    status_t DisplayModeSetMode(uint32_t display, const uint32_t config);

    status_t EnableHDCPSessionForDisplay(uint32_t connector,
                                         EHwcsContentType content_type);

    status_t EnableHDCPSessionForAllDisplays(EHwcsContentType content_type);

    status_t DisableHDCPSessionForDisplay(uint32_t connector);

    status_t DisableHDCPSessionForAllDisplays();

    status_t SetHDCPSRMForAllDisplays(const int8_t* SRM, uint32_t SRMLength);

    status_t SetHDCPSRMForDisplay(uint32_t connector, const int8_t* SRM,
                                  uint32_t SRMLength);
    uint32_t GetDisplayIDFromConnectorID(uint32_t connector_id);
    status_t VideoEnableEncryptedSession(uint32_t sessionID,
                                         uint32_t instanceID);
    status_t VideoDisableAllEncryptedSessions(uint32_t sessionID);
    status_t VideoDisableAllEncryptedSessions();
    bool VideoIsEncryptedSessionEnabled(uint32_t sessionID,
                                        uint32_t instanceID);
    bool needSetKeyFrameHint();
    status_t VideoSetOptimizationMode(EHwcsOptimizationMode mode);
    status_t MdsUpdateVideoState(int64_t videoSessionID, bool isPrepared);
    status_t MdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps);
    status_t MdsUpdateInputState(bool state);
    status_t WidiGetSingleDisplay(bool* pEnabled);
    status_t WidiSetSingleDisplay(bool enable);

   private:
    IAHWC2& mHwc;
    HwcService& mHwcService;
    bool mbHaveSessionsEnabled;
    EHwcsOptimizationMode mCurrentOptimizationMode;
  };

  enum ENotification {
    eInvalidNofiy = 0,
    eOptimizationMode,
    eMdsUpdateVideoState,
    eMdsUpdateInputState,
    eMdsUpdateVideoFps,
    ePavpEnableEncryptedSession,
    ePavpDisableEncryptedSession,
    ePavpDisableAllEncryptedSessions,
    ePavpIsEncryptedSessionEnabled,
    eWidiGetSingleDisplay,
    eWidiSetSingleDisplay,
    eNeedSetKeyFrameHint,
  };

  class NotifyCallback {
   public:
    virtual ~NotifyCallback() {
    }
    virtual void notify(ENotification notify, int32_t paraCnt,
                        int64_t para[]) = 0;
  };

  void RegisterListener(ENotification notify, NotifyCallback* pCallback);
  void UnregisterListener(ENotification notify, NotifyCallback* pCallback);
  void Notify(ENotification notify, int32_t paraCnt, int64_t para[]);

 private:
  HwcService();
  virtual ~HwcService();
  friend class IAHWC2;

  struct Notification {
    Notification() : mWhat(eInvalidNofiy), mpCallback(NULL) {
    }
    Notification(ENotification what, NotifyCallback* pCallback)
        : mWhat(what), mpCallback(pCallback) {
    }
    ENotification mWhat;
    NotifyCallback* mpCallback;
  };

  SpinLock lock_;
  IAHWC2* mpHwc;
  bool initialized_;

  sp<IDiagnostic> mpDiagnostic;

  std::vector<Notification> mNotifications;
};
}  // namespace android

#endif  // OS_ANDROID_HWCSERVICE_H_
