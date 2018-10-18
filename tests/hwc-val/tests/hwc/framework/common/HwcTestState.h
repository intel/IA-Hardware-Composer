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

#ifndef __HwcTestState_h__
#define __HwcTestState_h__

#include <dlfcn.h>
#include <cutils/log.h>
#include <assert.h>

#include "intel_bufmgr.h"

extern "C" {
#include "hardware/hardware.h"
#include "hardware/hwcomposer2.h"
#include <hardware/gralloc.h>
}

#include <utils/Vector.h>
#include <utils/SystemClock.h>
#include <binder/IServiceManager.h>
#include <utils/Condition.h>

#include "HwcvalDebug.h"
#include "HwcTestDefs.h"
#include "HwcTestConfig.h"
#include "HwcTestLog.h"
#include "HwcTestDisplaySpoof.h"
#include "HwcvalStall.h"
#include "public/nativebufferhandler.h"

class HwcShimService;
class HwcShimInitializer;
class HwcTestKernel;
class DrmShimChecks;
class HwcShim;
class HwcServiceShim;

namespace Hwcval {
class MultiDisplayInfoProviderShim;
class Selector;
}

class HwcTestEventHandler {
 public:
  virtual ~HwcTestEventHandler() {
  }

  virtual void CaptureVBlank(int fd, uint32_t crtcId) = 0;

  virtual void Restore(uint32_t crtcId) = 0;

  virtual void CancelEvent(uint32_t crtcId) = 0;
};

class HwcTestState {
 public:
  // Device attribute query
  enum DeviceType {
    eDeviceTypeBYT = 0,
    eDeviceTypeCHT,
    eDeviceTypeBXT,
    eDeviceTypeUnknown
  };

  enum DisplayType { eFixed = 1, eRemovable = 2, eVirtual = 4 };

  // Display attribute query
  enum DisplayPropertyType { ePropNone = 0, ePropConnectorId };

  enum ShimMaskType { eHwcShim = 1, eDrmShim = 2, eMdsShim = 8 };

  // Typedef for HWC Log function pointer
  typedef void (*HwcLogAddPtr)(const char* fmt, ...);

  // Typedef for hotplug simulate function
  typedef void (*HwcSimulateHotPlugPtr)(bool connected);

 protected:
  /// Get display properties
  void SetupDisplayProperties(void);
  /// TODO
  void SetupCheckFunctions(uint32_t platform);

// Check functions set error of the shim internally and so do not return a
// error code.

  /// pointer to checker for the DRM shim
  DrmShimChecks* mDrmChecks;
  HwcTestKernel* mTestKernel;

 public:
  HwcTestState();
  ~HwcTestState();

  /// get singular instance
  static HwcTestState* getInstance();

  /// Initialize
  typedef void (*HwcShimInitFunc)();
  void CreateTestKernel();
  int LoggingInit(void* libHwcHandle);
  int TestStateInit(HwcShimInitializer* hwcShimInitializer);
  void RegisterWithHwc();
  void SetPreferences();

  /// Part of the closedown procedure for the harness
  void StopThreads();

  /// Static function called on image exit
  /// also can be called by harness to perform tidyup
  static void rundown(void);

 private:
  /// Static pointer to singular object
  static HwcTestState* mInstance;

  // For hooks into real HWC
  void* mLibHwcHandle;

  HwcShimInitializer* mHwcShimInitializer;

  // true if we are validating live state
  bool mLive;

  /// Test configuration
  HwcTestConfig mConfig;

  /// Test results
  HwcTestResult mResult;

  /// Which shims are actually running (self-registered)
  uint32_t mRunningShims;

  /// Pointer to HWC add-to-log function
  HwcLogAddPtr mpHwcLogAdd;

  /// Pointer to HWC simulate hotplug function
  HwcSimulateHotPlugPtr mpHwcSimulateHotPlug;

  /// Number of hotplugs in progress (should only ever be 0 or 1)
  uint32_t mHotPlugInProgress;

  /// Default hotplug simulation state for new displays as HWC polls them for
  /// the first time
  bool mNewDisplayConnectionState;

  // used by the frame control class to signal to the binder when it is enabled
  bool mFrameControlEnabled;

