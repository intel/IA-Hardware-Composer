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

#include <cutils/properties.h>

#include "HwcTestDefs.h"
#include "HwcTestCrtc.h"
#include "DrmShimChecks.h"
#include "HwcTestUtil.h"
#include "HwcTestState.h"
#include "HwcvalContent.h"
#include "HwcvalLogDisplay.h"

using namespace Hwcval;

HwcTestCrtc::PowerState::PowerState()
    : mDPMS(true),
      mDispScreenControl(true),
      mBlack(false),
      mHasContent(false),
      mBlankingRequested(false),
      mModeSet(false),
      mVSyncEnabled(false),
      mDPMSInProgress(false) {
}

const char* HwcTestCrtc::PowerState::Report(char* strbuf, uint32_t len) {
  snprintf(strbuf, len, "DPMS:%d DispScreenControl:%d BlankingReq:%d", mDPMS,
           mDispScreenControl, mBlankingRequested);
  return strbuf;
}

HwcTestCrtc::HwcTestCrtc(uint32_t crtcId, uint32_t width, uint32_t height,
                         uint32_t clock, uint32_t vrefresh)
    : mCurrentCrtc(this),
      mCrtcId(crtcId),
      mDisplayIx(eNoDisplayIx),
      mSfSrcDisp(eNoDisplayIx),
      mWidth(width),
      mHeight(height),
      mClock(clock),
      mVRefresh(vrefresh),
      mOutWidth(width),
      mOutHeight(height),
      mDrawCount(0),
      mMainPlaneDisabled(false),
      mPageFlipsSinceDPMS(0),
      mCloneOptimization(false),
      mSkipAllLayers(false),
      mZOrder(0),
      mUnblankingTime(0),
      mVBlankActive(false),
      mVBlankCaptureTime(0),
      mEsdState(HwcTestCrtc::eEsdComplete),
      mSimulatedHotPlugConnectionState(true),
      mSetDisplayFailed(false),
      mDroppedFrame(false),
      mConsecutiveDroppedFrameCount(0),
      mMaxConsecutiveDroppedFrameCount(0),
      mDroppedFrameCount(0),
      mActivePlaneCount(0),
      mFrame(0),
      mDrmStartFrame(0),
      mDrmEndFrame(0),
      mBppChangePlane(0),
      mMaxFifo(true),
      mDisplayType(HwcTestState::eFixed),
      mValidatedFrameCount(0),
      mLastDisplayedFrame(0)
#ifdef DRM_PFIT_OFF
      ,
      mPanelFitterMode(DRM_PFIT_OFF),
      mPanelFitterSourceWidth(0),
      mPanelFitterSourceHeight(0)
#endif
      ,
      mVideoLayerIndex(-1),
      mVideoDisplayFrame({0, 0, 0, 0}),
      mSkipValidateNextFrame(false),
      mVBlankWatchdog(50 * HWCVAL_MS_TO_NS, eCheckDispGeneratesVSync),
      mPageFlipWatchdog(50 * HWCVAL_MS_TO_NS, eCheckTimelyPageFlip),
      mPageFlipTime(0),
      mSetDisplayCount(0),
      mSetDisplayWatchdog(15 * HWCVAL_SEC_TO_NS, eCheckDrmSetDisplayLockup),
      mDPMSWatchdog(15 * HWCVAL_SEC_TO_NS, eCheckDPMSLockup),
      mEsdRecoveryStartTime(0),
      mSetDisplayFailCount(0),
      mSetDisplayPassCount(0),
      mUserModeState(eUserModeNotSet),
      mFramesSinceRequiredModeChange(HWCVAL_EXTENDED_MODE_CHANGE_WINDOW),
      mDRRS(false),
      mMaxUnblankingLatency(HWCVAL_MAX_UNBLANKING_LATENCY_DEFAULT_US *
                            HWCVAL_US_TO_NS),
      mVideoRate(0.0) {
  memset(mPanelFitterModeCount, 0, sizeof(mPanelFitterModeCount));

  mVBlankWatchdog.SetMessage(std::string("VBlank watchdog Crtc ") + std::to_string(mCrtcId));
  mPageFlipWatchdog.SetMessage(std::string("Page flip watchdog Crtc ") + std::to_string(mCrtcId));
  mSetDisplayWatchdog.SetMessage(std::string("Set Display watchdog Crtc ") + std::to_string(mCrtcId));
  mDPMSWatchdog.SetMessage(std::string("DPMS watchdog Crtc ") + std::to_string(mCrtcId));

  mMaxUnblankingLatency =
      HwcTestState::getInstance()->GetMaxUnblankingLatency();
}

HwcTestCrtc::HwcTestCrtc(HwcTestCrtc& rhs)
    : mCurrentCrtc(&rhs),
      mCrtcId(rhs.mCrtcId),
      mDisplayIx(rhs.mDisplayIx),
      mSfSrcDisp(rhs.mSfSrcDisp),
      mWidth(rhs.mWidth),
      mHeight(rhs.mHeight),
      mClock(rhs.mClock),
      mVRefresh(rhs.mVRefresh),
      mOutWidth(rhs.mOutWidth),
      mOutHeight(rhs.mOutHeight),
      mDrawCount(rhs.mDrawCount),
      mMainPlaneDisabled(rhs.mMainPlaneDisabled),
      mPageFlipsSinceDPMS(rhs.mPageFlipsSinceDPMS),
      mCloneOptimization(rhs.mCloneOptimization),
      mSkipAllLayers(rhs.mSkipAllLayers),
      mZOrder(rhs.mZOrder),
      mUnblankingTime(rhs.mUnblankingTime),
      mPower(rhs.mPower),
      mPowerLastFlip(rhs.mPowerLastFlip),
      mPowerSinceLastUnblankingCheck(rhs.mPowerSinceLastUnblankingCheck),
      mVBlankActive(rhs.mVBlankActive),
      mVBlankCaptureTime(rhs.mVBlankCaptureTime),
      mEsdState(rhs.mEsdState),
      mSimulatedHotPlugConnectionState(rhs.mSimulatedHotPlugConnectionState),
      mSetDisplayFailed(rhs.mSetDisplayFailed),
      mDroppedFrame(rhs.mDroppedFrame),
      mConsecutiveDroppedFrameCount(rhs.mConsecutiveDroppedFrameCount),
      mMaxConsecutiveDroppedFrameCount(rhs.mMaxConsecutiveDroppedFrameCount),
      mDroppedFrameCount(rhs.mDroppedFrameCount),
      mActivePlaneCount(rhs.mActivePlaneCount),
      mCropTransform(rhs.mCropTransform),
      mScaleTransform(rhs.mScaleTransform),
      mFrame(rhs.mFrame),
      mDrmStartFrame(rhs.mDrmStartFrame),
      mDrmEndFrame(rhs.mDrmEndFrame),
      mBppChangePlane(rhs.mBppChangePlane),
      mMaxFifo(rhs.mMaxFifo),
      mDisplayType(rhs.mDisplayType),
      mValidatedFrameCount(rhs.mValidatedFrameCount),
      mLastDisplayedFrame(rhs.mLastDisplayedFrame)
#ifdef DRM_PFIT_OFF
      ,
      mPanelFitterMode(rhs.mPanelFitterMode),
      mPanelFitterSourceWidth(rhs.mPanelFitterSourceWidth),
      mPanelFitterSourceHeight(rhs.mPanelFitterSourceHeight),
      mPanelFitterTransform(rhs.mPanelFitterTransform)
// mPanelFitterModeCount is NOT maintained in the copy - must be in the current
#endif
      ,
      mVideoLayerIndex(rhs.mVideoLayerIndex),
      mVideoDisplayFrame(rhs.mVideoDisplayFrame),
      mSkipValidateNextFrame(rhs.mSkipValidateNextFrame),
      mVBlankWatchdog(rhs.mVBlankWatchdog),
      mPageFlipWatchdog(rhs.mPageFlipWatchdog),
      mPageFlipTime(rhs.mPageFlipTime),
      mSetDisplayCount(rhs.mSetDisplayCount),
      mSetDisplayWatchdog(rhs.mSetDisplayWatchdog),
      mDPMSWatchdog(rhs.mDPMSWatchdog),
      mEsdRecoveryStartTime(rhs.mEsdRecoveryStartTime),
      mMaxUnblankingLatency(rhs.mMaxUnblankingLatency),
      mVideoRate(rhs.mVideoRate) {
    for (std::map<uint32_t, DrmShimPlane*>::iterator itr = rhs.mPlanes.begin(); itr != rhs.mPlanes.end(); ++itr) {
      DrmShimPlane* plane = rhs.mPlanes[itr->first];

      if (plane) {
        plane->Flip();
        plane = new DrmShimPlane(*plane);
        plane->SetCrtc(this);
      }

      mPlanes.emplace(itr->first, plane);
   }
}

