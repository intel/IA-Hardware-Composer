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

#include "HwcTestKernel.h"
#include "HwcTestCrtc.h"
#include "HwcTestUtil.h"

#define DEBUG_CRC_READER_THREAD 0  // set to one to enable reader thread debug
#define VSYNC_RENDER_DELAY \
  2  // the number of vsyncs we delay before calling SetDisplay
#define ENABLE_SHORT_RUN_DETECTION \
  1  // account for the expected short runs which occur either side of a
     // CRCERROR

// DEBUG settings
#define ERROR_INJECTION_FREQUENCY \
  0  // set to n to inject CRC errors every n vsyncs
#define REPEATED_FRAME_INJECTION_FREQUENCY \
  0  // set to n to inject repeated frames every n CRC runs

HwcCrcReader::HwcCrcReader(HwcTestKernel *pKernel, HwcTestState *pState)
    : hwcomposer::HWCThread(0, "HwcCrcReader::HwcCrcReader"),
      mpKernel(pKernel),
      mpState(pState),
      mThreadRunning(false),
      mfCtl(mDbgfs),
      mfCrc(mDbgfs),
      mMtxCRCEnabled("HwcCrcReader.CRCEnabled"),
      mCRCCrtcId(-1),
      mCRCsSuspensionReason(CRC_SUSPEND_NOT_VALID) {
  mEnabled = false;
  Reset();
}

HwcCrcReader::~HwcCrcReader() {
  Disable();
}

bool HwcCrcReader::IsEnabled() const {
  return mEnabled;
}

void HwcCrcReader::CheckEnabledState(HwcTestCrtc *crtc) {
  if (mpState->IsCheckEnabled(eCheckCRC)) {
    HWCLOGV_COND(eLogCRC, "CRCLOG_HwcCrcReader::CheckEnabledState - called");
    if (IsEnabled()) {
      DoRenderStall(crtc);
    } else {
      if (mCRCsSuspensionReason == CRC_SUSPEND_NOT_VALID) {
        Reset();
      } else {
        ATRACE_BEGIN("CRC_SUSPENSION_OFF");
        int nonRepeatedPageFlips = mPageFlips - mRepeatedFrames;
        int additionalRuns = mCrcRuns - nonRepeatedPageFlips;
        int delta = additionalRuns - mCrcErrors;

        // going into CRC suspension can leave the counts out of sync
        if (delta) {
          // we don't want to reset everything to 0, so just tweak the counts
          if (delta > 0) {
            // a page flip wasn't counted
            HWCLOGI_COND(eLogCRC,
                         "CRCLOG_HwcCrcReader::CheckEnabledState - coming out "
                         "of suspend, tweaking page flip count by %d",
                         delta);
            mPageFlips += delta;
          } else {
            // a new CRC run wasn't counted
            HWCLOGI_COND(eLogCRC,
                         "CRCLOG_HwcCrcReader::CheckEnabledState - coming out "
                         "of suspend, tweaking CRC run count by %d",
                         -delta);
            mCrcRuns += -delta;
          }
        }

        if (mCrcRunLength < VSYNC_RENDER_DELAY) {
          // we don't want to flag a short run which was caused by going into
          // suspend
          mCrcRunLength = VSYNC_RENDER_DELAY;
          HWCLOGI_COND(eLogCRC,
                       "CRCLOG_HwcCrcReader::CheckEnabledState - coming out of "
                       "suspend, setting run length (%d)",
                       mCrcRunLength);
        }
      }

      if (!Enable()) {
        HWCLOGW(
            "CRCLOG_HwcCrcReader::CheckEnabledState - ERROR, failed to enable "
            "CRCs");
      }

      if (mCRCsSuspensionReason != CRC_SUSPEND_NOT_VALID) {
        ATRACE_END();
        mCRCsSuspensionReason = CRC_SUSPEND_NOT_VALID;
      }
    }
  } else if (IsEnabled()) {
    Disable();
  }
}

