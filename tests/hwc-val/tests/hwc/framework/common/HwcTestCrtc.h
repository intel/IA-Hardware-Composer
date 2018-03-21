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

#ifndef __HwcTestCrtc_h__
#define __HwcTestCrtc_h__

#include <stdint.h>
#include <vector>
#include <map>
#include <utils/KeyedVector.h>
#include "DrmShimBuffer.h"
#include "HwcTestConfig.h"
#include "HwcvalWatchdog.h"

#include <xf86drm.h>

// Define display Info structure
#include "hwcserviceapi.h"

class DrmShimPlane;
class HwcTestKernel;

namespace Hwcval {
class LogDisplayMapping;
class LayerList;
}

class HwcTestCrtc  {
 public:
  typedef std::vector<uint32_t> SeqVector;

  struct PowerState {
    bool mDPMS : 1;
    bool mDispScreenControl : 1;
    bool mBlack : 1;
    bool mHasContent : 1;
    bool mBlankingRequested : 1;
    bool mModeSet : 1;
    bool mVSyncEnabled : 1;
    bool mDPMSInProgress : 1;

    PowerState();
    const char* Report(char* strbuf, uint32_t len = HWCVAL_DEFAULT_STRLEN);
  };

  enum EsdRecoveryStateType {
    eEsdStarted,
    eEsdDpmsOff,
    eEsdModeSet,
    eEsdComplete,
    eEsdAny
  };

  typedef HwcsDisplayModeInfo Mode;
#define HWCVAL_MODE_FLAG_PREFERRED HWCS_MODE_FLAG_PREFERRED
  typedef std::vector<Mode> ModeVec;

  //-----------------------------------------------------------------------------
  // Constructor & Destructor
  HwcTestCrtc(uint32_t crtcId, uint32_t width, uint32_t height, uint32_t clock,
              uint32_t vrefresh);
  HwcTestCrtc(HwcTestCrtc& rhs);

  virtual ~HwcTestCrtc();
  virtual void StopThreads();

  /// Accessors
  uint32_t GetCrtcId();
  void SetCrtcId(uint32_t crtcId);

  void SetDisplayIx(uint32_t displayIx);
  uint32_t GetDisplayIx();
  uint32_t GetSfSrcDisplayIx();

  void ResetPlanes();
  void AddPlane(DrmShimPlane* plane);

  // Get plane, by zero-based INDEX, not plane id.
  DrmShimPlane* GetPlane(uint32_t planeIx);
  uint32_t NumPlanes();

  uint32_t GetWidth();
  uint32_t GetHeight();
  uint32_t GetClock();
  uint32_t GetVRefresh();

  void SetDimensions(uint32_t width, uint32_t height, uint32_t clock,
                     uint32_t vrefresh);
  void ResetOutDimensions();
  void SetOutDimensions(uint32_t width, uint32_t height);

  // These functions are telling the destination display which SF display the
  // layers come from
  // and how they will be cropped.
  void SetMosaicTransform(uint32_t srcDisp, double srcLeft, double srcTop,
                          double width, double height, double dstLeft,
                          double dstTop);

  // This function additionally supports scaling which is not currently a HWC
  // feature.
  void SetMosaicTransform(uint32_t srcDisp, double srcLeft, double srcTop,
                          double srcWidth, double srcHeight, double dstLeft,
                          double dstTop, double dstWidth, double dstHeight);

  // Set mosaic/passthrough display mapping
  void SetDisplayMapping(const Hwcval::LogDisplayMapping& mapping);

  uint32_t IncDrawCount();
  uint32_t GetDrawCount();
  void ResetDrawCount();

  void SetVideoLayerIndex(int layerIndex,
                          const hwcomposer::HwcRect<int>& rect = {0, 0, 0, 0});
  int GetVideoLayerIndex();

  // Current power state
  PowerState GetPower();

  // Record main plane disable - this is effectively an event, it only persists
  // for the
  // duration of the frame.
  // NOT to be confused with display enable/disable.
  HwcTestCrtc* SetMainPlaneDisabled(bool disabled);
  bool MainPlaneIsDisabled();

  HwcTestCrtc* SetZOrder(SeqVector* zOrder);
  SeqVector* GetZOrder();