HwcTestCrtc::~HwcTestCrtc() {
  for (std::map<uint32_t, DrmShimPlane*>::iterator it=mPlanes.begin(); it!=mPlanes.end(); ++it) {
    delete it->second;
  }
}

void HwcTestCrtc::StopThreads() {
}

void HwcTestCrtc::ResetPlanes() {
  mPlanes.clear();
}

void HwcTestCrtc::AddPlane(DrmShimPlane* plane) {
  // Check if we already know the plane
  for (std::map<uint32_t, DrmShimPlane*>::iterator itr = mPlanes.begin(); itr != mPlanes.end(); ++itr) {
    if(itr->second == plane)
     return;
  }

  // Add the plane
  mPlanes.emplace(plane->GetPlaneId(), plane);
  int i = 0;
  for (std::map<uint32_t, DrmShimPlane*>::iterator itr = mPlanes.begin(); itr != mPlanes.end(); ++itr) {
    itr->second->SetPlaneIndex(i++);
  }
}

uint32_t HwcTestCrtc::NumPlanes() {
  return mPlanes.size();
}

void HwcTestCrtc::SetAllPlanesNotUpdated() {
  for (std::map<uint32_t, DrmShimPlane*>::iterator itr = mPlanes.begin(); itr != mPlanes.end(); ++itr) {
    itr->second->SetBufferUpdated(false);
  }
}

void HwcTestCrtc::AddDroppedFrames(uint32_t count) {
  mCurrentCrtc->mDroppedFrameCount += count;
  mCurrentCrtc->mConsecutiveDroppedFrameCount += count;
}

void HwcTestCrtc::UpdateDroppedFrameCounts(bool droppedFrame) {
  // Also save consecutive dropped frame count if it's max
  if (droppedFrame) {
    HWCLOGD("Dropped frame detected on CRTC %d by consistency checking",
            mCrtcId);
    ++mDroppedFrameCount;
    ++mConsecutiveDroppedFrameCount;
  }

  if (mConsecutiveDroppedFrameCount > mMaxConsecutiveDroppedFrameCount) {
    mMaxConsecutiveDroppedFrameCount = mConsecutiveDroppedFrameCount;
  }
  HWCLOGD_COND(eLogBuffer,
               "D%d droppedFrame %s Count=%d consecutive=%d maxConsecutive=%d",
               GetDisplayIx(), droppedFrame ? "YES" : "NO", mDroppedFrameCount,
               mConsecutiveDroppedFrameCount, mMaxConsecutiveDroppedFrameCount);

  if (!droppedFrame) {
    mConsecutiveDroppedFrameCount = 0;
  }
}

void HwcTestCrtc::ResetConsecutiveDroppedFrames() {
  mCurrentCrtc->UpdateDroppedFrameCounts(false);
}

void HwcTestCrtc::ClearDrawnList() {
  HWCLOGV_COND(eLogCombinedTransform, "Crtc %d: Clearing drawn list", mCrtcId);
  mTransforms.clear();

  // Update dropped frame counts. Must be done on live CRTC, not cached copy.
  mCurrentCrtc->UpdateDroppedFrameCounts(mDroppedFrame);
  mDroppedFrame = false;

  // No plane has (yet) changed from 16 to 32 bit
  mBppChangePlane = 0;

  // Start by assuming we are in max FIFO. This will be cleared if we get more
  // than one plane activated,
  // or Z-order is set.
  mWasMaxFifo = mMaxFifo;
  mMaxFifo = true;
}

void HwcTestCrtc::GetDroppedFrameCounts(
    uint32_t& droppedFrameCount, uint32_t& maxConsecutiveDroppedFrameCount,
    bool clear) {
  droppedFrameCount = mDroppedFrameCount;

  if (mConsecutiveDroppedFrameCount > mMaxConsecutiveDroppedFrameCount) {
    mMaxConsecutiveDroppedFrameCount = mConsecutiveDroppedFrameCount;
  }

  maxConsecutiveDroppedFrameCount = mMaxConsecutiveDroppedFrameCount;
  mConsecutiveDroppedFrameCount = 0;

  if (clear) {
    mDroppedFrameCount = 0;
    mMaxConsecutiveDroppedFrameCount = 0;
    memset(mPanelFitterModeCount, 0, sizeof(mPanelFitterModeCount));
  }
}

bool HwcTestCrtc::ClassifyError(HwcTestCheckType& errorCode,
                                HwcTestCheckType normalErrorCode,
                                HwcTestCheckType futureUse) {
  HWCVAL_UNUSED(futureUse);

  // Clone optimization no longer used
  errorCode = normalErrorCode;

  return true;
}

void HwcTestCrtc::SetCurrentFrame(unsigned int frame) {
  mFrame = frame;
}

void HwcTestCrtc::IncCurrentFrame() {
  ++mFrame;
}

void HwcTestCrtc::SetDrmFrame() {
  if (mDrmStartFrame == 0) {
    mDrmStartFrame = mFrame;
  }

  mDrmEndFrame = mFrame;
}

bool HwcTestCrtc::IsFlickerDetected() {
  return (mDrmStartFrame != mDrmEndFrame);
}

uint32_t HwcTestCrtc::GetDrmStartFrame() {
  return mDrmStartFrame;
}

uint32_t HwcTestCrtc::GetDrmEndFrame() {
  return mDrmEndFrame;
}

bool HwcTestCrtc::IsBehavingAsConnected() {
  return mSimulatedHotPlugConnectionState;
}

bool HwcTestCrtc::IsConnected() {
  return (mAvailableModes.size() > 0) && IsBehavingAsConnected();
}

HwcTestCrtc* HwcTestCrtc::SetBlankingRequested(bool blank) {
  // If unblanking is requested and the display is currently off, measure the
  // time until
  // the display is re-enabled
  if (!blank && !mPower.mDPMS && mSimulatedHotPlugConnectionState) {
    mUnblankingTime = systemTime(SYSTEM_TIME_MONOTONIC);
    HWCCHECK(eCheckUnblankingLatency);
  }

  if (blank) {
    // Stop looking for ESD recovery to complete - we may just turn off the
    // display instead.
    mEsdRecoveryStartTime = 0;
  }

  mPower.mBlankingRequested = blank;

  return this;
}

HwcTestCrtc* HwcTestCrtc::SetDPMSEnabled(bool enable) {
  if (enable) {
    if (EsdStateTransition(eEsdModeSet, eEsdComplete)) {
      HWCLOGD("D%d: ESD recovery complete.", GetDisplayIx());
      EsdRecoveryEnd();
    }
  } else {
    // This will be reset when page flip is processed.
    // But we can't use that mechanism to set that flag, as there may not be a
    // page flip.
    mPowerSinceLastUnblankingCheck.mDPMS = false;
    EsdStateTransition(eEsdStarted, eEsdDpmsOff);
    mVBlankWatchdog.Stop();
    mPageFlipWatchdog.Stop();
  }

  HWCLOGD("HwcTestCrtc::SetDPMSEnabled D%d CRTC %d DPMS %s", mDisplayIx,
          mCrtcId, enable ? "ENABLED" : "DISABLED");
  mPower.mDPMS = enable;
  mPageFlipsSinceDPMS = 0;

  return this;
}

void HwcTestCrtc::SetPanelFitter(uint32_t mode) {
}

void HwcTestCrtc::SetDimensions(uint32_t width, uint32_t height, uint32_t clock,
                                uint32_t vrefresh) {
  mWidth = width;
  mHeight = height;
  mClock = clock;
  mVRefresh = vrefresh;

  mCropTransform.SetSourceCrop(0, 0, double(width), double(height));
  // Offset will default to (0,0) and scaling to (1.0,1.0).

  if ((mOutWidth == 0) || (mOutHeight == 0) ||
      ((mOutWidth == mWidth) && (mOutHeight == mHeight))) {
    ResetOutDimensions();
  } else {
    SetOutDimensions(mOutWidth, mOutHeight);
  }
}

void HwcTestCrtc::ResetOutDimensions() {
  mScaleTransform = mCropTransform;
  mScaleTransform.Log(ANDROID_LOG_DEBUG, "Scale transform reset");
}

