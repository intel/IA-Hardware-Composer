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

/* DrmShimEventHandler
* ===================
*
* This class runs a thread that captures VBlank and page flip events from
* DRM, so that the VBlank events can be used within the shim for such
* as flicker detection.
*
* In addition, the class emulates the behaviour that the HWC client will expect
* in terms of requesting these events and supplying the necessary callbacks.
*
* WaitVBlank provides the emulation of drmWaitVBlank. It requests (and
*optionally
* waits for) the next VSync event.
* Normally, this will be asynchronous (indicated by the DRM_VBLANK_EVENT
* flag).
*
* To collect the event, the client will have a thread running in which
* it calls DrmHandleEvent iteratively. This is implemented here by
* HandleEvent. It stores the event context, providing the addresses of the
* client's callback functions, and waits for the event to arrive using the
* android Condition object. It then pulls the event from the event queue
* (this is implemented here as an array of 10 events for safety as it's just
*possible
* that multiple displays will send their events at the same time).
* With the event in hand, it is then able to call the user's callback function
* for VBlank or Page Flip as appropriate.
*
* Meanwhile, in the thread, the call to the real DrmHandleEvent results in the
* vblank_hander and page_flip_handler being called.
* The page_flip_handler simply puts the event in the event
* queue so that it will be dispatched by any running
* HandleEvent.
*
* The vblank_handler calls DrmShimCrtc::IssueVBlank to find
* out if the client has requested a VBlank callback
* this frame. If so, the event is placed in the event queue.
*/

#include <utils/Trace.h>

#include "DrmShimEventHandler.h"
#include "DrmShimChecks.h"
#include "DrmShimCrtc.h"
#include "DrmShimCallbackBase.h"
#include "HwcTestDefs.h"
#include "HwcTestState.h"
#include "HwcTestConfig.h"
#include "HwcTestLog.h"
#include "HwcTestUtil.h"

#include "drm_shim.h"


extern int (*fpDrmWaitVBlank)(int fd, drmVBlankPtr vbl);
extern int (*fpDrmHandleEvent)(int fd, drmEventContextPtr evctx);
extern DrmShimCallbackBase* drmShimCallback;

DrmShimEventHandler* DrmShimEventHandler::mInstance = 0;

DrmShimEventHandler::DrmShimEventHandler(DrmShimChecks* checks)
    : EventThread("DrmShimEventHandler"), mChecks(checks), mDrmFd(0) {
  mInstance = this;
  memset(&mUserEvctx, 0, sizeof(mUserEvctx));

  // Set up event context
  memset(&mRealEvctx, 0, sizeof mRealEvctx);
  mRealEvctx.version = DRM_EVENT_CONTEXT_VERSION;
  mRealEvctx.vblank_handler = vblank_handler;
  mRealEvctx.page_flip_handler = NULL;

  // No saved page flip event
  mSavedPF.eventType = DrmEventData::eNone;
}

DrmShimEventHandler::~DrmShimEventHandler() {
  mInstance = 0;
}

void DrmShimEventHandler::QueueCaptureVBlank(int fd, uint32_t crtcId) {
  HWCLOGD_COND(
      eLogEventHandler,
      "DrmShimEventHandler::QueueCaptureVBlank @ %p: mChecks=%p, crtcId=%d",
      this, mChecks, crtcId);
  mDrmFd = fd;

  DrmShimCrtc* crtc = mChecks->GetCrtc(crtcId);

  if (!crtc) {
    HWCLOGW("DrmShimEventHandler::QueueCaptureVBlank: no CRTC %d", crtcId);
    return;
  }

  crtc->QueueCaptureVBlank(fd, this);
}

void DrmShimEventHandler::CaptureVBlank(int fd, uint32_t crtcId) {
  HWCLOGD_COND(eLogEventHandler,
               "DrmShimEventHandler::CaptureVBlank @ %p: mChecks=%p, crtcId=%d",
               this, mChecks, crtcId);
  mDrmFd = fd;

  DrmShimCrtc* crtc = mChecks->GetCrtc(crtcId);

  if (!crtc) {
    HWCLOGW("DrmShimEventHandler::CaptureVBlank: no CRTC %d", crtcId);
    return;
  }

  crtc->EnableVSync(true);

  if (!crtc->VBlankActive(true)) {
    ALOG_ASSERT(fpDrmWaitVBlank);

    // Request first event.
    drmVBlankPtr vbl = crtc->SetupVBlank();

    if (fd == 0) {
      HWCLOGW("DrmShimEventHandler::CaptureVBlank: crtc %d, No fd available",
              crtcId);
      return;
    }

    HWCLOGV_COND(eLogEventHandler,
                 "DrmShimEventHandler::CaptureVBlank: fd=0x%x", fd);
    int ret = fpDrmWaitVBlank(fd, vbl);

    if (ret != 0) {
      HWCLOGW("DrmShimEventHandler::CaptureVBlank drmWaitVBlank FAILED (%d)",
              ret);
      crtc->EnableVSync(false);  // VSync capture not enabled on this CRTC.
      crtc->VBlankActive(
          false);  // We don't have a current drmWaitVBlank active.
    } else {
      EnsureRunning();
    }
  }

  HwcTestState::getInstance()->SetVSyncRestorer(this);
}

