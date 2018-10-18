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

#include "HwcvalWatchdog.h"

Hwcval::Watchdog::Watchdog(uint64_t ns, HwcTestCheckType check, const char* str)
    : mTimeoutNs(ns),
      mHaveTimer(false),
      mRunning(false),
      mStartTime(0),
      mCheck(check),
      mMessage(str) {
}

// Only copy state, ie. start time, actual timer will not be running in the
// copy.
Hwcval::Watchdog::Watchdog(Hwcval::Watchdog& rhs)
    : mTimeoutNs(rhs.mTimeoutNs),
      mHaveTimer(false),
      mRunning(false),
      mStartTime(rhs.mStartTime),
      mCheck(rhs.mCheck) {
}

void Hwcval::Watchdog::SetMessage(const std::string& str) {
  mMessage = str;
}

void Hwcval::Watchdog::Start() {
  Stop();

  mStartTime = systemTime(SYSTEM_TIME_MONOTONIC);

  if (!mHaveTimer) {
    struct sigevent timerEvent;
    memset(&timerEvent, 0, sizeof(timerEvent));
    timerEvent.sigev_notify = SIGEV_THREAD;
    timerEvent.sigev_notify_function = TimerHandler;
    timerEvent.sigev_value.sival_ptr = this;

    if (0 != timer_create(CLOCK_MONOTONIC, &timerEvent, &mDelayTimer)) {
      HWCLOGW("Watchdog: Failed to create timer for %s", mMessage.c_str());
    } else {
      mHaveTimer = true;
      mRunning = true;
      HWCCHECK(mCheck);
    }
  }

  // Reset the timer
  if (mHaveTimer) {
    struct itimerspec timerSpec;
    timerSpec.it_value.tv_sec = mTimeoutNs / HWCVAL_SEC_TO_NS;
    timerSpec.it_value.tv_nsec = mTimeoutNs % HWCVAL_SEC_TO_NS;
    timerSpec.it_interval.tv_sec = 0;  // This is a one-hit timer so no interval
    timerSpec.it_interval.tv_nsec = 0;

    if (0 != timer_settime(mDelayTimer, 0, &timerSpec, NULL)) {
      ALOGE("Watchdog: Failed to set timer for %s", mMessage.c_str());
      Stop();
    }
  }
}

void Hwcval::Watchdog::StartIfNotRunning() {
  if (!mRunning) {
    Start();
  }
}

void Hwcval::Watchdog::TimerHandler(sigval_t value) {
  ALOG_ASSERT(value.sival_ptr);
  static_cast<Hwcval::Watchdog*>(value.sival_ptr)->TimerHandler();
}

void Hwcval::Watchdog::TimerHandler() {
  mRunning = false;
  HWCERROR(mCheck, "%s timed out after %fms. Start time %f", mMessage.c_str(),
           (double(mTimeoutNs) / HWCVAL_MS_TO_NS),
           double(mStartTime) / HWCVAL_SEC_TO_NS);
}

void Hwcval::Watchdog::Stop() {
  if (mHaveTimer) {
    HWCLOGV_COND(eLogEventHandler, "%s: Cancelled after %fms",
                 mMessage.c_str(),
                 double(systemTime(SYSTEM_TIME_MONOTONIC) - mStartTime) /
                     HWCVAL_MS_TO_NS);
    timer_delete(mDelayTimer);
    mHaveTimer = false;
    mRunning = false;
  }
}
