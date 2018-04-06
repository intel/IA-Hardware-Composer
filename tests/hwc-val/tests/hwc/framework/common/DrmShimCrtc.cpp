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

#include "DrmShimCrtc.h"
#include "DrmShimChecks.h"
#include "HwcTestUtil.h"
#include "HwcTestState.h"
#include "HwcvalContent.h"

#include <limits.h>
#include <drm_fourcc.h>

DrmShimCrtc::DrmShimCrtc(uint32_t crtcId, uint32_t width, uint32_t height,
                         uint32_t clock, uint32_t vrefresh)
    : HwcTestCrtc(crtcId, width, height, clock, vrefresh),
      mChecks(0),
      mPipeIx(0),
      mVBlankFrame(0),
      mVBlankSignal(0),
      mPageFlipUserData(0) {
  memset(&mVBlank, 0, sizeof(mVBlank));
}

DrmShimCrtc::~DrmShimCrtc() {
}

// VBlank handling
drmVBlankPtr DrmShimCrtc::SetupVBlank() {
  uint32_t flags = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;

  switch (mPipeIx) {
    case 1:
      flags |= DRM_VBLANK_SECONDARY;
      break;

    case 2:
      flags |= ((2 << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK);
      break;

    default:
      break;
  }

  mVBlank.request.type = (drmVBlankSeqType)flags;
  mVBlank.request.sequence = 1;
  mVBlank.request.signal = mCrtcId;

  HWCLOGV_COND(eLogEventHandler,
               "DrmShimCrtc::SetupVBlank mVBlank.request.type=0x%x "
               ".sequence=%d .signal=%p",
               mVBlank.request.type, mVBlank.request.sequence,
               mVBlank.request.signal);

  return &mVBlank;
}

drmVBlankPtr DrmShimCrtc::GetVBlank() {
  return &mVBlank;
}

void DrmShimCrtc::SetUserVBlank(drmVBlankPtr vbl) {
  if (vbl->request.type & DRM_VBLANK_RELATIVE) {
    mVBlankFrame = mFrame + vbl->request.sequence;
  } else {
    mVBlankFrame = vbl->request.sequence;
    // At this point could raise the event immediately if mVBlankFrame <=
    // mFrame.
    // Would need to return a status and let the caller deal with it.
    // However no use is made of absolute counts as far as I know.
  }

  HWCLOGV_COND(eLogEventHandler,
               "DrmShimCrtc:: SetUserVBlank crtc %d VBlankFrame %d", mCrtcId,
               mVBlankFrame);

  mVBlankSignal = vbl->request.signal;
}

bool DrmShimCrtc::IsVBlankRequested(uint32_t frame) {
  if ((frame >= mVBlankFrame) && (mVBlankSignal != 0)) {
    mVBlankFrame = UINT_MAX;
    return true;
  } else {
    HWCLOGD_COND(eLogEventHandler,
                 "IsVBlankRequested: No: crtc %d frame=%d, mVBlankFrame=%d, "
                 "mVBlankSignal=0x%" PRIx64,
                 mCrtcId, frame, mVBlankFrame, mVBlankSignal);
    return false;
  }
}

void* DrmShimCrtc::GetVBlankUserData() {
  return (void*)mVBlankSignal;
}

// VBlank intercepted from DRM.
// Do any local actions we want, and decide if DRM shim should pass on the
// callback.
bool DrmShimCrtc::IssueVBlank(unsigned int frame, unsigned int sec,
                              unsigned int usec, void*& userData) {
  HWCLOGV_COND(eLogEventHandler,
               "DrmShimCrtc:: IssueVBlank crtc %d frame:%d VBlankFrame %d",
               mCrtcId, frame, mVBlankFrame);
  HWCVAL_UNUSED(sec);   // Possible future use
  HWCVAL_UNUSED(usec);  // Possible future use

  // No longer returning user data from here
  HWCVAL_UNUSED(userData);

  if (frame > mFrame) {
    // All we do now at this stage is make sure each VSync comes only once
    mFrame = frame;
    return true;
  } else {
    return false;
  }
}

void DrmShimCrtc::SavePageFlipUserData(uint64_t userData) {
  HWCLOGV_COND(eLogEventHandler,
               "DrmShimCrtc::SavePageFlipUserData crtc %d userData %" PRIx64,
               mCrtcId, userData);
  mPageFlipUserData = userData;
}

uint64_t DrmShimCrtc::GetPageFlipUserData() {
  ALOG_ASSERT(this);
  return mPageFlipUserData;
}

void DrmShimCrtc::DrmCallStart() {
  mSetDisplayWatchdog.Start();
  mDrmCallStartTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

int64_t DrmShimCrtc::GetDrmCallDuration() {
  return systemTime(SYSTEM_TIME_MONOTONIC) - mDrmCallStartTime;
}

int64_t DrmShimCrtc::GetTimeSinceVBlank() {
  return systemTime(SYSTEM_TIME_MONOTONIC) - mVBlankWatchdog.GetStartTime();
}

bool DrmShimCrtc::SimulateHotPlug(bool connected) {
  HWCLOGD("Logically %sconnecting D%d crtc %d", connected ? "" : "dis",
          GetDisplayIx(), GetCrtcId());
  mSimulatedHotPlugConnectionState = connected;

  // Cancel any unblanking checks
  mUnblankingTime = 0;

  // We can't directly spoof the uevent that HWC receives so we tell
  // HwcTestState
  // to make a direct call into HWC to simulate the hot plug.
  return false;
}

uint32_t DrmShimCrtc::SetDisplayEnter(bool suspended) {
  if (HwcTestState::getInstance()->IsOptionEnabled(eOptPageFlipInterception)) {
    HWCCHECK(eCheckDispGeneratesPageFlip);
    if ((mPageFlipTime < mPageFlipWatchdog.GetStartTime()) &&
        (mSetDisplayCount > 1)) {
      // No page flips since last setdisplay on this CRTC.
      // Probably means display is in a bad way.
      HWCERROR(eCheckDispGeneratesPageFlip, "Crtc %d: No page flip since %fs",
               mCrtcId, double(mPageFlipTime) / HWCVAL_SEC_TO_NS);
    }

    if (IsDisplayEnabled()) {
      mPageFlipWatchdog.StartIfNotRunning();
    }
  }

  mSetDisplayCount++;

  mPowerStartSetDisplay = mPower;
  mSuspendStartSetDisplay = suspended;

  return mFramesSinceModeSet++;
}

void DrmShimCrtc::StopSetDisplayWatchdog() {
  mSetDisplayWatchdog.Stop();
}

const char* DrmShimCrtc::ReportSetDisplayPower(char* strbuf, uint32_t len) {
  uint32_t n = snprintf(strbuf, len, "Enter(");
  if (n >= len)
    return strbuf;

  mPowerStartSetDisplay.Report(strbuf + n, len - n);
  n = strlen(strbuf);
  if (n >= len)
    return strbuf;

  n += snprintf(strbuf + n, len - n, ") Exit(");
  if (n >= len)
    return strbuf;

  GetPower().Report(strbuf + n, len - n);
  n = strlen(strbuf);
  if (n >= len)
    return strbuf;

  n += snprintf(strbuf + n, len - n, ")");

  return strbuf;
}

bool DrmShimCrtc::IsDRRSEnabled() {
  ALOG_ASSERT(mChecks);
  return mChecks->IsDRRSEnabled(GetConnector());
}

void DrmShimCrtc::NotifyPageFlip() {
  ALOG_ASSERT(mChecks);
  mChecks->GetCrcReader().NotifyPageFlip(this);
}