  /// Lowest display index of an enabled display
  uint32_t mFirstDisplayWithVSync;

  // Display spoof control
  HwcTestNullDisplaySpoof mDefaultDisplaySpoof;
  HwcTestDisplaySpoof* mDisplaySpoof;

  // DrmShimEventHandler restorer for vsync events
  HwcTestEventHandler* mVSyncRestorer;

  // Allow harness to synchronize with (real) OnSet
  Hwcval::Condition mOnSetCondition;
  Hwcval::Mutex mOnSetMutex;
  bool mOnSetConditionEnable;

  // Power suspend/resume mode
  bool mSuspend;

  // MDS
  Hwcval::MultiDisplayInfoProviderShim* mMDSInfoProviderShim;

  // Target device type
  DeviceType mDeviceType;

  // Stalls
  Hwcval::Stall mStall[Hwcval::eStallMax];

  // Input frame to dump all buffers
  uint32_t mInputFrameToDump;

  // Dumping of input images
  std::shared_ptr<Hwcval::Selector> mFrameDumpSelector;
  uint32_t mMaxDumpImages;
  uint32_t mNumDumpImage;

  std::shared_ptr<Hwcval::Selector> mTgtFrameDumpSelector;
  // Max latency allowed to unblank the display
  int64_t mMaxUnblankingLatency;

  // Transparent layer notified from harness
  HWCNativeHandle mFutureTransparentLayer;

 public:
  // Accessors to get and set the device type (BYT/CHT/BXT)
  DeviceType GetDeviceType();
  void SetDeviceType(DeviceType device);

  /// Read/write access to test configuration
  void SetTestConfig(const HwcTestConfig& config);
  HwcTestConfig& GetTestConfig();

  // Are we validating live state (as opposed to a historic input file)
  bool IsLive();

  /// Get test result
  HwcTestResult& GetTestResult(void);

  /// Mark shim failure reason
  void SetShimFail(HwcTestCheckType feature);

  /// accessor for DRM checks object
  HwcTestKernel* GetTestKernel();

  /// access for HWC logger function
  HwcLogAddPtr GetHwcLogFunc();

  /// access for minimum Android log level
  int GetMinLogPriority();

  /// Reset results at start of the test
  void ResetTestResults();

  /// Send frame counts through to HwcTestResult
  void ReportFrameCounts(bool final = true);

  bool IsLoggingEnabled(int priority, HwcTestCheckType check);
  bool IsLoggingEnabled(int priority);
  bool IsCheckEnabled(HwcTestCheckType check);
  bool IsOptionEnabled(HwcTestCheckType check);
  int GetHwcOptionInt(const char* optionName);
  const char* GetHwcOptionStr(const char* optionName);
  bool IsBufferMonitorEnabled();
  bool IsAutoExtMode();
  unsigned GetSetDisplayCRCCheckDelay(unsigned batchDelay = unsigned(-1));
  bool ConfigRequiresFrameControl(const HwcTestConfig* pConfig = NULL) const;
  void SetFrameControlEnabled(bool);
  bool IsFrameControlEnabled() const;

  // Access HWC frame number
  uint32_t GetHwcFrame(uint32_t displayIx);

  // Power suspend/resume mode
  void SetSuspend(bool suspend);
  bool IsSuspended();

  void SetFirstDisplayWithVSync(uint32_t disp);
  uint32_t GetFirstDisplayWithVSync();
  int64_t GetVBlankTime(uint32_t displayIx, bool& enabled);

  // Wait for composition validation to complete
  // Allows test to ensure that all the important compositions get validated.
  void WaitForCompValToComplete();

  // Display property query
  uint32_t GetDisplayProperty(uint32_t displayIx,
                              HwcTestState::DisplayPropertyType prop);

  // Override preferred HDMI mode
  void SetHdmiPreferredMode(uint32_t width = 0, uint32_t height = 0,
                            uint32_t refresh = 0);

  // Direct test kernel to simulate hot plug
  bool IsHotPluggableDisplayAvailable();
  bool SimulateHotPlug(bool connected, uint32_t displayTypes = 0xff);
  bool GetNewDisplayConnectionState();