  HwcTestCrtc* SetBlankingRequested(bool blank);
  bool IsBlankingRequested();
  bool WasBlankingRequested();

  HwcTestCrtc* SetDisplayEnable(bool enable);
  bool IsDisplayEnabled();

  HwcTestCrtc* SetDPMSEnabled(bool enable);
  bool IsDPMSEnabled();
  bool IsDispScreenControlEnabled();
  uint32_t PageFlipsSinceDPMS();
  void SetDPMSInProgress(bool inProgress);
  bool IsDPMSInProgress();

  HwcTestCrtc* SetSkipAllLayers(bool skipAllLayers);
  bool SkipAllLayers();

  void ResetActivePlaneCount();
  void IncActivePlaneCount();
  uint32_t GetActivePlaneCount();

  void GetDroppedFrameCounts(uint32_t& droppedFrameCount,
                             uint32_t& maxConsecutiveDroppedFrameCount,
                             bool clear);
  void SetDroppedFrame();
  bool IsDroppedFrame();

  // Call when some error has been detected on this display.
  // Returns true if the caller should continue to report the error, false if it
  // has been trapped at a higher level.
  bool ClassifyError(HwcTestCheckType& errorCode,
                     HwcTestCheckType normalErrorCode = eCheckDrmShimFail,
                     HwcTestCheckType cloneModeErrorCode = eCheckDrmShimFail);

  // Add to the tally of dropped frames
  void AddDroppedFrames(uint32_t count);

  // Add to dropped frames having first considered if the display was turned
  // off.
  void RecordDroppedFrames(uint32_t count);

  // Update the scores of consecutive and max consecutive dropped frames
  void UpdateDroppedFrameCounts(bool droppedFrame);

  // Force the consecutive stream to be broken so we start counting again
  void ResetConsecutiveDroppedFrames();

  // Clear the list of transforms showing what's been drawn, and associated
  // stuff.
  void ClearDrawnList();

  DrmShimTransform& GetCropTransform();
  DrmShimTransform& GetScaleTransform();

  void SetDrmFrame();
  void ConfirmNewFrame(uint32_t frame);
  bool IsFlickerDetected();
  uint32_t GetDrmStartFrame();
  uint32_t GetDrmEndFrame();

  void EnableVSync(bool enable);
  bool IsVSyncEnabled(bool updateActive = false);
  bool VBlankActive(bool active);
  bool WaitInactiveVBlank(uint32_t ms);

  void SetModeSet(bool enable);
  bool IsModeSet();

  int64_t GetVBlankTime(bool& enabled);
  void MarkVBlankCaptureTime();
  int64_t GetVBlankCaptureTime();

  void StopPageFlipWatchdog();

  void SetCurrentFrame(unsigned int frame);
  void IncCurrentFrame();
  uint32_t GetLastDisplayedFrame();

  void SetAllPlanesNotUpdated();

  void SetDisplayType(HwcTestState::DisplayType displayType);
  HwcTestState::DisplayType GetDisplayType();
  void SetRealDisplayType(HwcTestState::DisplayType displayType);
  HwcTestState::DisplayType GetRealDisplayType();
  bool IsHotPluggable();
  virtual bool SimulateHotPlug(bool connected);

  // Value of the hotplug spoof flag
  // So returns true if SF/the harness will see this as a connected display when
  // it is physically connected
  bool IsBehavingAsConnected();

  // Combined connected flag
  // Returns true if and only if the display is actually connected, and
  // logically connected from a hot plug
  // spoof point of view.
  bool IsConnected();

  // Is the display physically connected? Nothing to do with hot plug spoof.
  bool IsConnectedDisplay();

  bool IsExternalDisplay();
  bool IsMappedFromOtherDisplay();

  void SetBppChangePlane(DrmShimPlane* plane);
  DrmShimPlane* GetBppChangePlane();
  void ClearMaxFifo();
  bool HasLeftMaxFifo();

  bool CurrentFrameIsComplete();
  virtual void NotifyRetireFence(int retireFenceFd);

  void SkipValidateNextFrame();
  bool AmSkippingFrameValidation();

  // Is one of the planes using this buffer?
  bool IsUsing(std::shared_ptr<DrmShimBuffer> buf);

  // Failure of last attempt to set the display contents
  void SetDisplayFailed(bool failed);
  bool DidSetDisplayFail();
  bool IsTotalDisplayFail();


