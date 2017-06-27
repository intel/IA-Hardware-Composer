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
#include "utils_android.h"

namespace android {
class DrmHwcTwo;
using namespace hwcomposer;

class HwcService : public BnService {
 public:
  class Diagnostic : public BnDiagnostic {
   public:
    Diagnostic(DrmHwcTwo& DrmHwcTwo) : mHwc(DrmHwcTwo) {
      HWC_UNUSED(mHwc);
    }

    virtual status_t readLogParcel(Parcel* parcel);
    virtual void enableDisplay(uint32_t d);
    virtual void disableDisplay(uint32_t d, bool bBlank);
    virtual void maskLayer(uint32_t d, uint32_t layer, bool bHide);
    virtual void dumpFrames(uint32_t d, int32_t frames, bool bSync);

   private:
    DrmHwcTwo& mHwc;
  };

  bool Start(DrmHwcTwo& hwc);

  sp<IDiagnostic> getDiagnostic();
  sp<IControls> getControls();

  android::String8 GetHwcVersion();

  void dumpOptions(void);
  status_t setOption(android::String8 option, android::String8 optionValue);
  status_t enableLogviewToLogcat(bool enable = true);

  class Controls : public BnControls {
   public:
    Controls(DrmHwcTwo& hwc, HwcService& hwcService);
    virtual ~Controls();

    status_t displaySetOverscan(uint32_t display, int32_t xoverscan,
                                int32_t yoverscan);
    status_t displayGetOverscan(uint32_t display, int32_t* xoverscan,
                                int32_t* yoverscan);
    status_t displaySetScaling(uint32_t display, EHwcsScalingMode eScalingMode);
    status_t displayGetScaling(uint32_t display,
                               EHwcsScalingMode* eScalingMode);
    status_t displayEnableBlank(uint32_t display, bool blank);
    status_t displayRestoreDefaultColorParam(uint32_t display,
                                             EHwcsColorControl color);
    status_t displayGetColorParam(uint32_t display, EHwcsColorControl color,
                                  float* value, float* startvalue,
                                  float* endvalue);
    status_t displaySetColorParam(uint32_t display, EHwcsColorControl color,
                                  float value);

    android::Vector<HwcsDisplayModeInfo> displayModeGetAvailableModes(
        uint32_t display);
    status_t displayModeGetMode(uint32_t display, HwcsDisplayModeInfo* pMode);
    status_t displayModeSetMode(uint32_t display,
                                const HwcsDisplayModeInfo* pMode);

    status_t videoEnableEncryptedSession(uint32_t sessionID,
                                         uint32_t instanceID);
    status_t videoDisableEncryptedSession(uint32_t sessionID);
    status_t videoDisableAllEncryptedSessions();
    bool videoIsEncryptedSessionEnabled(uint32_t sessionID,
                                        uint32_t instanceID);
    bool needSetKeyFrameHint();
    status_t videoSetOptimizationMode(EHwcsOptimizationMode mode);
    status_t mdsUpdateVideoState(int64_t videoSessionID, bool isPrepared);
    status_t mdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps);
    status_t mdsUpdateInputState(bool state);
    status_t widiGetSingleDisplay(bool* pEnabled);
    status_t widiSetSingleDisplay(bool enable);

   private:
    DrmHwcTwo& mHwc;
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

  void registerListener(ENotification notify, NotifyCallback* pCallback);
  void unregisterListener(ENotification notify, NotifyCallback* pCallback);
  void notify(ENotification notify, int32_t paraCnt, int64_t para[]);

 private:
  HwcService();
  virtual ~HwcService();
  friend class DrmHwcTwo;

  struct Notification {
    Notification() : mWhat(eInvalidNofiy), mpCallback(NULL) {
    }
    Notification(ENotification what, NotifyCallback* pCallback)
        : mWhat(what), mpCallback(pCallback) {
    }
    ENotification mWhat;
    NotifyCallback* mpCallback;
  };

  Mutex mLock;
  DrmHwcTwo* mpHwc;

  sp<IDiagnostic> mpDiagnostic;

  std::vector<Notification> mNotifications;
};
}  // android

#endif  // OS_ANDROID_HWCSERVICE_H_
