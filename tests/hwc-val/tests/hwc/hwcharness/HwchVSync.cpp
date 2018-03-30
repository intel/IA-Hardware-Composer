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

#include "HwchVSync.h"
#include "HwcTestDefs.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwchDefs.h"

Hwch::VSync::VSync()
    : mDelayns(0),
      mTimeoutns(50000000),               // 50 ms default
      mRequestedVSyncPeriodns(16666667),  // default equivalent to 60Hz.
      mActive(true),
      mHaveTimer(false),
      mOffsetVSyncTime(0),
      mLastConsumedOffsetVSyncTime(0) {
  // Set default VSync delay to 6.5ms
  SetVSyncDelay(6500);
}

Hwch::VSync::~VSync() {
  DestroyTimer();
}

void Hwch::VSync::SetVSyncDelay(uint32_t delayus) {
  mDelayns = delayus * 1000;
  mActive = true;
}

void Hwch::VSync::SetTimeout(uint32_t timeoutus) {
  mTimeoutns = timeoutus * 1000;
}

void Hwch::VSync::SetRequestedVSyncPeriod(uint32_t periodus) {
  mRequestedVSyncPeriodns = periodus * 1000;
}

void Hwch::VSync::Stop() {
  mActive = false;
}

// VSync has occurred
void Hwch::VSync::Signal(uint32_t disp) {
  HWCLOGD_COND(eLogEventHandler, "Hwch::VSync::Signal(%d)", disp);

  if (disp == HwcTestState::getInstance()->GetFirstDisplayWithVSync()) {
    HWCLOGV_COND(eLogEventHandler,
                 "Hwch::VSync::Signal Triggering offset VSync from display %d",
                 disp);

    if (mDelayns == 0) {
      // No delay required, signal main thread straight away
      OffsetVSync();
    } else {
      if (!mHaveTimer) {
        struct sigevent timerEvent;
        memset(&timerEvent, 0, sizeof(timerEvent));
        timerEvent.sigev_notify = SIGEV_THREAD;
        timerEvent.sigev_notify_function = TimerHandler;
        timerEvent.sigev_value.sival_ptr = this;

        if (0 != timer_create(CLOCK_MONOTONIC, &timerEvent, &mDelayTimer)) {
          HWCERROR(eCheckInternalError, "Failed to create VSync offset timer");
        } else {
          mHaveTimer = true;
        }
      }

      // Re-set the timer
      if (mHaveTimer) {
        struct itimerspec timerSpec;
        timerSpec.it_value.tv_sec = 0;
        timerSpec.it_value.tv_nsec = mDelayns;
        timerSpec.it_interval.tv_sec =
            0;  // This is a one-hit timer so no interval
        timerSpec.it_interval.tv_nsec = 0;

        if (0 != timer_settime(mDelayTimer, 0, &timerSpec, NULL)) {
          ALOGE("Failed to reset VSync offset timer");
          DestroyTimer();
        }
      }
    }
  }
}

void Hwch::VSync::TimerHandler(sigval_t value) {
  ALOG_ASSERT(value.sival_ptr);
  static_cast<Hwch::VSync*>(value.sival_ptr)->OffsetVSync();
}

void Hwch::VSync::OffsetVSync() {
  HWCLOGV_COND(eLogEventHandler, "Hwch::Vsync::OffsetVSync");
  mOffsetVSyncTime = systemTime(SYSTEM_TIME_MONOTONIC);
  mCondition.signal();
}

void Hwch::VSync::DestroyTimer() {
  if (mHaveTimer) {
    timer_delete(mDelayTimer);
    mHaveTimer = false;
  }
}

const char* fmtTime(int64_t ns, char* str) {
  int64_t s = (ns / 1000000000);
  int32_t ms = (int32_t)((ns / 1000000) % 1000);
  int32_t us = (int32_t)((ns / 1000) % 1000);

  sprintf(str, "%" PRIi64 ".%03d.%03d", s, ms, us);
  return str;
}