void HwcTestCrtc::SetOutDimensions(uint32_t width, uint32_t height) {
  mOutWidth = width;
  mOutHeight = height;

  DrmShimFixedAspectRatioTransform transform(mWidth, mHeight, width, height);

  mScaleTransform = transform;
  mScaleTransform.Log(ANDROID_LOG_DEBUG, "Scale transform");

  mCropTransform.SetSourceCrop(0, 0, double(width), double(height));
}

void HwcTestCrtc::SetMosaicTransform(uint32_t srcDisp, double srcLeft,
                                     double srcTop, double width, double height,
                                     double dstLeft, double dstTop) {
  SetMosaicTransform(srcDisp, srcLeft, srcTop, width, height, dstLeft, dstTop,
                     width, height);
}

void HwcTestCrtc::SetMosaicTransform(uint32_t srcDisp, double srcLeft,
                                     double srcTop, double srcWidth,
                                     double srcHeight, double dstLeft,
                                     double dstTop, double dstWidth,
                                     double dstHeight) {
  mSfSrcDisp = srcDisp;

  HWCLOGI("SetMosaicTransform Src D%d (%f,%f) %fx%f Dst D%d (%f,%f) %fx%f",
          srcDisp, srcLeft, srcTop, srcWidth, srcHeight, GetDisplayIx(),
          dstLeft, dstTop, dstWidth, dstHeight);
  HWCLOGD_COND(eLogMosaic, "D%d %dx%d", GetDisplayIx(), mWidth, mHeight);

  ALOG_ASSERT(srcLeft >= 0 && srcTop >= 0 && srcWidth > 0 && srcHeight > 0);
  ALOG_ASSERT(dstLeft >= 0 && dstTop >= 0 && dstWidth > 0 && dstHeight > 0);
  mScaleTransform.SetSourceCrop(srcLeft, srcTop, srcWidth, srcHeight);
  mScaleTransform.SetDisplayOffset(dstLeft + 0.5, dstTop + 0.5);
  mScaleTransform.SetDisplayFrameSize(dstWidth + 0.5, dstHeight + 0.5);
  mScaleTransform.Log(ANDROID_LOG_DEBUG, "Mosaic: Scale transform");
}

void HwcTestCrtc::SetDisplayMapping(const Hwcval::LogDisplayMapping& mapping) {
  SetMosaicTransform(mapping.mLogDisplayIx, mapping.mSrcX, mapping.mSrcY,
                     mapping.mSrcW, mapping.mSrcH, mapping.mDstX, mapping.mDstY,
                     mapping.mDstW, mapping.mDstH);
}

bool HwcTestCrtc::SetPanelFitterSourceSize(uint32_t sourceWidth,
                                           uint32_t sourceHeight) {
  bool isOK = true;
  mPanelFitterSourceWidth = sourceWidth;
  mPanelFitterSourceHeight = sourceHeight;
  const char* modeStr = "OFF";
  HWCLOGV_COND(eLogDrm,
               "Crtc %d: Panel Fitter source size set to %dx%d, mode=%s",
               mCrtcId, sourceWidth, sourceHeight, modeStr);

  if (HwcTestState::getInstance()->GetDeviceType() ==
      HwcTestState::eDeviceTypeBXT) {
    // On Broxton, Panel fitter is same hardware as plane scalers and uses the
    // same rules.
    // Different X and Y scaling is supported.
    DrmShimChecks::BroxtonPlaneValidation(this, 0, "Crtc", mCrtcId, sourceWidth,
                                          sourceHeight, mWidth, mHeight,
                                          hwcomposer::HWCTransform::kIdentity);
  } else {
    uint32_t swdh = sourceWidth * mHeight;
    uint32_t shdw = sourceHeight * mWidth;

    // First check that we are within acceptable divergence from same scaling
    // factor in x & y
    //
    // We do this twice (once assuming that the kernel uses the ratio of the
    // widths
    // as its principal means of working out the scale factor, and once assuming
    // the height).
    // We are quite generous as we are allowing +/- 1% of screen size in both.
    //
    HWCCHECK(eCheckPanelFitterConstantAspectRatio);
    uint32_t whMarginX = mHeight * sourceWidth / 100;
    uint32_t whMarginY = mWidth * sourceHeight / 100;
    if (((swdh - whMarginX) > shdw) || (shdw > (swdh + whMarginX)) ||
        ((shdw - whMarginY) > swdh) || (swdh > (shdw + whMarginY))) {
      HWCERROR(eCheckPanelFitterConstantAspectRatio,
               "Screen %dx%d Panel fitter %dx%d", mWidth, mHeight, sourceWidth,
               sourceHeight);
    }
  }
  CalculatePanelFitterTransform();
  return isOK;
}

void HwcTestCrtc::CalculatePanelFitterTransform() {
  float sw = mPanelFitterSourceWidth;
  float sh = mPanelFitterSourceHeight;
  float desiredAspectRatio = sw / sh;
  float destWidth;
  float destHeight;

  destWidth = mWidth;
  destHeight = mWidth / desiredAspectRatio;

  HWCLOGV(
      "HwcTestCrtc::CalculatePanelFitterTransform: mode=%s sw=%f sh=%f "
      "mWidth=%d mHeight=%d destWidth=%f destHeight=%f",
      "unknown", (double)sw, (double)sh, mWidth, mHeight, (double)destWidth,
      (double)destHeight);
  mPanelFitterTransform = DrmShimTransform(sw, sh, destWidth, destHeight);
}

bool HwcTestCrtc::IsPanelFitterEnabled() {
#ifdef DRM_PFIT_OFF
  return (mPanelFitterMode != DRM_PFIT_OFF);
#else
  return false;
#endif
}

DrmShimTransform& HwcTestCrtc::GetPanelFitterTransform() {
  return mPanelFitterTransform;
}

uint32_t HwcTestCrtc::GetPanelFitterSourceWidth() {
#ifdef DRM_PFIT_OFF
  if (mPanelFitterMode != DRM_PFIT_OFF) {
    return mPanelFitterSourceWidth;
  } else
#endif
  {
    return mWidth;
  }
}

uint32_t HwcTestCrtc::GetPanelFitterSourceHeight() {
#ifdef DRM_PFIT_OFF
  if (mPanelFitterMode != DRM_PFIT_OFF) {
    return mPanelFitterSourceHeight;
  } else
#endif
  {
    return mHeight;
  }
}

void HwcTestCrtc::Checks(Hwcval::LayerList* ll, HwcTestKernel* testKernel,
                         uint32_t hwcFrame) {
  ATRACE_CALL();
  ConfirmNewFrame(mCurrentCrtc->mFrame);
  ++mCurrentCrtc->mValidatedFrameCount;
  mCurrentCrtc->mLastDisplayedFrame = hwcFrame;

  ALOG_ASSERT(mPanelFitterMode <
              (sizeof(mPanelFitterModeCount) / sizeof(uint32_t)));
  ++mCurrentCrtc->mPanelFitterModeCount[mPanelFitterMode];

  if (ll && ll->GetNumLayers()) {
    ClearDrawnList();
    ResetActivePlaneCount();

    // Any planes which had a rotation applied should also have been the subject
    // of a SetPlane
    for (uint32_t i = 0; i < NumPlanes(); ++i) {
      DrmShimPlane* plane = mPlanes[i];
      HWCLOGD_COND(eLogBuffer, "About to expand plane %d", plane->GetPlaneId());

      HWCCHECK(eCheckSetPlaneNeededAfterRotate);
      if (plane->IsRedrawExpected()) {
        HWCERROR(eCheckSetPlaneNeededAfterRotate, "plane %d",
                 plane->GetPlaneId());
        plane->SetRedrawExpected(false);
      }

      DrmShimTransform& transform = plane->GetTransform();
      std::shared_ptr<DrmShimBuffer> buf = transform.GetBuf();

      if (buf.get()) {
        transform.SetPlaneOrder(plane->GetZOrder());
        FlickerClassify(plane, buf);

        // Recursively expand the buffer currently displayed on the plane
        // using the information we have about how buffers were internally
        // composed
        // within hardware composer
        plane->Expand(mTransforms);
        plane->ValidateFormat();
      }
    }

    FlickerChecks();

    if ((GetDisplayIx() > 0) &&
        // Don't check mode if we recently transitioned in or out of Extended
        (testKernel->IsExtendedModeStable()) &&
        // Don't check mode on dropped frame
        (GetDrawCount() > 0)) {
      ExtendedModeChecks(testKernel);
    }
    {
      // Panel is turned off in Extended Mode, so don't generate any errors
      // (We have already checked that the panel was DPMS disabled).
      HWCLOGV_COND(eLogCombinedTransform,
                   "Considering ConsistencyChecks on crtc %d displayIx %d Ext "
                   "Mode Expected %d",
                   GetCrtcId(), GetDisplayIx(),
                   testKernel->IsEMPanelOffRequired());
      if ((GetDisplayIx() > 0) || !testKernel->IsEMPanelOffRequired()) {
        ConsistencyChecks(ll, hwcFrame);
      }
    }
  }
}