void HwcCrcReader::DoRenderStall(HwcTestCrtc *crtc) {
  HWCLOGD_COND(eLogCRC, "HwcCrcReader::DoRenderStall - called");
  if (VSYNC_RENDER_DELAY) {
    // wait for (one vsync + a little bit) less than the specified delay, the
    // page flip
    // will round it up as it has to wait for the following vsync. Minimising
    // the stall
    // here allows the maximum amount of time for SetDisplay to complete its
    // work in
    // time and flip on the desired vsync
    //
    int64_t vsync_period_ns = seconds_to_nanoseconds(1) / crtc->GetVRefresh();
    int64_t delay_ns =
        vsync_period_ns * (VSYNC_RENDER_DELAY - 1) + (vsync_period_ns >> 4);

    HWCLOGD_COND(eLogCRC,
                 "HwcCrcReader::DoRenderStall - stalling rendering for %d ms",
                 int(delay_ns / milliseconds_to_nanoseconds(1)));
    usleep(delay_ns / 1000);
  }
}

void HwcCrcReader::NotifyPageFlip(HwcTestCrtc *crtc) {
  HWCVAL_UNUSED(crtc);
  if (mEnabled) {
    ATRACE_BEGIN("CRC_NOTIFY_PF");
    if (mEnabled) {
      if (StableCRCCount()) {
        ++mPageFlips;
        HWCLOGI_COND(eLogCRC,
                     "CRCLOG_HwcCrcReader::NotifyPageFlip(%d) - in CRC run(%d)",
                     mPageFlips, mCrcRuns);
      }
    }
    ATRACE_END();
  }
}

void HwcCrcReader::SuspendCRCs(int crtcId, enum CRC_SUSPENSIONS reason,
                               bool suspend) {
  bool alreadySuspended = mCRCsSuspensionReason != CRC_SUSPEND_NOT_VALID;

  HWCLOGV_COND(eLogCRC, "HwcCrcReader::SuspendCRCs(%d, %d, %c)", crtcId, reason,
               suspend ? 'y' : 'n');
  if (mCRCCrtcId != crtcId) {
    HWCLOGI_COND(eLogCRC,
                 "HwcCrcReader::SuspendCRCs - ignoring, CRCs are on crtcId %d",
                 mCRCCrtcId);
  } else {
    if (alreadySuspended && !suspend) {
      if (reason == mCRCsSuspensionReason) {
        // there's nothing to do here: mCRCsSuspensionReason will be set to
        // CRC_SUSPEND_NOT_VALID in NotifySetDisplayEnter
        HWCLOGI_COND(eLogCRC,
                     "HwcCrcReader::SuspendCRCs - enabling CRC validation");
      } else {
        HWCLOGI_COND(eLogCRC,
                     "HwcCrcReader::SuspendCRCs - ignoring enable for %d, "
                     "suspended for %d",
                     reason, mCRCsSuspensionReason);
      }
    } else if (!alreadySuspended && suspend) {
      ATRACE_BEGIN("CRC_SUSPENSION_ON");
      HWCLOGI_COND(eLogCRC,
                   "HwcCrcReader::SuspendCRCs - disabling CRC validation");
      Disable();
      mCRCsSuspensionReason = reason;
      ATRACE_END();
    } else {
      HWCLOGI_COND(
          eLogCRC,
          "HwcCrcReader::SuspendCRCs - CRCs suspended? %c. Nothing to do",
          alreadySuspended ? 'y' : 'n');
    }
  }
}

void HwcCrcReader::Reset() {
  HWCLOGI_COND(eLogCRC, "HwcCrcReader::Reset - zeroing counters");
  mCrcs = 0;
  mCrcsOnEnable = 0;
  mCrcRuns = 0;
  mCrcRunLength = 0;

  memset(&mCrcRes, 0, sizeof mCrcRes);
  memset(&mCrcResPrev, 0, sizeof mCrcResPrev);

  mPageFlips = 0;
  mRepeatedFrames = 0;

  mCrcErrors = 0;
  mShortRuns = 0;
}