  // Panel Fitter
  void SetPanelFitter(uint32_t mode);
  bool SetPanelFitterSourceSize(uint32_t sourceWidth, uint32_t sourceHeight);
  uint32_t GetPanelFitterSourceWidth();
  uint32_t GetPanelFitterSourceHeight();
  bool IsPanelFitterEnabled();
  DrmShimTransform& GetPanelFitterTransform();

  // VSync
  void QueueCaptureVBlank(int fd, HwcTestEventHandler* vsyncRestorer);
  void ExecuteCaptureVBlank();

  // Esd Recovery
  bool EsdStateTransition(EsdRecoveryStateType from, EsdRecoveryStateType to);
  bool IsEsdRecoveryMode();
  void MarkEsdRecoveryStart();
  void EsdRecoveryEnd(const char* str = "took");

  // Mode control - override of the preferred mode
  void ClearUserMode();
  void SetUserModeStart();
  void SetUserModeFinish(int st, uint32_t width, uint32_t height,
                         uint32_t refresh, uint32_t flags, uint32_t ratio);
  void SetAvailableModes(const ModeVec& modes);
  void SetActualMode(const Mode& mode);
  void ValidateMode(HwcTestKernel* testKernel);
  bool RecentModeChange();
  void SetVideoRate(float videoRate);
  uint32_t GetVideoRate();
  virtual bool IsDRRSEnabled();
  bool MatchMode(uint32_t w, uint32_t h, uint32_t rate);

  // Checks
  void Checks(Hwcval::LayerList* ll, HwcTestKernel* testKernel,
              uint32_t hwcFrame);
  void FlickerClassify(DrmShimPlane* plane, std::shared_ptr<DrmShimBuffer>& buf);
  void DetectCloneOptimization(DrmShimPlane* plane,
                               std::shared_ptr<DrmShimBuffer>& buf);
  void FlickerChecks();
  void ExtendedModeChecks(HwcTestKernel* testKernel);
  void ConsistencyChecks(Hwcval::LayerList* ll, uint32_t hwcFrame);
  bool BlankingChecks(Hwcval::LayerList* ll, uint32_t hwcFrame);
  void WidiVideoConsistencyChecks(Hwcval::LayerList* ll);

  // Reporting
  void ReportPanelFitterStatistics(FILE* f);
  const char* ReportPower(char* strbuf, uint32_t len = HWCVAL_DEFAULT_STRLEN);
  void LogTransforms(int priority, uint32_t hwcFrame);
  void LogPlanes(int priority, const char* str);

 private:
  void CalculatePanelFitterTransform();
  void SetDisplayIsBlack(uint32_t numTransforms, uint32_t numLayers);

 protected:
  virtual void NotifyPageFlip() {
  }
  // Pointer from cached CRTC to real current CRTC
  // If we are the real current CRTC, will point to "this".
  HwcTestCrtc* mCurrentCrtc;

  // data members - Configuration
  // DRM CRTC Id / ADF Interface Id
  uint32_t mCrtcId;

  // Display index
  uint32_t mDisplayIx;
  // Source display index when mosaic displays are in use
  uint32_t mSfSrcDisp;

  // Display size as seen by SurfaceFlinger
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mClock;
  uint32_t mVRefresh;

  // Actual display size that HWC scales to
  uint32_t mOutWidth;
  uint32_t mOutHeight;

  // All the planes for the CRTC, indexed by planeId
  std::map<uint32_t, DrmShimPlane*> mPlanes;

  // State
  uint32_t mDrawCount;

  // Main plane disabled
  bool mMainPlaneDisabled;

  // Display Power Management Something Enabled
  uint32_t mPageFlipsSinceDPMS;

  // Clone mode detected
  bool mCloneOptimization;

  // SF has requested to skip all layers - should mean rotation
  bool mSkipAllLayers;

  // Z-order Sequence currently defined for all planes
  SeqVector* mZOrder;

  // Blanking requested by OnBlank
  int64_t mUnblankingTime;

  // Number of frames flipped since mode set
  uint32_t mFramesSinceModeSet;

  // Power states
  PowerState mPower;
  PowerState mPowerLastFlip;
  PowerState mPowerSinceLastUnblankingCheck;