void HwcTestCrtc::FlickerClassify(DrmShimPlane* plane,
                                  std::shared_ptr<DrmShimBuffer>& buf) {
  IncActivePlaneCount();

  // For flicker classification, determine if we hafve a 16->32 bit plane change
  uint32_t oldBpp = plane->GetBpp();
  uint32_t bpp = buf->GetBpp();
  HWCLOGD_COND(eLogFlicker, "Plane %d bpp %d", plane->GetPlaneId(), bpp);
  plane->SetBpp(bpp);

  if (oldBpp == 0) {
    // Don't let's generate a flicker error the first time a plane is used
    mDrmStartFrame = mDrmEndFrame;
  } else if (oldBpp != bpp) {
    HWCLOGD_COND(eLogFlicker, "Plane %d has changed from %d to %d-bit",
                 plane->GetPlaneId(), oldBpp, bpp);
    SetBppChangePlane(plane);
  }
}

void HwcTestCrtc::FlickerChecks() {
  ATRACE_CALL();
  // Flicker checks
  if (GetActivePlaneCount() > 1) {
    ClearMaxFifo();
  }

  HWCCHECK(eCheckFlickerMaxFifo);
  HWCCHECK(eCheckFlickerClrDepth);
  HWCCHECK(eCheckFlicker);
  if (IsFlickerDetected()) {
    if (HasLeftMaxFifo()) {
      HWCERROR(eCheckFlickerMaxFifo,
               "DRM calls span VSync (frame %d-%d) on crtc %d",
               GetDrmStartFrame(), GetDrmEndFrame(), GetCrtcId());
    } else if (GetBppChangePlane() != 0) {
      HWCERROR(eCheckFlickerClrDepth,
               "DRM calls span VSync (frame %d-%d) on crtc %d, colour depth "
               "change plane %d",
               GetDrmStartFrame(), GetDrmEndFrame(), GetCrtcId(),
               GetBppChangePlane()->GetPlaneId());
    } else {
      HWCERROR(eCheckFlicker, "DRM calls span VSync (frame %d-%d) on crtc %d",
               GetDrmStartFrame(), GetDrmEndFrame(), GetCrtcId());
    }
  }
}

void HwcTestCrtc::ExtendedModeChecks(HwcTestKernel* testKernel) {
  ATRACE_CALL();

  if (HwcTestState::getInstance()->IsAutoExtMode()) {
    // We don't have video sessions so we aren't going to validate what the test
    // is doing
    // we are just going to assume it is correct.
    return;
  }

  // This is really just validating the test
  HWCCHECK(eCheckExtendedModeExpectation);
  switch (HwcGetTestConfig()->GetStableModeExpect()) {
    case HwcTestConfig::eOff: {
      // The user expected extended mode.
      // Do the inputs from MDS agree with that?
      if (!testKernel->IsEMPanelOffAllowed()) {
        HWCERROR(eCheckExtendedModeExpectation,
                 "Test expects panel to be disabled, but MDS state combined "
                 "with layer list contents is not consistent with this");
      }
      break;
    }

    case HwcTestConfig::eOn: {
      // The user expected we would NOT be in extended mode
      // Do the inputs from MDS agree with that?
      if (testKernel->IsEMPanelOffRequired()) {
        HWCERROR(eCheckExtendedModeExpectation,
                 "Test expects panel to be enabled, but MDS state combined "
                 "with layer list contents suggests it should be turned off");
      }
      break;
    }

    case HwcTestConfig::eDontCare:
      // No check to do.
      if (HwcGetTestConfig()->GetModeExpect() !=
          HwcGetTestConfig()->GetStableModeExpect()) {
        HWCLOGV_COND(eLogVideo,
                     "Mode expectation from test %s, from MDS %s, not stable "
                     "so no error",
                     HwcTestConfig::Str(HwcGetTestConfig()->GetModeExpect()),
                     testKernel->EMPanelStr());
      }
      break;
  }
}