bool HwcCrcReader::Enable() {
  if (!mThreadRunning) {
    mThreadRunning = true;
  }

  int rc;

  HWCLOGI_COND(eLogCRC, "HwcCrcReader::Enable - called");
  if (ConfigurePipe()) {
    mCrcsOnEnable = mCrcs;
    mpState->SetFrameControlEnabled(true);
    mEnabled = true;
    mCRCEnabledCondition.signal();  // start the CRC reader thread

    // wait for the reader to stabilise, which takes 2 vsyncs/CRCs.
    // NB. this has the desirable effect of flushing pending page flips out of
    // the system, so
    //     that when checking for additional runs in ProcessCRC() we can expect
    //     the page flip
    //     count to be one less than the CRC run count.
    //
    {
      Hwcval::Mutex::Autolock lock(mMtxCRCEnabled);
      rc = mCRCEnabledCondition.waitRelative(mMtxCRCEnabled,
                                             milliseconds_to_nanoseconds(100));
    }
    if (rc) {
      if (rc == -ETIMEDOUT) {
        HWCLOGW("HwcCrcReader::Enable - timed out");
      } else {
        HWCLOGW(
            "HwcCrcReader::Enable - error %d waiting for condition to signal",
            rc);
      }
      Disable();
    }
  }

  return mEnabled;
}

bool HwcCrcReader::Disable(bool calledFromReaderThread) {
  int rc;

  HWCLOGI_COND(eLogCRC, "HwcCrcReader::Disable - called");
  mEnabled = false;

  if (!calledFromReaderThread) {
    HWCLOGI_COND(eLogCRC,
                 "HwcCrcReader::Disable - waiting for reader thread to exit");

    Hwcval::Mutex::Autolock lock(mMtxCRCEnabled);
    rc = mCRCEnabledCondition.waitRelative(mMtxCRCEnabled,
                                           milliseconds_to_nanoseconds(50));
    if (rc) {
      if (rc == -ETIMEDOUT) {
        HWCLOGW("HwcCrcReader::Disable - timed out");
      } else {
        HWCLOGW(
            "HwcCrcReader::Disable - error %d waiting for condition to signal",
            rc);
      }
    }
  }

  HWCLOGI_COND(eLogCRC, "HwcCrcReader::Disable - disabling CRC pipe");
  mfCtl.DisablePipe(mPipe);
  // mfCrc.Close(); // don't seem to be able to re-open

  mpState->SetFrameControlEnabled(false);
  HWCLOGI_COND(eLogCRC, "HwcCrcReader::Disable - returning");
  return true;
}

bool HwcCrcReader::ConfigurePipe() {
  int pipe = PIPE_A;
  int source = INTEL_PIPE_CRC_SOURCE_DP_C;

  if (!GetCRCSource(pipe, source)) {
    HWCLOGW("HwcCrcReader::ConfigurePipe - failed to determine CRC source");
    return false;
  }

  mPipe = (enum pipe)pipe;
  mSource = (enum intel_pipe_crc_source)source;

  HWCLOGD_COND(eLogCRC, "HwcCrcReader::ConfigurePipe - resetting CRC driver");
  mfCtl.OpenPipe();
  mfCtl.DisablePipe(mPipe);

  // if the CRC results file isn't open already, do so now
  if (mfCrc.IsOpen() && mfCrc.Pipe() != mPipe) {
    mfCrc.Close();  // switching pipes, close the previously opened CRC file
  }

  if (!mfCrc.IsOpen()) {
    if (!mfCrc.Open(mPipe)) {
      HWCLOGW("HwcCrcReader::ConfigurePipe - failed to open CRC results");
      return false;
    }
  }

  // enable CRCs on this pipe
  if (!mfCtl.EnablePipe(mPipe, mSource)) {
    HWCLOGW("HwcCrcReader::ConfigurePipe - EnablePipe failed");
    mfCrc.Close();
    return false;
  }

  return true;
}

bool HwcCrcReader::GetCRCSource(int &pipe, int &source) {
  bool found = false;
  bool foundRemovable = false;
  int numDisplays;

  source = INTEL_PIPE_CRC_SOURCE_AUTO;

  for (numDisplays = 0; numDisplays < HWCVAL_MAX_CRTCS; ++numDisplays) {
    HwcTestCrtc *crtc = mpKernel->GetHwcTestCrtcByDisplayIx(numDisplays);
    HwcTestState::DisplayType type;

    if (crtc == NULL) {
      break;
    }

    type = crtc->GetDisplayType();

    HWCLOGD_COND(
        eLogCRC,
        "HwcCrcReader::GetCRCSource - display[%d] fixed(%c) connected(%c)",
        numDisplays, type == HwcTestState::eFixed ? 'y' : 'n',
        crtc->IsBehavingAsConnected() ? 'y' : 'n');

    if (type == HwcTestState::eFixed) {
      if (!found && !foundRemovable) {
        found = true;
        pipe = numDisplays;
      }
    } else if (type == HwcTestState::eRemovable) {
      if (!foundRemovable && crtc->IsBehavingAsConnected()) {
        found = true;
        foundRemovable = true;
        pipe = numDisplays;
      }
    }
  }
  HWCLOGD_COND(eLogCRC,
               "HwcCrcReader::GetCRCSource - validating removable display? %c "
               "pipe(%c) source(%d)",
               foundRemovable ? 'y' : 'n', pipe + 'A', source);
  return true;
}