  // VBlank capture active
  volatile int32_t mVBlankActive;
  int64_t mVBlankCaptureTime;

  EsdRecoveryStateType mEsdState;

  // Override of real connection state
  bool mSimulatedHotPlugConnectionState;

  // Last set display failed
  bool mSetDisplayFailed;

  // Dropped frame counting
  bool mDroppedFrame;
  uint32_t mConsecutiveDroppedFrameCount;
  uint32_t mMaxConsecutiveDroppedFrameCount;
  uint32_t mDroppedFrameCount;

  // Active plane counting
  uint32_t mActivePlaneCount;

  // Sorted list of transforms mapped to the CRTC
  DrmShimSortedTransformVector mTransforms;

  // Crop transform that you get by putting something on the screen
  DrmShimTransform mCropTransform;

  // Transform for global scaling to the output display
  DrmShimTransform mScaleTransform;

  // Frame number on last VBlank
  uint32_t mFrame;

  // Flicker detection
  uint32_t mDrmStartFrame;
  uint32_t mDrmEndFrame;
  DrmShimPlane* mBppChangePlane;
  bool mMaxFifo;
  bool mWasMaxFifo;

  // Display type (after spoofing)
  HwcTestState::DisplayType mDisplayType;
  // Physical display type
  HwcTestState::DisplayType mRealDisplayType;

  // Counter for this CRTC
  uint32_t mValidatedFrameCount;

  // Frame sequence of last frame validated
  uint32_t mLastDisplayedFrame;

  // Panel Fitter
  uint32_t mPanelFitterMode;
  uint32_t mPanelFitterSourceWidth;
  uint32_t mPanelFitterSourceHeight;
  DrmShimTransform mPanelFitterTransform;

  // Number of times frames used each of the panel fitter modes
  uint32_t mPanelFitterModeCount[4];

  // Transparency filter detection
  int mVideoLayerIndex;
  hwcomposer::HwcRect<int> mVideoDisplayFrame;

  // Skip validation of next frame owing to ADF errors
  bool mSkipValidateNextFrame;

  // Vsync restoration following resume
  HwcTestEventHandler* mQueuedVSyncRequest;
  int mQueuedVSyncFd;

  // VSync and page flip timing
  Hwcval::Watchdog mVBlankWatchdog;
  Hwcval::Watchdog mPageFlipWatchdog;
  int64_t mPageFlipTime;
  uint32_t mSetDisplayCount;

  // Check for lockup in drmModeSetDisplay
  Hwcval::Watchdog mSetDisplayWatchdog;

  // Check for lockup in set DPMS
  Hwcval::Watchdog mDPMSWatchdog;

  // ESD Recovery
  int64_t mEsdRecoveryStartTime;

  // Counts to establish total DRM fail
  uint32_t mSetDisplayFailCount;
  uint32_t mSetDisplayPassCount;

  // User mode selection
  enum UserModeStateType {
    eUserModeUndefined,
    eUserModeNotSet,
    eUserModeChanging,
    eUserModeSet
  };
  UserModeStateType mUserModeState;

  // Mode requested by HWC service call
  Mode mUserMode;

  // Modes the connected display actually allows
  ModeVec mAvailableModes;

  // The mode that was actually set to the display.
  Mode mActualMode;

  // The mode that we actually want set
  Mode mRequiredMode;

  // The preferred mode from the list of availables.
  // (we can only cope with one).
  Mode mPreferredMode;
  uint32_t mPreferredModeCount;
  uint32_t mFramesSinceRequiredModeChange;

  // DRRS enabled by property (or spoofing)
  bool mDRRS;

  // Max permitted latency from unblank request to 1st real content on screen
  int64_t mMaxUnblankingLatency;

  // Effective refresh rate for extended mode / DRRS validation
  float mVideoRate;
};

/// Accessors
inline uint32_t HwcTestCrtc::GetCrtcId() {
  return mCrtcId;
}

inline void HwcTestCrtc::SetCrtcId(uint32_t crtcId) {
  ALOG_ASSERT((mCrtcId == 0) || (mCrtcId == crtcId));
  mCrtcId = crtcId;
}

inline void HwcTestCrtc::SetDisplayIx(uint32_t displayIx) {
  mDisplayIx = displayIx;
  mSfSrcDisp = displayIx;
}