void DrmShimEventHandler::CancelEvent(uint32_t crtcId) {
  HWCLOGD_COND(eLogEventHandler, "DrmShimEventHandler::CancelEvent CRTC %d",
               crtcId);
  DrmShimCrtc* crtc = mChecks->GetCrtc(crtcId);

  if (crtc) {
    bool enabled = crtc->IsVSyncEnabled();

    if (enabled) {
      crtc->EnableVSync(false);

      // Wait for long enough for one more VBlank to happen
      if (crtc->WaitInactiveVBlank(100)) {
        HWCLOGW(
            "DrmShimEventHandler::CancelEvent crtc %d, wait for last VBlank "
            "timed out.",
            crtcId);
      }
    }

    HWCLOGD_COND(eLogEventHandler,
                 "DrmShimEventHandler::CancelEvent crtc %d complete.", crtcId);
  }
}

int DrmShimEventHandler::WaitVBlank(drmVBlankPtr vbl) {
  ATRACE_CALL();
  uint32_t displayIx = 0;
  uint32_t pipeIx = 0;

  if (vbl->request.type & DRM_VBLANK_SECONDARY) {
    pipeIx = 1;
  } else {
    pipeIx = (vbl->request.type & DRM_VBLANK_HIGH_CRTC_MASK) >>
             DRM_VBLANK_HIGH_CRTC_SHIFT;
  }

  HWCLOGV_COND(
      eLogEventHandler,
      "DrmShimEventHandler::WaitVBlank request.type 0x%x pipe %d displayIx %d",
      vbl->request.type, pipeIx, displayIx);

  DrmShimCrtc* crtc = mChecks->GetCrtcByPipe(pipeIx);
  displayIx = crtc->GetDisplayIx();

  if (crtc) {
    crtc->SetUserVBlank(vbl);

    if ((vbl->request.type & DRM_VBLANK_EVENT) == 0) {
      // Wait for VBlank actually to occur
      HWCLOGV_COND(eLogEventHandler,
                   "DrmShimEventHandler::WaitVBlank: waiting for VBlank "
                   "actually to occur on display %d",
                   displayIx);
      Hwcval::Mutex::Autolock lock(mMutex);
      if (mCondition.waitRelative(mMutex, 100000000)) {
        HWCLOGD(
            "DrmShimEventHandler::WaitVBlank: No VBlank event within 100ms on "
            "display %d",
            displayIx);
        mCondition.wait(mMutex);
      }

      RaiseEventFromQueue();
    } else {
      HWCLOGV_COND(
          eLogEventHandler,
          "DrmShimEventHandler::WaitVBlank: Setup async vblank display %d",
          displayIx);
    }
  } else {
    HWCLOGW("DrmShimEventHandler::WaitVBlank: no display %d", displayIx);
  }

  return 0;
}

int DrmShimEventHandler::HandleEvent(int fd, drmEventContextPtr evctx) {
  ATRACE_CALL();
  HWCLOGV_COND(eLogEventHandler, "DrmShimEventHandler::HandleEvent fd=%d entry",
               fd);
  mUserEvctx = *evctx;
  mContinueHandleEvent = true;

  RaiseEventFromQueue();

  HWCLOGV_COND(eLogEventHandler, "DrmShimEventHandler::HandleEvent fd=%d exit",
               fd);
  return 0;
}

bool DrmShimEventHandler::RaiseEventFromQueue() {
  DrmEventData event;

  if (!ReadWait(event)) {
    return false;
  }

  ATRACE_CALL();
  if (event.eventType == DrmEventData::eVBlank) {
    int64_t timeAfterVBlank = event.crtc->GetTimeSinceVBlank();

    if (event.crtc->IsVBlankRequested(event.seq)) {
      HWCCHECK(eCheckVSyncTiming);
      if (timeAfterVBlank > 25000000)  // 25ms. Enough for 48Hz panel.
      {
        int32_t ms = timeAfterVBlank / 1000000;

        HWCERROR(eCheckVSyncTiming,
                 "VSync occurred %dms after HWC called drmWaitVBlank", ms);
      }

      HWCLOGD_COND(eLogEventHandler,
                   "RaiseEventFromQueue: calling user VBlank handler");
      (mUserEvctx.vblank_handler)(event.fd, event.seq, event.sec, event.usec,
                                  (void*)event.crtc->GetVBlankUserData());
    } else {
      HWCLOGV_COND(eLogEventHandler,
                   "Discarding VBlank event CRTC %d for frame:%d as it was not "
                   "requested",
                   event.crtc->GetCrtcId(), event.seq);
    }
  } else {
    HWCERROR(eCheckInternalError, "Unsupported DRM Event type %d",
             event.eventType);
  }

  return true;
}