// The consistency checks
// The main purpose of these is to identify any differences between the Layer
// List passed in
// and the expanded list of transforms taken from what was actually displayed
// for the frame (mTransforms).
void HwcTestCrtc::ConsistencyChecks(Hwcval::LayerList* ll, uint32_t hwcFrame) {
  ATRACE_CALL();
  HwcTestCheckType errorCode;
  uint32_t logTransformPriority = 0;
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  char strbuf2[HWCVAL_DEFAULT_STRLEN];
  mVideoLayerIndex = -1;
  HwcTestKernel* testKernel = HwcTestState::getInstance()->GetTestKernel();
  uint32_t cropErrorCount = 0;
  uint32_t scaleErrorCount = 0;

  // For this display, do the FBs indirectly mapped match the requested layer
  // list?
  uint32_t transformIx = 0;

  HWCLOGV_COND(eLogCombinedTransform,
               "HwcTestCrtc::ConsistencyChecks Enter GetNumLayers=%d, "
               "mTransforms.size()=%d",
               ll->GetNumLayers(), mTransforms.size());
  if (hwcFrame == 0) {
    // Validation doesn't work on frame 0 because no previous signalled fences.
    goto func_exit;
  }

  if (SkipAllLayers()) {
    HWCLOGI(
        "HwcTestCrtc::ConsistencyChecks Rotation animation in progress, skip "
        "checks on display %d (frame:%d)",
        GetDisplayIx(), hwcFrame);
    goto func_exit;
  }

  if (!BlankingChecks(ll, hwcFrame)) {
    // Don't generate consistency errors if we already know that it's taken ages
    // to unblank the screen
    // since the chances are that some of the input buffers will have cycled
    goto func_exit;
  }

  // If there is only a FRAMEBUFFERTARGET, mTransforms will be empty
  if ((ll->GetNumLayers() == 1) && (mTransforms.size() == 1)) {
    DrmShimSortedTransformVectorItr itr = mTransforms.begin();
    DrmShimTransform transform = *itr;
    transform.GetBuf()->IdStr(strbuf);
    HWCLOGI(
        "HwcTestCrtc::ConsistencyChecks Nothing to put on D%d. %s is probably "
        "blank.",
        GetDisplayIx(), strbuf);
    goto func_exit;
  } else if (mTransforms.size() == 0) {
    // Display has been blanked.
    // Perhaps we anticipate DPMS disable imminently?
    HWCLOGV_COND(
        eLogCombinedTransform,
        "HwcTestCrtc::ConsistencyChecks mTransforms.size()=0, exiting");
    goto func_exit;
  }

  if (!IsEsdRecoveryMode()) {
    HWCCHECK(eCheckDisabledDisplayBlanked);
    if (!IsDisplayEnabled() && IsBlankingRequested()) {
      HWCERROR(eCheckDisabledDisplayBlanked,
               "HwcTestCrtc::ConsistencyChecks CRTC %d was disabled but not "
               "blanked. DPMS=%d DISP_SCREEN_CONTROL=%d (frame:%d)",
               mCrtcId, mPower.mDPMS, mPower.mDispScreenControl, hwcFrame);
      return;
    }
  }

  for (uint32_t i = 0; i < ll->GetNumLayers(); ++i) {
    HWCLOGV_COND(eLogCombinedTransform,
                 "HwcTestCrtc::ConsistencyChecks Transform validating D%d P%d "
                 "input layer %d",
                 GetSfSrcDisplayIx(), GetDisplayIx(), i);
    Hwcval::ValLayer& layer = ll->GetLayer(i);
    std::shared_ptr<DrmShimBuffer> buf = layer.GetBuf();

    if (buf.get() == 0) {
      HWCLOGV_COND(eLogCombinedTransform, "Null buffer");
      continue;
    }

    HWCNativeHandle handle = buf->GetHandle();

    HWCLOGV_COND(
        eLogCombinedTransform,
        "HwcTestCrtc::ConsistencyChecks D%d layer %d P%d transformIx %d/%d",
        GetSfSrcDisplayIx(), i, GetDisplayIx(), transformIx,
        mTransforms.size());

    if (layer.GetFlags() & HWC_SKIP_LAYER) {
      HWCLOGV_COND(eLogCombinedTransform, "Ignoring SKIP_LAYER");
      continue;
    }

    if (GetVideoLayerIndex() < 0) {
      if (buf->IsVideoFormat()) {
        HWCLOGD_COND(eLogVideo,
                     "HwcTestCrtc::ConsistencyChecks Crtc %d D%d P%d Video "
                     "layer index is %d layer.DisplayFrame: (%d, %d, %d, %d)",
                     mCrtcId, GetSfSrcDisplayIx(), GetDisplayIx(), i,
                     layer.GetDisplayFrame().left, layer.GetDisplayFrame().top,
                     layer.GetDisplayFrame().right,
                     layer.GetDisplayFrame().bottom);
        SetVideoLayerIndex(i, layer.GetDisplayFrame());
      }
    }

    // Initial state: layer has not been found in transform list
    DrmShimSortedTransformVectorItr itr;
    uint32_t transformIxFound = mTransforms.size();

    // Let's see if the next sequential transform (after the last one we
    // matched) is the one we want
    if (transformIx < mTransforms.size()) {
      itr = mTransforms.begin();

      // We don't want to try to match FRAMEBUFFER_TARGETs or Blanking buffers.
      // Avoid apparent Z-order errors by skipping these in the transform list
      // (this will only happen if there are skip layers).
      DrmShimTransform drmShimTransform = *itr;
      if (drmShimTransform.GetBuf()->IsFbt() || drmShimTransform.GetBuf()->IsBlanking()) {
        if (drmShimTransform.GetBuf()->IsFbt()) {
          HWCLOGV_COND(eLogCombinedTransform,
                       "HwcTestCrtc::ConsistencyChecks Ignoring transform "
                       "which is FBT %s at ix %d",
                       drmShimTransform.GetBuf()->IdStr(strbuf), transformIx);
        } else {
          HWCLOGV_COND(eLogCombinedTransform,
                       "HwcTestCrtc::ConsistencyChecks Ignoring transform "
                       "which is Blanking %s at ix %d",
                       drmShimTransform.GetBuf()->IdStr(strbuf), transformIx);
        }

        ++transformIx;
        ++itr;

        if ( itr ==  mTransforms.end()) {
             itr = mTransforms.begin();
        }
        HWCLOGV_COND(
            eLogCombinedTransform,
            "HwcTestCrtc::ConsistencyChecks transformIx=%d, itr=%p",
            transformIx, itr);
      }

      HWCLOGV_COND(eLogCombinedTransform,
                   "HwcTestCrtc::ConsistencyChecks D%d P%d Searching for layer "
                   "%d %s beginning with transform %d @%p",
                   GetSfSrcDisplayIx(), GetDisplayIx(), i, buf->IdStr(strbuf),
                   transformIx, itr);
    }

    HWCCHECK(eCheckLayerDisplay);
    DrmShimTransform drmShimTransform = *itr;
    Hwcval::ValidityType pcValidity = layer.GetValidity();
    if ((pcValidity != ValidityType::Valid) &&
        (pcValidity != ValidityType::ValidUntilModeChange) &&
        (pcValidity != ValidityType::Indeterminate)) {
      if ((itr !=  mTransforms.end()) && (drmShimTransform.GetLayerIndex() == eNoLayer) &&
         drmShimTransform.GetBuf()->IsBlack() && drmShimTransform.GetBuf()->IsFbt()) {
        // Bad protected content has been correctly rendered as a black layer.
        char strbuf2[HWCVAL_DEFAULT_STRLEN];

        // Get the requested transform
        DrmShimTransform layerTransform(buf, i, layer);

        // Apply the portal of the physical screen
        DrmShimTransform croppedLayerTransform(
            layerTransform, GetScaleTransform(), eLogCroppedTransform,
            "Trim [and scale if appropriate] input layer to physical screen "
            "co-ordinates");
        DrmShimTransform croppedScreenTransform(
            drmShimTransform, GetCropTransform(), eLogCroppedTransform,
            "Trim actual display transform to physical screen co-ordinates");

        // Compare the requested display frame of the protected layer with that
        // of the black layer.
        if (!croppedLayerTransform.CompareDf(croppedScreenTransform,
                                             layerTransform, GetDisplayIx(),
                                             this, scaleErrorCount)) {
          // No more checks to be done on this display (frame drop detected)
          break;
        }

        // We have consumed the transform.
        drmShimTransform.SetLayerIndex(i);
        ++transformIx;
        continue;
      } else if (drmShimTransform.GetBuf() == buf) {
        if (drmShimTransform.IsFromSfComp()) {
          HWCLOGW(
              "D%d P%d layer %d %s %s was composed by SF, will be black",
              GetSfSrcDisplayIx(), GetDisplayIx(), i, buf->IdStr(strbuf));
        } else if (pcValidity == ValidityType::Invalid) {
         //Removed protected content check
        } else {
        }

        // We have consumed the transform.
        drmShimTransform.SetLayerIndex(i);
        ++transformIx;
        ++itr;
        continue;
      } else {
        if (buf.get() && !buf->IsFbt()) {
          // We did not find a match for this bad protected layer.
          // Does this layer actually have any part that is visible on this
          // physical screen?
          DrmShimCroppedLayerTransform croppedLayerTransform(buf, i, layer,
                                                             this);

          if (croppedLayerTransform.IsDfIntersecting(mWidth, mHeight)) {
            //removed protected content check
          } else {
            HWCLOGD_COND(eLogCroppedTransform,
                         "D%d Layer %d NOT VISIBLE on physical display %d",
                         GetSfSrcDisplayIx(), i, GetDisplayIx());
          }
        }
        continue;
      }
    } else if ((itr != mTransforms.end()) &&
               drmShimTransform.GetBuf()->IsBlack()) {
      // We have a protected buffer that we think is valid but which HWC has
      // decided to
      // render as black.
      //
      // Use the buffer validity state at the time of OnSet - probably the state
      // has changed since then.
      if (layer.GetValidity() == ValidityType::Valid) {
      }
      drmShimTransform.SetLayerIndex(i);
      ++transformIx;
      ++itr;
      continue;
    } else if ((itr != mTransforms.end()) || (buf != drmShimTransform.GetBuf())) {
      HWCLOGV_COND(eLogCombinedTransform,
                   "HwcTestCrtc::ConsistencyChecks Next sequential transform "
                   "is NOT a match for layer[i]...");
      if (!ClassifyError(errorCode, eCheckLayerDisplay)) {
        // No need to check for any more errors on this CRTC
        break;
      }
      
      itr = mTransforms.begin();

      // ... so let's see if a match exists elsewhere in the transform list
      for (DrmShimSortedTransformVectorItr itr = mTransforms.begin(); itr != mTransforms.end(); ++itr) {
        DrmShimTransform tr = *itr;
        if ((buf == tr.GetBuf()) && (tr.GetLayerIndex() == eNoLayer)) {
          // Match found.
          HWCLOGV_COND(eLogCombinedTransform,
                       "HwcTestCrtc::ConsistencyChecks CheckSetExit: D%d P%d "
                       "found@%p at %d",
                       GetSfSrcDisplayIx(), GetDisplayIx(), &tr, itr);
          break;
        }
      }

      HWCCHECK(eCheckLayerOrder);
      DrmShimTransform tr = *itr;
      if (layer.GetCompositionType() == CompositionType::TGT) {
        // Usually the FRAMEBUFFERTARGET won't match a buffer on the screen as
        // we will
        // have expanded it to its constituents from the SF composition.
        // But, if its constituents are all SKIP layers, we will get a match
        // which we can safely ignore from a validation point of view.
        if (itr != mTransforms.end()) {
          tr.SetLayerIndex(i);
        }
        HWCLOGV_COND(eLogCombinedTransform,
                     "HwcTestCrtc::ConsistencyChecks "
                     "layer.GetCompositionType() == CompositionType::TGT, "
                     "continuing");
        continue;
      } else if (itr != mTransforms.end()) {
        // Ignore this error on 1st frame
        // This can happen as HWC may put up a blanking buffer only.
        if (mValidatedFrameCount < 2) {
          HWCLOGV_COND(eLogCombinedTransform,
                       "HwcTestCrtc::ConsistencyChecks mValidatedFrameCount < "
                       "2, continuing");
          continue;
        }

        if (mVideoLayerIndex >= 0) {
          if (i > (uint32_t)mVideoLayerIndex) {
            // Has this layer been removed by the transparency filter?

            // Transform the display rect of the video layer BACK into the frame
            // of reference
            // of the source in the transparent layer
            const hwcomposer::HwcRect<int>& df = layer.GetDisplayFrame();
            const hwcomposer::HwcRect<float>& sc = layer.GetSourceCrop();

            HWCLOGD_COND(eLogVideo,
                         "HwcTestCrtc::ConsistencyChecks VideoDF (%d,%d,%d,%d) "
                         "LayerDF (%d,%d,%d,%d) layerSourceCropF (%f,%f,%f,%f) "
                         "transform %d",
                         mVideoDisplayFrame.left, mVideoDisplayFrame.top,
                         mVideoDisplayFrame.right, mVideoDisplayFrame.bottom,
                         df.left, df.top, df.right, df.bottom, sc.left, sc.top,
                         sc.right, sc.bottom, layer.GetTransformId());
            hwcomposer::HwcRect<int> rectToCheck =
                InverseTransformRect(mVideoDisplayFrame, layer);

            // then check that rect for transparency.
            if (buf->IsBufferTransparent(rectToCheck)) {
              // Yes
              HWCLOGI(
                  "HwcTestCrtc::ConsistencyChecks Detected buffer discarded by "
                  "transparency filter %s",
                  buf->IdStr(strbuf));
              continue;
            }
          }
        }

        hwcomposer::HwcRect<int> bounds = layer.GetVisibleRegionBounds();

        if ((bounds.left >= bounds.right) || (bounds.top >= bounds.bottom)) {
          // No part of layer is visible, so it was sensible of HWC to remove
          // it.
          HWCLOGV_COND(eLogCombinedTransform,
                       "HwcTestCrtc::ConsistencyChecks No part of layer is "
                       "visible, expected HWC removal, continuing");
        } else {
          // Does this layer actually have any part that is visible on this
          // physical screen?
          DrmShimCroppedLayerTransform croppedLayerTransform(buf, i, layer,
                                                             this);

          if (croppedLayerTransform.IsDfIntersecting(mWidth, mHeight)) {
            if (buf->IsActuallyTransparent()) {
              errorCode = eCheckTransparencyDetectionFailure;
            }

            logTransformPriority = HWCERROR(
                errorCode, "D%d Layer %d %s NOT MAPPED TO P%d WHEN REQUESTED",
                GetSfSrcDisplayIx(), i, buf->IdStr(strbuf), GetDisplayIx());
          } else {
            HWCLOGD_COND(eLogCroppedTransform,
                         "HwcTestCrtc::ConsistencyChecks D%d Layer %d NOT "
                         "VISIBLE on physical display %d",
                         GetSfSrcDisplayIx(), i, GetDisplayIx());
          }
        }
        continue;
      } else if (transformIxFound < transformIx) {
        ClassifyError(errorCode, eCheckLayerOrder);
        logTransformPriority =
            HWCERROR(errorCode, "D%d Layer %d %s is ON SCREEN P%d TOO FAR BACK",
                     GetSfSrcDisplayIx(), i,
                     tr.GetBuf()->IdStr(strbuf), GetDisplayIx());
      } else {
        // This is not a big deal: it must be either because another layer has
        // been inserted
        // (which would be logged as an extra layer error) or it has been
        // swapped with another layer
        // (in which case that one would be logged as too far back).
        HWCLOGW("Display %d Layer %d %s is ON SCREEN P%d TOO FAR FORWARD",
                GetSfSrcDisplayIx(), i, drmShimTransform.GetBuf()->IdStr(strbuf),
                GetDisplayIx());
        transformIx = transformIxFound + 1;  // next one
      }
    } else {
      ++transformIx;
    }

    // layer/expanded plane match found
    HWCLOGV_COND(eLogCombinedTransform,
                 "HwcTestCrtc::ConsistencyChecks D%d P%d Matched transform ix "
                 "%d @%p to layer %d",
                 GetSfSrcDisplayIx(), GetDisplayIx(), transformIxFound,
                 itr, i);
    drmShimTransform.SetLayerIndex(i);

    // If rotation is in progress, co-ordinates may have been perturbed to
    // restore them to
    // what they were last frame. So in that instance we don't compare against
    // the layer list.
    if (!testKernel->IsRotationInProgress(hwcFrame)) {
      // Get the requested transform, as translated by proxy scaling and/or
      // mosaic display settings
      DrmShimCroppedLayerTransform croppedLayerTransform(buf, i, layer, this);

      // Get the actual transform
      DrmShimTransform croppedScreenTransform(
          drmShimTransform, GetCropTransform(), eLogCroppedTransform,
          "Trim actual display transform to physical screen co-ordinates");

      // Find and log differences in the expected and actual transforms
      if (!croppedLayerTransform.Compare(
              croppedScreenTransform, croppedLayerTransform, GetDisplayIx(),
              this, cropErrorCount, scaleErrorCount, hwcFrame)) {
        // No more checks to be done on this display (frame drop detected)
        break;
      }
    } else {
      // TODO: actually check that for snapshot, co-ordinates are retained from
      // previous frame.
    }
  }

  HWCCHECK(eCheckPlaneCrop);
  if (cropErrorCount > 0) {
    HWCERROR(eCheckPlaneCrop, "%d cropping inconsistencies on P%d frame:%d",
             cropErrorCount, GetDisplayIx(), hwcFrame);
  }

  HWCCHECK(eCheckPlaneScale);
  if (scaleErrorCount > 0) {
    HWCERROR(eCheckPlaneScale, "%d scaling inconsistencies on P%d frame:%d",
             scaleErrorCount, GetDisplayIx(), hwcFrame);
  }

  if (IsDroppedFrame()) {
    goto func_exit;
  }
  for (DrmShimSortedTransformVectorItr itr = mTransforms.begin(); itr != mTransforms.end(); ++itr) {
    DrmShimTransform tr = *itr;

    // Logging condition
    if (HWCCOND(eLogCombinedTransform)) {
      tr.Log(ANDROID_LOG_VERBOSE,
                    "Checking for buffers on screen not in layer list");
    }

    HWCCHECK(eCheckLayerDisplay);
    HWCLOGV_COND(eLogCombinedTransform, "HwcTestCrtc::ConsistencyChecks ");

    if (!ClassifyError(errorCode, eCheckLayerDisplay)) {
      // No need to check for any more errors on this CRTC
      break;
    }

    if (tr.GetLayerIndex() != eNoLayer) {
      break;  // // for (uint32_t i=0; i < mTransforms.size(); ++i)
    }
    if (!tr.GetBuf().get()) {
      break;  // // for (uint32_t i=0; i < mTransforms.size(); ++i)
    }

    if (tr.GetBuf()->IsBlanking()) {
      break;  // // for (uint32_t i=0; i < mTransforms.size(); ++i)
    }

    std::shared_ptr<DrmShimBuffer> buf = tr.GetBuf();
    HWCNativeHandle handle = buf->GetHandle();

    if (buf->IsFbt()) {
      // HWCCHECK is in HwcTestKernel::CheckSetEnter
      errorCode = eCheckSkipLayerUsage;
    } else {
      }

    // If this is the snapshot layer in a rotation animation then this is not an
    // error
    if (!testKernel->IsSnapshot(handle, hwcFrame)) {
      logTransformPriority = HWCERROR(
          errorCode, "D%d P%d %s IS MAPPED TO SCREEN WHEN NOT REQUESTED",
          GetSfSrcDisplayIx(), GetDisplayIx(), buf->IdStr(strbuf));
    }

  }  // for (uint32_t i=0; i < mTransforms.size(); ++i)

func_exit:

  if (logTransformPriority > 0) {
    LogTransforms(logTransformPriority, hwcFrame);
  } else if (HWCCOND(eLogCombinedTransform)) {
    LogTransforms(ANDROID_LOG_INFO, hwcFrame);
    HWCLOGV("HwcTestCrtc::ConsistencyChecks Exit");
  }
}