uint32_t HwcCrcReader::StableCRCCount() const {
  int count = int(mCrcs) - (mCrcsOnEnable + 2);
  if (count < 0) {
    count = 0;
  }
  return uint32_t(count);
}

bool HwcCrcReader::UpdateCRCRuns() {
  char traceString[80];
  bool continuationOfRun =
      (memcmp(&mCrcRes.crc, &mCrcResPrev.crc, sizeof mCrcRes.crc) == 0);

  if (continuationOfRun) {
    snprintf(traceString, sizeof traceString, "CRCRUN_CONTINUE(%d)", mCrcs);
    ATRACE_BEGIN(traceString);
    ++mCrcRunLength;
    HWCLOGI_COND(eLogCRC,
                 "CRCLOG_HwcCrcReader::UpdateCRCRuns(%d, %d ms) - continuation "
                 "of CRC run(%d, %d)",
                 mCrcs, int(mCrcRes.time_ns / milliseconds_to_nanoseconds(1)),
                 mCrcRuns, mCrcRunLength);
    ATRACE_END();
  } else {
    snprintf(traceString, sizeof traceString, "CRCRUN_START(%d)", mCrcs);
    ATRACE_BEGIN(traceString);

#if ENABLE_SHORT_RUN_DETECTION
    // A run of 1 always indicates an error (unless we're using a very short
    // render
    // delay of 0 or 1)
    //
    if (VSYNC_RENDER_DELAY > 1 && mCrcRunLength < VSYNC_RENDER_DELAY) {
      // a CRC error fragments a valid run into 2 or 3 (the latter if
      // VSYNC_RENDER_DELAY > 2)
      ++mShortRuns;
      HWCLOGD_COND(eLogCRC,
                   "CRCLOG_HwcCrcReader::UpdateCRCRuns(%d, %d ms) - short run "
                   "detected. Total Short Runs %d, %d of which are errors",
                   mCrcs, int(mCrcRes.time_ns / milliseconds_to_nanoseconds(1)),
                   mShortRuns, mCrcErrors);
    }
#endif  // ENABLE_SHORT_RUN_DETECTION

    ++mCrcRuns;
    mCrcRunLength = 1;

    HWCLOGI_COND(eLogCRC,
                 "CRCLOG_HwcCrcReader::UpdateCRCRuns(%d, %d ms) - start of new "
                 "CRC run(%d)",
                 mCrcs, int(mCrcRes.time_ns / milliseconds_to_nanoseconds(1)),
                 mCrcRuns);
    ATRACE_END();
  }
  return continuationOfRun;
}

