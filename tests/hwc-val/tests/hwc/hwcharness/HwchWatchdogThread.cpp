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

#include "HwchWatchdogThread.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"
#include "HwcvalThreadTable.h"
#include "HwcHarness.h"
#include "HwchDefs.h"
#include "HwchFrame.h"

Hwch::WatchdogThread::WatchdogThread(HwcTestRunner* runner)
    : hwcomposer::HWCThread(PRIORITY_NORMAL, "HwchWatchdogThread"), mStartTime(0), mRunner(runner) {
  Hwcval::ValCallbacks::Set(this);
}

Hwch::WatchdogThread::~WatchdogThread() {
  HWCLOGI("WatchdogThread::~WatchdogThread()");
  requestExitAndWait();
}


void Hwch::WatchdogThread::Set(uint32_t minMinutes, float minFps) {
  mMinNs = ((int64_t)minMinutes) * 60 * HWCVAL_SEC_TO_NS;
  mMinFps = minFps;
}

void Hwch::WatchdogThread::Start() {
  mStartTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

void Hwch::WatchdogThread::Stop() {
  mStartTime = 0;
}

void Hwch::WatchdogThread::Exit() {
  ALOGE("Unrecoverable error detected. Aborting HWC harness...");
  mRunner->Lock();
  ALOGD("Runner lock obtained.");
  Hwcval::ReportThreadStates();
  mRunner->LogTestResult();
  mRunner->LogSummary();
  mRunner->WriteCsvFile();
  mRunner->CombineFiles(0);
  Hwch::System::QuickExit();
}

void Hwch::WatchdogThread::requestExitAndWait(){

  HandleWait();
  Exit();
}

void Hwch::WatchdogThread::HandleRoutine() {
  uint32_t lastFrameCount = 0;
  uint32_t noChangeCount = 0;

    // Check every minute
    sleep(60);

    if (mStartTime != 0) {
      uint64_t currentTime = systemTime(SYSTEM_TIME_MONOTONIC);
      uint64_t runTime = currentTime - mStartTime;
      HwcTestState* state = HwcTestState::getInstance();

      if (runTime > mMinNs) {
        state->ReportFrameCounts(false);
        uint32_t tooSlowDisplaysCount = 0;
        int fastestDisplay = -1;
        float fastestFps = 0;
        uint32_t fastestFrames = 0;

        for (uint32_t d = 0; d < HWCVAL_MAX_CRTCS; ++d) {
          uint32_t frames = HwcGetTestResult()->mPerDisplay[d].mFrameCount;
          float fps = (float(frames) * HWCVAL_SEC_TO_NS) / runTime;

          if (fps < mMinFps) {
            ++tooSlowDisplaysCount;

            if (fps > fastestFps) {
              fastestFps = fps;
              fastestDisplay = d;
              fastestFrames = frames;
            }
          }
        }

        if (tooSlowDisplaysCount == HWCVAL_MAX_CRTCS) {
          HWCERROR(eCheckTooSlow,
                   "Test has achieved %d frames on D%d in %d seconds (%f fps), "
                   "below minimum frame rate of %3.1f fps",
                   fastestFrames, fastestDisplay,
                   int(runTime / HWCVAL_SEC_TO_NS), double(fastestFps),
                   double(mMinFps));
          Exit();
        }
      }

      uint32_t framesNow = Hwch::Frame::GetFrameCount();

      if (framesNow == lastFrameCount) {
        ++noChangeCount;

        if (noChangeCount >= HWCH_WATCHDOG_INACTIVITY_MINUTES) {
          float fps = (float(framesNow) * HWCVAL_SEC_TO_NS) / runTime;

          HWCERROR(eCheckTooSlow,
                   "Test has achieved %d frames in %d seconds (%f fps) and no "
                   "frames for last %d minutes.",
                   framesNow, int(runTime / HWCVAL_SEC_TO_NS), double(fps),
                   HWCH_WATCHDOG_INACTIVITY_MINUTES);
          Exit();
        }
      } else {
        lastFrameCount = framesNow;
        noChangeCount = 0;
      }
    }
}