bool HwcTestCrtc::BlankingChecks(Hwcval::LayerList* ll, uint32_t hwcFrame) {
  bool ret = true;

  // If previous layer list had any content...
  if (mPower.mHasContent) {
    // If display was black, but won't be after this...
    if ((mPower.mBlack || (!mPowerSinceLastUnblankingCheck.mDPMS)) &&
        mPower.mDPMS) {
      // Display is unblanked: was it done in timely fashion?
      if (mUnblankingTime > 0) {
        int64_t unblankingLatency =
            systemTime(SYSTEM_TIME_MONOTONIC) - mUnblankingTime;
        if (unblankingLatency > mMaxUnblankingLatency) {
          // HWCCHECK is done where unblanking is started
          HWCERROR(eCheckUnblankingLatency,
                   "Unblanking CRTC %d took %fms (limit %dms)", mCrtcId,
                   double(unblankingLatency) / HWCVAL_MS_TO_NS,
                   int(mMaxUnblankingLatency / HWCVAL_MS_TO_NS));
        }

        // We have dealt with the unblanking
        mCurrentCrtc->mUnblankingTime = 0;
        mPowerSinceLastUnblankingCheck = mPower;

        // Don't do any more checks this frame: we have just unblanked, which
        // disrupts the pipeline
        // and may mean that by the time we perform the checks, the buffers have
        // been overwritten.
        return false;
      }
    }
  } else {
    // Nothing is supposed to be on the screen, so reset the unblanking timer
    mCurrentCrtc->mUnblankingTime = 0;
  }

  if (!WasBlankingRequested()      // Blanking request from SF(/harness) is not
                                   // active
      && mPower.mBlack             // and screen is black
      && mPower.mHasContent        // and last frame had content
      && (ll->GetNumLayers() > 1)  // and this frame has content
      && mPower.mDPMS              // and DPMS is enabled
      &&
      (mTransforms.size() == 0))  // and at least one of the planes is not blank
  {
    // Display was blanked and we haven't had DPMS disable.
    //
    // NOTE: For this check, the evaluation count is incremented at
    // the ClassifyCheckEval() function later in this routine, so we don't need
    // to call
    // HWCCHECK.
    // This check is being done later in time, hence why it's not adjacent to
    // the ClassifyCheckEval().
    HWCERROR(eCheckLayerDisplay,
             "CRTC %d was blanked for no apparent reason (frame:%d)",
             GetCrtcId(), hwcFrame);
  }

  // Set the "display is black" flag if either there is no content, or it is
  // DPMS disabled
  mCurrentCrtc->SetDisplayIsBlack(mTransforms.size(), ll->GetNumLayers());

  return ret;
}