void HwcCrcReader::ProcessCRC(crc_t &res) {
  mCrcRes = res;
  ++mCrcs;

  DebugCRC();

  if (DEBUG_CRC_READER_THREAD) {
    HWCLOGI_COND(eLogCRC,
                 "CRCLOG_HwcCrcReader::ProcessCRC(%d, %d ms) crc = "
                 "%08x-%08x-%08x-%08x-%08x",
                 mCrcs, int(mCrcRes.time_ns / milliseconds_to_nanoseconds(1)),
                 mCrcRes.crc[0], mCrcRes.crc[1], mCrcRes.crc[2], mCrcRes.crc[3],
                 mCrcRes.crc[4]);
  }

  if (!StableCRCCount()) {
    HWCLOGI_COND(eLogCRC,
                 "CRCLOG_HwcCrcReader::ProcessCRC(%d, %d ms) - ignoring, CRC "
                 "not stabilised)",
                 mCrcs, int(mCrcRes.time_ns / milliseconds_to_nanoseconds(1)),
                 mCrcRuns);
  } else {
    int pageFlips = mPageFlips;  // record the value now so that we're using a
                                 // consistent figure
    int nonRepeatedPageFlips;
    int additionalRuns;
    int newErrors;

    if (StableCRCCount() == 1) {
      ++mCrcRuns;
      mCrcRunLength = 1;

      HWCLOGI_COND(eLogCRC,
                   "CRCLOG_HwcCrcReader::ProcessCRC(%d, %d ms) - start of new "
                   "CRC run(%d) (the first since CRC stabilised)",
                   mCrcs, int(mCrcRes.time_ns / milliseconds_to_nanoseconds(1)),
                   mCrcRuns);

      // the enable function has been waiting for this, release it
      mCRCEnabledCondition.signal();
    } else {
      UpdateCRCRuns();
    }

    // each page flip of a non-repeated frame will result in 1 CRC run,
    // calculate how many
    // additional runs we've got
    nonRepeatedPageFlips = pageFlips - mRepeatedFrames;
    additionalRuns = mCrcRuns - nonRepeatedPageFlips;

    if (mCrcErrors == 0) {
      HWCLOGD_COND(eLogCRC,
                   "CRCLOG_HwcCrcReader::ProcessCRC(%d) - validating: runs(%d) "
                   "- uniquePFs(%d-%d=%d) = %d additional runs",
                   mCrcs, mCrcRuns, pageFlips, mRepeatedFrames,
                   nonRepeatedPageFlips, additionalRuns);
    } else {
      int additionalRunsWithoutErrors = additionalRuns;
      additionalRuns -= mCrcErrors;
      HWCLOGD_COND(
          eLogCRC,
          "CRCLOG_HwcCrcReader::ProcessCRC(%d) - validating: runs(%d) - "
          "uniquePFs(%d-%d=%d) = %d - %d errorRuns = %d additional runs",
          mCrcs, mCrcRuns, pageFlips, mRepeatedFrames, nonRepeatedPageFlips,
          additionalRunsWithoutErrors, mCrcErrors, additionalRuns);
    }

    // we expect 1 additional run to the number of unique page flips, because
    // the page flip
    // count was 0 for the first run*. Note that because of a race condition
    // between the CRC
    // and page flip counters, if the page flip notification beats the CRC, we
    // will have 0
    // additional runs, rather than 1.
    //
    // * the page flip count is always zero on the first run because the enable
    // function waits
    //   for 2-3 vsyncs for the CRC mechanism to stabilise (and during this
    //   time, page flips
    //   aren't counted). So by the time the enable function completes, any
    //   pending page flips
    //   will have been flushed from the system. Therefore the first page flip
    //   will be issued
    //   after the first CRC run has started.
    //
    if (additionalRuns > 1) {
      ATRACE_BEGIN("CRCERROR");
      // how many error runs do we have?
      newErrors = additionalRuns - 1;
      HWCLOGE_IF(newErrors > 1,
                 "CRCLOG_HwcCrcReader::ProcessCRC(%d) - ERROR unexpectedly "
                 "large number of new errors(%d)",
                 mCrcs, newErrors);

      ++mCrcErrors;
      HWCERROR(eCheckCRC,
               "CRCLOG_CRC(%d...%d) is likely culprit. Total Errors %d",
               mCrcs - 1, mCrcs, mCrcErrors);
      ATRACE_END();
    } else if (additionalRuns < 0) {
      ++mRepeatedFrames;
      HWCLOGD_COND(eLogCRC,
                   "CRCLOG_HwcCrcReader::ProcessCRC(%d) - detected a repeated "
                   "frame, total repeated frames = %d",
                   mCrcs, mRepeatedFrames);
    }
  }

  mCrcResPrev = mCrcRes;
}