  // Display is deemed to be in a bad state, we need to reboot
  bool IsTotalDisplayFail();

  // Harness can call to avoid our work queue overflowing
  void ProcessWork();

  // Set up DRM/ADF failure spoofing
  void SetDisplaySpoof(HwcTestDisplaySpoof* displaySpoof);
  HwcTestDisplaySpoof& GetDisplaySpoof();

  // Vsync event restoration
  void SetVSyncRestorer(HwcTestEventHandler* restorer);
  void RestoreVSync(uint32_t disp);

  // Print out panel fitter statistics
  void ReportPanelFitterStatistics(FILE* f);

  // Fence validity checking.
  // If we are not live, these always return true.
  bool IsFenceValid(int fence, uint32_t disp, uint32_t hwcFrame,
                    bool checkSignalled = false, bool checkUnsignalled = false);
  bool IsFenceSignalled(int fence, uint32_t disp, uint32_t hwcFrame);
  bool IsFenceUnsignalled(int fence, uint32_t disp, uint32_t hwcFrame);

  // OnSet synchronization
  void TriggerOnSetCondition();
  void WaitOnSetCondition();

  // ESD Recovery
  void MarkEsdRecoveryStart(uint32_t connectorId);

  // MDS
  void SetMDSInfoProviderShim(Hwcval::MultiDisplayInfoProviderShim* shim);
  Hwcval::MultiDisplayInfoProviderShim* GetMDSInfoProviderShim();

  // Forced stalls for stability testing
  void SetStall(Hwcval::StallType ix, const Hwcval::Stall& stall);
  Hwcval::Stall& GetStall(Hwcval::StallType ix);

  // Configure image dump
  void ConfigureImageDump(std::shared_ptr<Hwcval::Selector> selector,
                          uint32_t maxDumpImages);
  void ConfigureTgtImageDump(std::shared_ptr<Hwcval::Selector> selector);
  uint32_t TestImageDump(uint32_t hwcFrame);
  bool TestTgtImageDump(uint32_t hwcFrame);

  // So we can confirm if all the shims we expect are running
  void SetRunningShim(ShimMaskType shim);
  void CheckRunningShims(uint32_t mask);

  // Log to Kmsg (useful when DRM debug is enabled)
  void LogToKmsg(const char* format, ...);

  // Is a hot plug in progress?
  bool HotPlugInProgress();

  // Notification from harness that a layer in the next OnSet will be
  // transparent
  void SetFutureTransparentLayer(HWCNativeHandle handle);
  HWCNativeHandle GetFutureTransparentLayer();

  // Tell the validation the rate we are (trying to) produce video at.
  // This can easily be broken by loading the system heavily or simply by
  // running on a slow device
  // So will need to be careful on which tests these checks are enabled.
  void SetVideoRate(uint32_t disp, float videoRate);

  // Stringification
  static const char* DisplayTypeStr(uint32_t displayType);
  void SetMaxUnblankingLatency(int64_t ns);
  int64_t GetMaxUnblankingLatency();
};

inline void HwcTestState::SetDeviceType(DeviceType device) {
  HWCLOGV("HwcTestState::SetDeviceType: setting device type to: %s",
          device == eDeviceTypeBYT ? "Baytrail" : device == eDeviceTypeCHT
                                                      ? "Cherrytrail"
                                                      : device == eDeviceTypeBXT
                                                            ? "Broxton"
                                                            : "Unknown");

  mDeviceType = device;
}

inline HwcTestState::DeviceType HwcTestState::GetDeviceType() {
  return (mDeviceType);
}

inline bool HwcTestState::IsLive() {
  return mLive;
}

inline void HwcTestState::SetTestConfig(const HwcTestConfig& config) {
  HWCLOGD("HwcTestState::SetTestConfig");
  mConfig = config;
}

inline void HwcTestState::SetShimFail(HwcTestCheckType feature) {
  mResult.SetFail(feature);
}

inline HwcTestKernel* HwcTestState::GetTestKernel() {
  return mTestKernel;
}

inline HwcTestState::HwcLogAddPtr HwcTestState::GetHwcLogFunc() {
  return mpHwcLogAdd;
}