void HwcTestCrtc::SetDisplayIsBlack(uint32_t numTransforms,
                                    uint32_t numLayers) {
  mPowerLastFlip = mPower;

  // Set the "display is black" flag if either there is no content, or it is
  // DPMS disabled
  mPower.mBlack = (numTransforms == 0) || (!mPower.mDPMS);
  HWCLOGV(
      "mCurrentCrtc->mDisplayIsBlack=%d mTransforms.size()=%d "
      "mCurrentCrtc->mDPMSEnabled=%d",
      mPower.mBlack, numTransforms, mPower.mDPMS);
  mPower.mHasContent = (numLayers > 1);
}

void HwcTestCrtc::ReportPanelFitterStatistics(FILE* f) {
  fprintf(f, "Crtc %2d \n", mCrtcId);
}

void HwcTestCrtc::LogTransforms(int priority, uint32_t hwcFrame) {
  HWCLOG(priority, "Actual display list for CRTC %d frame:%d:", GetCrtcId(),
         hwcFrame);
  for (DrmShimSortedTransformVectorItr itr = mTransforms.begin(); itr != mTransforms.end(); ++itr) {
    DrmShimTransform tr = *itr;
    char strbuf[HWCVAL_DEFAULT_STRLEN];
    DrmShimTransform transform = *itr;
    HWCLOG(priority, "%2d %s", itr, transform.GetBuf()->IdStr(strbuf));
  }
}

void HwcTestCrtc::LogPlanes(int priority, const char* str) {
  HWCLOG(priority, "%s: CRTC %d: Planes", str, mCrtcId);

  for (std::map<uint32_t, DrmShimPlane*>::iterator itr = mPlanes.begin(); itr != mPlanes.end(); ++itr) {
    if(itr->second)
     itr->second->Log(priority);
  }
}

void HwcTestCrtc::ConfirmNewFrame(uint32_t frame) {
  if (mDrmEndFrame > frame) {
    mDrmStartFrame = frame;
  } else {
    mDrmStartFrame = 0;
    mDrmEndFrame = 0;
  }
}

void HwcTestCrtc::NotifyRetireFence(int retireFenceFd) {
  if (retireFenceFd > 0)
    hwcomposer::HWCPoll(retireFenceFd, -1);

  StopPageFlipWatchdog();
  ResetDrawCount();
  PageFlipsSinceDPMS();
  NotifyPageFlip();
}

void HwcTestCrtc::SkipValidateNextFrame() {
  mSkipValidateNextFrame = true;
}

bool HwcTestCrtc::AmSkippingFrameValidation() {
  if (mSkipValidateNextFrame) {
    mCurrentCrtc->mSkipValidateNextFrame = false;
    return true;
  } else {
    return false;
  }
}

bool HwcTestCrtc::IsUsing(std::shared_ptr<DrmShimBuffer> buf) {
  for (std::map<uint32_t, DrmShimPlane*>::iterator itr = mPlanes.begin(); itr != mPlanes.end(); ++itr) {
    if(itr->second)
     itr->second->IsUsing(buf);
     return true;
  }

  return false;
}

bool HwcTestCrtc::VBlankActive(bool active) {
  int32_t ret = android_atomic_swap((int32_t)active, &mVBlankActive);

  if (active) {
    mVBlankWatchdog.Start();
  }

  return (bool)ret;
}

bool HwcTestCrtc::WaitInactiveVBlank(uint32_t ms) {
  // Wait for last VBlank to happen, and disable the active flag
  uint32_t count = 0;
  while (mVBlankActive && (count++ < ms)) {
    usleep(1000);
  }

  return mVBlankActive;
}

bool HwcTestCrtc::SimulateHotPlug(bool connected) {
  HWCVAL_UNUSED(connected);
  return false;
}

void HwcTestCrtc::RecordDroppedFrames(uint32_t droppedFrames) {
  if (droppedFrames > 0) {
    if (mPageFlipsSinceDPMS <= 2) {
      HWCLOGI(
          "Ignoring %d constructively dropped frames on display %d because %d "
          "flips since DPMS",
          droppedFrames, mDisplayIx, mPageFlipsSinceDPMS);
    } else {
      HWCLOGI("HWC constructively dropped %d frames on display %d",
              droppedFrames, mDisplayIx);
      AddDroppedFrames(droppedFrames);
    }
  }
}

void HwcTestCrtc::SetDisplayFailed(bool failed) {
  mSetDisplayFailed = failed;

  if (failed) {
    ++mSetDisplayFailCount;
  } else {
    ++mSetDisplayPassCount;
  }
}

bool HwcTestCrtc::DidSetDisplayFail() {
  return mSetDisplayFailed;
}

bool HwcTestCrtc::IsTotalDisplayFail() {
  return ((mSetDisplayFailCount > 50) && (mSetDisplayPassCount < 10));
}

// VSync
void HwcTestCrtc::QueueCaptureVBlank(int fd,
                                     HwcTestEventHandler* vsyncRestorer) {
  mQueuedVSyncRequest = vsyncRestorer;
  mQueuedVSyncFd = fd;
}

void HwcTestCrtc::ExecuteCaptureVBlank() {
  if (mQueuedVSyncRequest) {
    mQueuedVSyncRequest->CaptureVBlank(mQueuedVSyncFd, mCrtcId);
    mQueuedVSyncRequest = 0;
  }
}

void HwcTestCrtc::MarkEsdRecoveryStart() {
  if (IsDisplayEnabled()) {
    mEsdRecoveryStartTime = systemTime(SYSTEM_TIME_MONOTONIC);
  }
}

void HwcTestCrtc::EsdRecoveryEnd(const char* str) {
  if (mEsdRecoveryStartTime) {
    int64_t esdRecoveryDuration =
        systemTime(SYSTEM_TIME_MONOTONIC) - mEsdRecoveryStartTime;
    mEsdRecoveryStartTime = 0;

    if (esdRecoveryDuration > (3 * HWCVAL_SEC_TO_NS)) {
      HWCERROR(eCheckEsdRecovery, "ESD Recovery CRTC %d %s %fs", mCrtcId, str,
               (double(esdRecoveryDuration) / HWCVAL_SEC_TO_NS));
    }
  }
}