void HwcCrcReader::DebugCRC() {
#if ERROR_INJECTION_FREQUENCY

  if ((mCrcs % ERROR_INJECTION_FREQUENCY) == 0) {
    if (mCrcs % 2) {
      memset(&mCrcRes.crc, 0, sizeof mCrcRes.crc);
    } else {
      memset(&mCrcRes.crc, 0xFF, sizeof mCrcRes.crc);
    }
  }

#elif REPEATED_FRAME_INJECTION_FREQUENCY
  static int repeatedFrames;
  if (!StableCRCCount()) {
    repeatedFrames = mRepeatedFrames;
  } else {
    int pageFlips = mPageFlips;
    int delta;
    bool continuationOfRun = false;
    int run = mCrcRuns;

    delta = run - (pageFlips - repeatedFrames);
    HWCLOGI_COND(eLogCRC,
                 "DrmShimCRCChecks::DebugCRC(REPEATED_FRAME_INJECTION_"
                 "FREQUENCY) - run(%d) pageFlips(%d) repeatedFrames(%d), delta "
                 "= %d",
                 run, pageFlips, repeatedFrames, delta);

    if (StableCRCCount() == 1) {
      HWCLOGD_COND(eLogCRC,
                   "DrmShimCRCChecks::DebugCRC(REPEATED_FRAME_INJECTION_"
                   "FREQUENCY) - first stable CRC, start of a new run");
    } else if (delta < 0) {
      // a new page flip arrived, ending the injected run
      ++repeatedFrames;
      HWCLOGD_COND(eLogCRC,
                   "DrmShimCRCChecks::DebugCRC(REPEATED_FRAME_INJECTION_"
                   "FREQUENCY) - repeat frame completed, repeatedFrames(%d)",
                   repeatedFrames);
      continuationOfRun = false;
    } else if (delta == 0) {
      if (run && ((run + 1) % REPEATED_FRAME_INJECTION_FREQUENCY) == 0) {
        HWCLOGI_COND(eLogCRC,
                     "DrmShimCRCChecks::DebugCRC(REPEATED_FRAME_INJECTION_"
                     "FREQUENCY) - Injecting repeat frame");
        continuationOfRun = true;
      } else {
        continuationOfRun = false;
      }

    } else if (delta == 1) {
      continuationOfRun = true;
    }

    if (continuationOfRun) {
      HWCLOGV_COND(eLogCRC,
                   "DrmShimCRCChecks::DebugCRC(REPEATED_FRAME_INJECTION_"
                   "FREQUENCY) - continuation run");
      memcpy(&mCrcRes.crc, &mCrcResPrev.crc, sizeof mCrcRes.crc);
    } else {
      HWCLOGV_COND(eLogCRC,
                   "DrmShimCRCChecks::DebugCRC(REPEATED_FRAME_INJECTION_"
                   "FREQUENCY) - Start of new run");
      if ((run + 1) % 2) {
        memset(&mCrcRes.crc, 0, sizeof mCrcRes.crc);
      } else {
        memset(&mCrcRes.crc, 0xFF, sizeof mCrcRes.crc);
      }
    }
  }
#endif
}

void HwcCrcReader::HandleRoutine() {
  HWCLOGD_COND(eLogCRC, "HwcCrcReader::HandleRoutine - starting");
    const nsecs_t ns = milliseconds_to_nanoseconds(1000);
    int rc;

    HWCLOGD_COND(eLogCRC,
                 "HwcCrcReader::HandleRoutine - waiting for CRC enable...");

    {
      Hwcval::Mutex::Autolock lock(mMtxCRCEnabled);
      rc = mCRCEnabledCondition.waitRelative(mMtxCRCEnabled, ns);
    }

      crc_t crc;

      if (!mpState->GetTestConfig().mCheckConfigs[eCheckCRC].enable) {
        // CRC checking no longer enabled. We'd like to learn of this through
        // NotifySetDisplayEnter(), but there's no guarantee that it will be
        // called during
        // the shutdown sequence (and, in fact, it seems it never is), therefore
        // we need
        // to check for the test enable flag ourselves, which will be reset by
        // HwcShimService::WaitForFrameControlFrameRelease()
        //
        HWCLOGI_COND(
            eLogCRC,
            "HwcCrcReader::threadLoop - CRC checking no longer enabled");
        Disable(true);
        return;
      }

      // perform a blocking read
      if (!mfCrc.Read(crc)) {
        // read won't block until the pipe control enables the pipe
        HWCLOGD_IF(DEBUG_CRC_READER_THREAD,
                   "HwcCrcReader::threadLoop - sleeping");
        usleep(200);
      } else {
        ATRACE_BEGIN("CRC_READ");
        ProcessCRC(crc);
        ATRACE_END();
      }
}