inline int HwcTestState::GetMinLogPriority() {
  return mConfig.mMinLogPriority;
}

inline bool HwcTestState::IsLoggingEnabled(int priority,
                                           HwcTestCheckType check) {
  return (mConfig.IsLevelEnabled(priority) &&
          (mConfig.mCheckConfigs[check].enable) && mConfig.mGlobalEnable);
}

inline bool HwcTestState::IsLoggingEnabled(int priority) {
  return (mConfig.IsLevelEnabled(priority));
}

inline bool HwcTestState::IsCheckEnabled(HwcTestCheckType check) {
  return (mConfig.mCheckConfigs[check].enable && mConfig.mGlobalEnable);
}

inline bool HwcTestState::IsOptionEnabled(HwcTestCheckType check) {
  return (mConfig.mCheckConfigs[check].enable);
}

inline bool HwcTestState::IsBufferMonitorEnabled() {
  return mConfig.mBufferMonitorEnable;
}

inline void HwcTestState::SetSuspend(bool suspend) {
  mSuspend = suspend;
}

inline bool HwcTestState::IsSuspended() {
  return mSuspend;
}

inline HwcTestConfig* HwcGetTestConfig() {
  return &(HwcTestState::getInstance()->GetTestConfig());
}

inline HwcTestResult* HwcGetTestResult() {
  return &(HwcTestState::getInstance()->GetTestResult());
}

inline unsigned HwcTestState::GetSetDisplayCRCCheckDelay(unsigned batchDelay) {
  if (batchDelay != unsigned(-1)) {
    mConfig.mDisplayCRCCheckDelay = batchDelay;
  }
  return mConfig.mDisplayCRCCheckDelay;
}

inline bool HwcTestState::ConfigRequiresFrameControl(
    const HwcTestConfig* pConfig) const {
  const HwcTestConfig* pCfg = pConfig ? pConfig : &mConfig;

  return pCfg->mGlobalEnable && pCfg->mCheckConfigs[eCheckCRC].enable;
}

inline void HwcTestState::SetFrameControlEnabled(bool enabled) {
  mFrameControlEnabled = enabled;
}

inline bool HwcTestState::IsFrameControlEnabled() const {
  return mFrameControlEnabled;
}

inline void HwcTestState::SetFirstDisplayWithVSync(uint32_t disp) {
  HWCLOGD("First display with VSync=%d", disp);
  mFirstDisplayWithVSync = disp;
}

inline uint32_t HwcTestState::GetFirstDisplayWithVSync() {
  return mFirstDisplayWithVSync;
}

inline void HwcTestState::SetDisplaySpoof(HwcTestDisplaySpoof* displaySpoof) {
  if (displaySpoof) {
    mDisplaySpoof = displaySpoof;
  } else {
    mDisplaySpoof = &mDefaultDisplaySpoof;
  }
}

inline HwcTestDisplaySpoof& HwcTestState::GetDisplaySpoof() {
  return *mDisplaySpoof;
}

inline void HwcTestState::SetVSyncRestorer(HwcTestEventHandler* restorer) {
  // If allowed, setup the callback to restore VSync handling on VSync timeout
  if (mConfig.mCheckConfigs[eOptAutoRestoreVSync].enable) {
    mVSyncRestorer = restorer;
  }
}

inline void HwcTestState::RestoreVSync(uint32_t disp) {
  if (mVSyncRestorer) {
    mVSyncRestorer->Restore(disp);
  }
}

inline void HwcTestState::SetRunningShim(ShimMaskType shim) {
  mRunningShims |= (uint32_t)shim;
}

inline bool HwcTestState::HotPlugInProgress() {
  return (mHotPlugInProgress != 0);
}

inline void HwcTestState::SetMaxUnblankingLatency(int64_t ns) {
  mMaxUnblankingLatency = ns;
}

inline int64_t HwcTestState::GetMaxUnblankingLatency() {
  return mMaxUnblankingLatency;
}

inline bool HwcTestState::GetNewDisplayConnectionState() {
  return mNewDisplayConnectionState;
}

#endif  // __HwcTestState_h__