inline uint32_t HwcTestCrtc::GetDisplayIx() {
  return mDisplayIx;
}

inline uint32_t HwcTestCrtc::GetSfSrcDisplayIx() {
  return mSfSrcDisp;
}

inline bool HwcTestCrtc::IsConnectedDisplay() {
  return (mDisplayIx != eNoDisplayIx);
}

// Get plane, by zero-based INDEX, not plane id.
inline DrmShimPlane* HwcTestCrtc::GetPlane(uint32_t planeIx) {
  if (planeIx < mPlanes.size()) {
    return mPlanes[planeIx];
  } else {
    return 0;
  }
}

inline uint32_t HwcTestCrtc::GetWidth() {
  return mWidth;
}

inline uint32_t HwcTestCrtc::GetHeight() {
  return mHeight;
}

inline uint32_t HwcTestCrtc::GetVRefresh() {
  return mVRefresh;
}

inline uint32_t HwcTestCrtc::GetClock() {
  return mClock;
}

inline HwcTestCrtc::PowerState HwcTestCrtc::GetPower() {
  return mPower;
}

inline uint32_t HwcTestCrtc::IncDrawCount() {
  ++mDrawCount;
  return mDrawCount;
}

inline uint32_t HwcTestCrtc::GetDrawCount() {
  return mDrawCount;
}

inline void HwcTestCrtc::ResetDrawCount() {
  mDrawCount = 0;
}

inline void HwcTestCrtc::SetVideoLayerIndex(
    int layerIndex, const hwcomposer::HwcRect<int>& rect) {
  mVideoLayerIndex = layerIndex;
  mVideoDisplayFrame = rect;
}

inline int HwcTestCrtc::GetVideoLayerIndex() {
  return mVideoLayerIndex;
}

inline HwcTestCrtc* HwcTestCrtc::SetMainPlaneDisabled(bool disabled) {
  mMainPlaneDisabled = disabled;
  return this;
}

inline bool HwcTestCrtc::MainPlaneIsDisabled() {
  return mMainPlaneDisabled;
}

inline HwcTestCrtc* HwcTestCrtc::SetZOrder(HwcTestCrtc::SeqVector* zOrder) {
  mZOrder = zOrder;
  return this;
}

inline HwcTestCrtc::SeqVector* HwcTestCrtc::GetZOrder() {
  return mZOrder;
}

inline bool HwcTestCrtc::IsBlankingRequested() {
  return mPower.mBlankingRequested;
}

inline bool HwcTestCrtc::WasBlankingRequested() {
  return mPowerLastFlip.mBlankingRequested;
}

inline HwcTestCrtc* HwcTestCrtc::SetDisplayEnable(bool enable) {
  mPower.mDispScreenControl = enable;
  return this;
}

// Display is disabled if either disp screen control or DPMS is disabled
inline bool HwcTestCrtc::IsDisplayEnabled() {
  return mPower.mDispScreenControl && mPower.mDPMS;
}

inline bool HwcTestCrtc::IsDPMSEnabled() {
  return mPower.mDPMS;
}

inline bool HwcTestCrtc::IsDispScreenControlEnabled() {
  return mPower.mDispScreenControl;
}

inline uint32_t HwcTestCrtc::PageFlipsSinceDPMS() {
  return ++(mCurrentCrtc->mPageFlipsSinceDPMS);
}

inline HwcTestCrtc* HwcTestCrtc::SetSkipAllLayers(bool skipAllLayers) {
  mSkipAllLayers = skipAllLayers;
  return this;
}

inline bool HwcTestCrtc::SkipAllLayers() {
  return mSkipAllLayers;
}

inline void HwcTestCrtc::ResetActivePlaneCount() {
  mActivePlaneCount = 0;
}

inline void HwcTestCrtc::IncActivePlaneCount() {
  ++mActivePlaneCount;
}

inline uint32_t HwcTestCrtc::GetActivePlaneCount() {
  return mActivePlaneCount;
}

inline bool HwcTestCrtc::IsDroppedFrame() {
  return (mDroppedFrame);
}

inline void HwcTestCrtc::SetDroppedFrame() {
  mCurrentCrtc->mDroppedFrame = true;
  mDroppedFrame = true;
}