void DrmShimEventHandler::vblank_handler(int fd, unsigned int frame,
                                         unsigned int sec, unsigned int usec,
                                         void* data) {
  // VBlank event is handled
  mInstance->FwdVBlank(fd, frame, sec, usec, data);
}

void DrmShimEventHandler::FwdVBlank(int fd, unsigned int frame,
                                    unsigned int sec, unsigned int usec,
                                    void* data) {
  ATRACE_CALL();
  // VBlank event is handled
  void* userData;
  uint32_t crtcId = (uint32_t)(uintptr_t) data;

  HWCLOGD_COND(eLogVBlank, "DrmShimEventHandler: Real VBlank, crtc %d", crtcId);
  DrmShimCrtc* crtc = mChecks->GetCrtc(crtcId);

  if (crtc == 0) {
    HWCERROR(eCheckInternalError, "Invalid CRTC id %d in VBlank event", crtcId);
  } else {
    if (mUserEvctx.vblank_handler != 0) {
      if (crtc->IssueVBlank(frame, sec, usec, userData)) {
        {
          DrmEventData eventData;
          eventData.eventType = DrmEventData::eVBlank;
          eventData.fd = fd;
          eventData.seq = frame;
          eventData.sec = sec;
          eventData.usec = usec;
          eventData.data = (uint64_t)userData;
          eventData.crtc = crtc;

          Push(eventData);
        }
      }
    } else {
      HWCLOGV_COND(eLogEventHandler,
                   "FwdVBlank ignoring VBlank because no handler");

      crtc->SetCurrentFrame(frame);
    }

    HWCLOGV_COND(eLogEventHandler, "FwdVBlank drmShimCallback=%p",
                 drmShimCallback);
    // Callback could include a sleep at this point, so long as it is << frame
    // duration
    if (drmShimCallback != 0) {
      drmShimCallback->VSync(crtc->GetDisplayIx());
    }

    if (crtc->IsVSyncEnabled(true)) {
      // Request next event.
      drmVBlankPtr vbl = crtc->SetupVBlank();

      int ret = fpDrmWaitVBlank(fd, vbl);

      if (ret != 0) {
        HWCLOGW("DrmShimEventHandler::FwdVBlank drmWaitVBlank FAILED (%d)",
                ret);

        // disable VSync until next enabled after mode change
        crtc->EnableVSync(false);
        crtc->VBlankActive(false);
      }
    } else {
      HWCLOGD_COND(
          eLogEventHandler,
          "DrmShimEventHandler::FwdVBlank: disabled, VBlanks not forwarded");
    }
  }
}

void DrmShimEventHandler::Restore(uint32_t disp) {
  DrmShimCrtc* crtc = mChecks->GetCrtcByDisplayIx(disp);

  if (crtc) {
    if (crtc->IsVSyncEnabled(true)) {
      // Request next event.
      drmVBlankPtr vbl = crtc->SetupVBlank();

      int ret = fpDrmWaitVBlank(mDrmFd, vbl);

      if (ret != 0) {
        HWCLOGW(
            "DrmShimEventHandler::RestoreVBlank drmWaitVBlank display crtc %d "
            "FAILED (%d)",
            crtc->GetCrtcId(), ret);

        // disable VSync until next enabled after mode change
        crtc->EnableVSync(false);
        crtc->VBlankActive(false);
      } else {
        HWCLOGI("RestoreVBlank: VBlank handling restored to display %d", disp);
      }
    } else {
      HWCLOGD_COND(eLogEventHandler,
                   "DrmShimEventHandler::RestoreVBlank: disabled, VBlanks not "
                   "forwarded");
    }
  } else {
    HWCLOGW("Can't restore VSync to display %d, it doesn't exist (yet?)", disp);
  }
}

void DrmShimEventHandler::onFirstRef() {
}

bool DrmShimEventHandler::threadLoop() {
  // Handle all events
  // HWCLOGV_COND(eLogEventHandler, "DrmShimEventHandler::threadLoop: waiting
  // for event (fd %d)", mDrmFd);
  if (fpDrmHandleEvent(mDrmFd, &mRealEvctx)) {
    HWCLOGD_COND(eLogEventHandler,
                 "DrmShimEventHandler::threadLoop: event not handled");
  }
  // HWCLOGV_COND(eLogEventHandler, "DrmShimEventHandler::threadLoop: exiting
  // exitPending()=%d", exitPending());

  return true;
}