void Hwch::VSync::WaitForOffsetVSync() {
  if (mActive) {
    char strbuf[30];
    char strbuf2[30];
    int64_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    int64_t timeToWait;

    int64_t timeSinceOffsetVSync = now - mOffsetVSyncTime;

    // If we seem to have stopped getting VSyncs, enforce the period we want
    // (say 17ms), otherwise wait the whole timeout period (say 50ms).
    if (timeSinceOffsetVSync > mTimeoutns) {
      timeToWait = mLastConsumedOffsetVSyncTime + mRequestedVSyncPeriodns - now;
      HWCLOGV_COND(eLogHarnessVSync, "mOffsetVsyncTime %" PRIi64
                                     " mLastConsumedOffsetVSyncTime %" PRIi64
                                     " mRequestedVSyncPeriodns %d now %" PRIi64
                                     " timeToWait %" PRIi64,
                   mOffsetVSyncTime, mLastConsumedOffsetVSyncTime,
                   mRequestedVSyncPeriodns, now, timeToWait);
    } else {
      timeToWait = mLastConsumedOffsetVSyncTime + mTimeoutns - now;
      HWCLOGV_COND(eLogHarnessVSync,
                   "mOffsetVsyncTime %" PRIi64
                   " mLastConsumedOffsetVSyncTime %" PRIi64
                   " mTimeoutns %d now %" PRIi64 " timeToWait %" PRIi64,
                   mOffsetVSyncTime, mLastConsumedOffsetVSyncTime, mTimeoutns,
                   now, timeToWait);
    }

    // If we haven't had an offset vsync since the last time we waited for one,
    // wait for the next
    if (timeToWait > 0) {
      HWCLOGV_COND(eLogHarnessVSync,
                   "Calculated wait for VSYNC up to %" PRIi64 "ns", timeToWait);
      Hwcval::Mutex::Autolock lock(mMutex);
      int st =
          mCondition.waitRelative(mMutex, uint32_t(timeToWait));
      HWCLOGV_COND(eLogEventHandler, "waitRelative %s status=%d",
                   ((st == 0) ? "OK" : "timed out"), st);
    }

    // Refresh "now" after the wait
    now = systemTime(SYSTEM_TIME_MONOTONIC);

    if (mOffsetVSyncTime <= mLastConsumedOffsetVSyncTime) {
      // No VSync since last time, Next expected VSync time should be calculated
      // relative to current time
      mLastConsumedOffsetVSyncTime = now;
    } else {
      // VSyncs are working, calculate next expected VSync time relative to the
      // last.
      mLastConsumedOffsetVSyncTime = mOffsetVSyncTime;
    }

    int64_t timeSinceHwcVSync = now - mOffsetVSyncTime;
    HWCCHECK(eCheckHwcGeneratesVSync);
    if (timeSinceHwcVSync < mTimeoutns) {
      HWCLOGD_COND(eLogHarnessVSync,
                   "Hwch::Vsync::WaitForOffsetVSync completing");
    } else {
      uint32_t displayIx =
          HwcTestState::getInstance()->GetFirstDisplayWithVSync();
      bool vblankEnabled;
      int64_t realVBlankTime =
          HwcTestState::getInstance()->GetVBlankTime(displayIx, vblankEnabled);

      if (realVBlankTime) {
        int64_t timeSinceRealVSync = now - realVBlankTime;

        HWCLOGV_COND(eLogHarnessVSync, "realVBlankTime %" PRIi64
                                       " systemTime %" PRIi64 " diff %" PRIi64,
                     realVBlankTime, now, timeSinceRealVSync);

        if (vblankEnabled) {
          if (timeSinceRealVSync < mTimeoutns) {
            HWCERROR(eCheckHwcGeneratesVSync,
                     "No VSync callback from HWC within %dms (last offset "
                     "VSync at %s, now %s)",
                     mTimeoutns / 1000000, fmtTime(mOffsetVSyncTime, strbuf),
                     fmtTime(now, strbuf2));
          }
        } else {
          HWCLOGD_COND(eLogHarnessVSync,
                       "VSync timeout because it's currently disabled at the "
                       "display level.");
        }
      }
    }
  } else {
    HWCLOGI_COND(eLogEventHandler, "WaitForOffsetVSync skipped\n");
  }
}