inline DrmShimTransform& HwcTestCrtc::GetCropTransform() {
  return mCropTransform;
}

inline DrmShimTransform& HwcTestCrtc::GetScaleTransform() {
  return mScaleTransform;
}

inline void HwcTestCrtc::SetBppChangePlane(DrmShimPlane* plane) {
  mBppChangePlane = plane;
}

inline DrmShimPlane* HwcTestCrtc::GetBppChangePlane() {
  return mBppChangePlane;
}

inline void HwcTestCrtc::ClearMaxFifo() {
  mMaxFifo = false;
}

inline bool HwcTestCrtc::HasLeftMaxFifo() {
  return mWasMaxFifo && !mMaxFifo;
}

inline void HwcTestCrtc::EnableVSync(bool enable) {
  if (enable && !mPower.mVSyncEnabled) {
    MarkVBlankCaptureTime();
  }

  mPower.mVSyncEnabled = enable;
}

inline bool HwcTestCrtc::IsVSyncEnabled(bool updateActive) {
  if (updateActive) {
    mVBlankActive = (int32_t)mPower.mVSyncEnabled;

    if (mVBlankActive) {
      mVBlankWatchdog.Start();
    }
  }

  return mPower.mVSyncEnabled;
}

inline int64_t HwcTestCrtc::GetVBlankTime(bool& enabled) {
  enabled = mPower.mVSyncEnabled && mPower.mDPMS;
  return mVBlankWatchdog.GetStartTime();
}

inline void HwcTestCrtc::MarkVBlankCaptureTime() {
  mVBlankCaptureTime = systemTime(SYSTEM_TIME_MONOTONIC);
  mVBlankWatchdog.Start();
}

inline int64_t HwcTestCrtc::GetVBlankCaptureTime() {
  if (mPower.mVSyncEnabled && mPower.mDPMS) {
    return mVBlankCaptureTime;
  } else {
    return 0;
  }
}

inline bool HwcTestCrtc::EsdStateTransition(EsdRecoveryStateType from,
                                            EsdRecoveryStateType to) {
  if (from == eEsdAny || from == mEsdState) {
    mEsdState = to;
    return true;
  } else {
    return false;
  }
}

inline bool HwcTestCrtc::IsEsdRecoveryMode() {
  return (mEsdState != eEsdComplete);
}

inline void HwcTestCrtc::SetModeSet(bool enable) {
  mPower.mModeSet = enable;

  if (enable) {
    mFramesSinceModeSet = 0;
  }
}

inline bool HwcTestCrtc::IsModeSet() {
  return mPower.mModeSet;
}

inline void HwcTestCrtc::SetDisplayType(HwcTestState::DisplayType displayType) {
  HWCLOGD_COND(eLogDrm, "HwcTestCrtc::SetDisplayType Crtc=%d %s", mCrtcId,
               HwcTestState::DisplayTypeStr(displayType));

  mDisplayType = displayType;
}

inline HwcTestState::DisplayType HwcTestCrtc::GetDisplayType() {
  return mDisplayType;
}

inline void HwcTestCrtc::SetRealDisplayType(
    HwcTestState::DisplayType displayType) {
  HWCLOGD_COND(eLogDrm, "HwcTestCrtc::SetRealDisplayType Crtc=%d %s", mCrtcId,
               HwcTestState::DisplayTypeStr(displayType));

  mRealDisplayType = displayType;
}

inline HwcTestState::DisplayType HwcTestCrtc::GetRealDisplayType() {
  return mRealDisplayType;
}

inline bool HwcTestCrtc::IsHotPluggable() {
  return (mDisplayType == HwcTestState::eRemovable);
}

inline uint32_t HwcTestCrtc::GetLastDisplayedFrame() {
  return mLastDisplayedFrame;
}

inline bool HwcTestCrtc::IsDPMSInProgress() {
  return mPower.mDPMSInProgress;
}

inline void HwcTestCrtc::SetVideoRate(float videoRate) {
  mVideoRate = videoRate;
}

inline bool HwcTestCrtc::IsExternalDisplay() {
  return (mSfSrcDisp > 0);
}

inline bool HwcTestCrtc::IsMappedFromOtherDisplay() {
  return (mSfSrcDisp != mDisplayIx);
}

#endif  // __HwcTestCrtc_h__