void HwcTestCrtc::StopPageFlipWatchdog() {
  mPageFlipWatchdog.Stop();
  mPageFlipTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

void HwcTestCrtc::ClearUserMode() {
  mUserModeState = eUserModeNotSet;
}

void HwcTestCrtc::SetUserModeStart() {
  mUserModeState = eUserModeChanging;
}

void HwcTestCrtc::SetUserModeFinish(int st, uint32_t width,
                                    uint32_t height, uint32_t refresh,
                                    uint32_t flags, uint32_t ratio) {
  if (st == 0) {
    mUserMode.width = width;
    mUserMode.height = height;
    mUserMode.refresh = refresh;
    mUserModeState = eUserModeSet;
  } else {
    mUserModeState = eUserModeUndefined;
  }
}

void HwcTestCrtc::SetAvailableModes(const HwcTestCrtc::ModeVec& modes) {
  HWCLOGD_COND(eLogVideo, "D%d SetAvailableModes: %d modes", mDisplayIx,
               modes.size());
  mAvailableModes = modes;
  mPreferredModeCount = 0;

  // Don't generate any wrong mode errors until HWC has a chance to process
  // this.
  // HWC issues drmModeGetConnector on the hotplug thread not the drm thread, so
  // inherently it is racing the
  // drmModeSetDisplay calls which define when mode validation is done.
  mFramesSinceRequiredModeChange = 0;
}

void HwcTestCrtc::SetActualMode(const HwcTestCrtc::Mode& mode) {
  mActualMode = mode;
}

uint32_t HwcTestCrtc::GetVideoRate() {
  HwcTestState* state = HwcTestState::getInstance();
  if (state->IsAutoExtMode()) {
    return uint32_t(mVideoRate + 0.5);
  } else {
    return state->GetTestKernel()->GetMDSVideoRate();
  }
}

void HwcTestCrtc::ValidateMode(HwcTestKernel* testKernel) {
  HWCLOGD("ValidateMode D%d entry", GetDisplayIx());
  if (!IsDisplayEnabled()) {
    // Don't validate mode if the display is turned off, or is about to be
    HWCLOGD(
        "ValidateMode early exit - display enabled %d blanking requested %d",
        IsDisplayEnabled(), IsBlankingRequested());
    ;
    return;
  }

  bool extendedMode = testKernel->IsExtendedModeRequired();
  uint32_t videoRate = GetVideoRate();

  Mode requiredMode = mPreferredMode;

  if (mUserModeState == eUserModeSet) {
    requiredMode = mUserMode;
  } else if (mUserModeState != eUserModeNotSet) {
    HWCLOGI("User mode in transition, not validating");
    return;
  } else {
    if (mPreferredModeCount != 1) {
      HWCLOGW("Can't validate mode because the number of preferred modes is %d",
              mPreferredModeCount);
      return;
    }
  }

  // Check the rate
  HWCCHECK(eCheckDisplayMode);

  bool matchRefresh = false;
  bool drrs = false;

  if (mDisplayType == HwcTestState::eFixed) {
    drrs = IsDRRSEnabled();
    matchRefresh = drrs && (videoRate > 0);
    HWCLOGV_COND(eLogVideo, "DRRS %s: %s matching video rate %d",
                 drrs ? "ON" : "OFF", matchRefresh ? "" : "not", videoRate);
  } else if (HwcTestState::getInstance()->GetHwcOptionInt("modechange")) {
    matchRefresh = (extendedMode && (videoRate > 0));
    HWCLOGV_COND(eLogVideo, "Extended mode %d videoRate %d => matchRefresh %d",
                 extendedMode, videoRate, matchRefresh);
  }

  uint32_t refreshForChangeDetection = requiredMode.refresh;
  bool mismatch = false;

// We are only going to do refresh rate validation if gralloc supports
// media timestamps.
#ifdef HWCVAL_GRALLOC_HAS_MEDIA_TIMESTAMPS
  if (matchRefresh) {
    // Requirement is to match the video rate and avoid a change of resolution
    // So we aren't looking to match the user requested mode in this case.
    if ((mActualMode.refresh % videoRate) != 0) {
      // Failed to match the video rate
      HWCLOGD_COND(
          eLogVideo,
          "Failed to match the video rate. actual mode %f video rate %f",
          double(mActualMode.refresh), double(videoRate));
      mismatch = true;
    }

    // This is to ensure the "frames since required mode change" counter gets
    // reset
    // What we actually require is an integer multiple of videoRate
    refreshForChangeDetection = videoRate;
  } else if (mActualMode.refresh != requiredMode.refresh) {
    // There are issues with refresh matching when auto ext mode is not enabled
    if (HwcTestState::getInstance()->IsAutoExtMode()) {
      mismatch = true;
    }
  }
#endif

  if ((requiredMode.width == mRequiredMode.width) &&
      (requiredMode.height == mRequiredMode.height) &&
      (refreshForChangeDetection == mRequiredMode.refresh)) {
    ++mFramesSinceRequiredModeChange;
  } else {
    mRequiredMode = requiredMode;
    mRequiredMode.refresh = refreshForChangeDetection;
    mFramesSinceRequiredModeChange = 0;
  }

  if (mFramesSinceRequiredModeChange < HWCVAL_EXTENDED_MODE_CHANGE_WINDOW) {
    HWCLOGI("Mode change required %d frames ago, not validating yet",
            mFramesSinceRequiredModeChange);
    return;
  }

  if ((mActualMode.width != requiredMode.width) ||
      (mActualMode.height != requiredMode.height)) {
    mismatch = true;
  }

  if (mismatch) {
    uint32_t minRefresh = UINT_MAX;
    uint32_t maxRefresh = 0;

    // Does a mode exist which matches the current requirement?
    for (uint32_t i = 0; i < mAvailableModes.size(); ++i) {
      const Mode& mode = mAvailableModes.at(i);
      if ((mode.width == requiredMode.width) &&
          (mode.height == requiredMode.height)) {
        bool err = false;
        if (matchRefresh) {
          if (drrs) {
            minRefresh = min(mode.refresh, minRefresh);
            maxRefresh = max(mode.refresh, maxRefresh);
          } else {
            // HWC will not INCREASE the refresh rate due to video rate matching
            if (mActualMode.refresh > mode.refresh) {
              if ((mode.refresh % videoRate) == 0) {
                err = true;
              }
            }
          }
        } else {
          if (requiredMode.refresh == mode.refresh) {
            err = true;
          }
        }
        if (err) {
          break;
        }
      }
    }

  } else if (requiredMode.refresh != mActualMode.refresh) {
    HWCLOGD_COND(
        eLogVideo,
        "Video mode override in progress: %dx%d using refresh %d not %d",
        mActualMode.width, mActualMode.height, mActualMode.refresh,
        requiredMode.refresh);
  }
}

bool HwcTestCrtc::RecentModeChange() {
  return (mFramesSinceRequiredModeChange <=
          HWCVAL_EXTENDED_MODE_CHANGE_WINDOW) ||
         (mUserModeState == eUserModeChanging);
}

bool HwcTestCrtc::MatchMode(uint32_t w, uint32_t h, uint32_t rate) {
  for (uint32_t i = 0; i < mAvailableModes.size(); ++i) {
    const Mode& mode = mAvailableModes.at(i);

    bool matches = (mode.width == w || w == 0) &&
                   (mode.height == h || h == 0) &&
                   (mode.refresh == rate || rate == 0);
    HWCLOGD_COND(
        eLogMosaic, "P%d Available Mode %d: %dx%d@%d Testing: %dx%d@%d => %s",
        mDisplayIx, i, mActualMode.width, mActualMode.height,
        mActualMode.refresh, w, h, rate, matches ? "MATCH" : "NO MATCH");

    if (matches) {
      mUserMode = mode;
      mUserModeState = eUserModeSet;
      return true;
    }
  }

  return false;
}

const char* HwcTestCrtc::ReportPower(char* strbuf, uint32_t len) {
  return mPower.Report(strbuf, len);
}

void HwcTestCrtc::SetDPMSInProgress(bool inProgress) {
  mPower.mDPMSInProgress = inProgress;

  if (inProgress) {
    mDPMSWatchdog.Start();
  } else {
    mDPMSWatchdog.Stop();
  }
}

bool HwcTestCrtc::IsDRRSEnabled() {
  // Default implementation
  return false;
}
