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

#include "HwchRandomTest.h"
#include "HwchDefs.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include "HwchPngImage.h"
#include "HwchFrame.h"
#include "HwchLayers.h"

#include <math.h>

using namespace hwcomposer;

Hwch::RandomTest::RandomTest(Hwch::Interface& interface)
    : Hwch::Test(interface),
      mBoolChoice(0, 1, "mBoolChoice"),
      mScreenDisableChooser(0, 1),
      mBlankFramesChoice(2, 5),  // If number of compositions between suspend
                                 // and resume exceeds 20, we can have problems
                                 // owing to HWC buffer exhaustion
      mBlankFrameSleepUsChoice(
          0.1, 10 * HWCVAL_SEC_TO_US),  // 0.1 will be rounded down to 0
      mHotPlugChooser(0, -1, "mHotPlugChooser"),
      mEsdRecoveryChooser(0, -1, "mEsdRecoveryChooser"),
      mModeChangeChooser(0, -1, "mModeChangeChooser"),
      mModeChoice(0, -1, "mModeChoice"),
      mVideoOptimizationModeChooser(0, -1, "mVideoOptimizationModeChooser"),
      mVideoOptimizationModeChoice(0, 0, "mVideoOptimizationModeChoice"),
      mEventDelayChoice(17000000, "mEventDelayChoice"),  // 0 to 17ms
      mModeChangeDelayChoice(1, 17000000,
                             "mModeChangeDelayChoice"),         // 1 to 17 ms
      mHotPlugDelayChoice(1, 17000000, "mHotPlugDelayChoice"),  // 1 to 17ms
      mVideoOptimizationModeDelayChoice(
          1, 17000000, "mVideoOptimizationModeDelayChoice"),  // 1 to 17 ms
      mPlugged(0),
      mNumNormalLayersCreated(0),
      mNumPanelFitterLayersCreated(0),
      mNumSkipLayersCreated(0),
      mNumSuspends(0),
      mNumFencePolicySelections(0),
      mNumModeChanges(0),
      mNumExtendedModeTransitions(0),
      mNumExtendedModePanelDisables(0),
      mNumVideoOptimizationModeChanges(0),
      mNumRCLayersCreated(0),
      mNumRCLayersAuto(0),
      mNumRCLayersRC(0),
      mNumRCLayersCC_RC(0),
      mNumRCLayersHint(0) {
  mScreenRotationChoice.Add(hwcomposer::HWCRotation::kRotateNone);
  mScreenRotationChoice.Add(hwcomposer::HWCRotation::kRotate90);
  mScreenRotationChoice.Add(hwcomposer::HWCRotation::kRotate180);
  mScreenRotationChoice.Add(hwcomposer::HWCRotation::kRotate270);

  mSystem.GetKernelEventGenerator().ResetCounts();
}

Hwch::RandomTest::~RandomTest() {
}

void Hwch::RandomTest::ParseOptions() {
  mStartSeed = GetIntParam("start_seed", 1);
  mClearLayersPeriod = GetIntParam("clear_layers_period", 10);

  // Whether to disable screen rotation simulation
  mNoRotation = (GetParam("no_rotation") != 0);

  // Mean period (in frames) of screen suspend
  // Default is no suspend
  uint32_t screenDisablePeriod = GetIntParam("screen_disable_period", 0);

  // Mean period (in frames) of hotplug/unplug (if HDMI connected)
  // Default is no hot plug
  // 0 would be coninuous hot plug
  int hotPlugPeriod = GetIntParam("hot_plug_period", -1);

  // Mean period (in frames) of sending ESD recovery events
  // Default is no ESD recovery
  // 0 would be continuous ESD recovery
  int esdRecoveryPeriod = GetIntParam("esd_recovery_period", -1);

  // Hot plug/ESD burst length and interburst gap in ms
  // default burst length is effectively infinite
  // default gap between bursts is 1s
  int burstLength = GetIntParam("burst_length", INT_MAX);
  int burstInterval = GetTimeParamUs("burst_interval", 1000000);

  // Mean period (in frames) of performing a mode change request
  uint32_t modeChangePeriod = GetIntParam("mode_change_period", 0);

  // Mean period (in frames) of changes to video optimization mode
  uint32_t videoOptimizationModePeriod =
      GetIntParam("video_optimization_mode_period", 0);

  // Max delay in microseconds between request of asynchronous event, and that
  // event being triggered.
  // Negative value means only synchronous (main thread) event delivery is
  // allowed
  int maxEventDelayUs = GetTimeParamUs("event_delay", 17000);  // default 17ms

  std::string suspendMethodStr(GetStrParam("screen_disable_method"));

  // Setup random choice objects
  mScreenDisableChooser.SetMax(screenDisablePeriod - 1,
                               (screenDisablePeriod == 0));

  uint32_t continuousKernelEvent = 0;
  if (hotPlugPeriod == 0) {
    continuousKernelEvent = AsyncEvent::eHotPlug | AsyncEvent::eHotUnplug;
  } else if (hotPlugPeriod > 0) {
    // Hot plug period must be at least 2 because otherwise you get one unplug
    // and no plugs.
    mHotPlugChooser.SetMax(max(1, hotPlugPeriod - 1));
  }

  if (esdRecoveryPeriod == 0) {
    continuousKernelEvent |= AsyncEvent::eESDRecovery;
  } else if (esdRecoveryPeriod > 0) {
    mEsdRecoveryChooser.SetMax(esdRecoveryPeriod - 1);
  }

  if (continuousKernelEvent) {
   std::shared_ptr<AsyncEvent::RepeatData> repeatData =
      std::shared_ptr<AsyncEvent::RepeatData>(new AsyncEvent::RepeatData(burstLength, burstInterval));
    mSystem.AddEvent(continuousKernelEvent, 0, burstInterval, repeatData);
  }

  mModeChangeChooser.SetMax(modeChangePeriod - 1, (modeChangePeriod == 0));
  mVideoOptimizationModeChooser.SetMax(videoOptimizationModePeriod - 1,
                                       (videoOptimizationModePeriod == 0));

  // Three choices of blanking type: blank only, blank and suspend, and suspend
  // only
  // (only when suspend is enabled).
  //
  // Default method (blank) will be used if none is specified.
  if (suspendMethodStr.size() > 0) {
    if (suspendMethodStr.find("all") >= 0) {
      mBlankTypeChoice.Add(Hwch::AsyncEvent::eBlank);
      mBlankTypeChoice.Add(Hwch::AsyncEvent::eSuspend);
      mBlankTypeChoice.Add(Hwch::AsyncEvent::eSuspend |
                           Hwch::AsyncEvent::eBlank);
    } else {
      if (suspendMethodStr.find("blank") >= 0) {
        mBlankTypeChoice.Add(Hwch::AsyncEvent::eBlank);
      }

      if (suspendMethodStr.find("power") >= 0) {
        mBlankTypeChoice.Add(Hwch::AsyncEvent::eSuspend);
      }

      if (suspendMethodStr.find("both") >= 0) {
        mBlankTypeChoice.Add(Hwch::AsyncEvent::eSuspend |
                             Hwch::AsyncEvent::eBlank);
      }
    }
  } else {
    mBlankTypeChoice.Add(Hwch::AsyncEvent::eBlank);
  }

  // Are we spoofing the panel as HDMI?
  // If so we have two choices each time we want to hot plug.
  if (IsOptionEnabled(eOptSpoofNoPanel)) {
    mHotPlugDisplayTypeChoice.Add(AsyncEvent::cFixedDisplay);
    mHotPlugDisplayTypeChoice.Add(AsyncEvent::cRemovableDisplay);
  }

  // Maximum delay on suspend/resume/ESD recovery events, etc.
  // 50% of events will be synchronous.
  // The remainder will be queued with a random delay between 0 and
  // maxEventDelayUs
  mEventDelayChoice.SetMax(maxEventDelayUs);

  // For hot plugs, we don't do synchronous, not a good idea.
  mHotPlugDelayChoice.SetMax(max(0, maxEventDelayUs));

  // Also for mode changes, we don't do synchronous
  mModeChangeDelayChoice.SetMax(max(0, maxEventDelayUs));

  // Ditto video optimization mode (guessing here)
  mVideoOptimizationModeDelayChoice.SetMax(max(0, maxEventDelayUs));

  // If we are interested in modes, find out which are available.
  Hwch::Display& hdmi = mSystem.GetDisplay(HWCVAL_HDMI_DISPLAY_INDEX);

  if (hdmi.IsConnected()) {
    uint32_t modeCount = hdmi.GetModes();
    HWCLOGI_COND(eLogHarness, "HDMI connected, %d modes", modeCount);
    mModeChoice.SetMax(modeCount - 1);
  } else {
    // No HDMI attached, disable mode change
    if (mModeChangeChooser.IsEnabled()) {
      HWCERROR(eCheckHdmiReq, "HDMI not connected: can't test HDMI modes.");
    } else {
      HWCLOGI_COND(eLogHarness, "HDMI not connected.");
    }

    mModeChangeChooser.SetMax(0, false);
  }
}

void Hwch::RandomTest::ChooseScreenDisable(Hwch::Frame& frame) {
  if (mScreenDisableChooser.IsEnabled()) {
    int choice = mScreenDisableChooser.Get();
    if (choice == 0) {
      // When we blank, we should not send more than a few frames to HWC
      // otherwise it will exhaust its buffers, resulting in random eviction
      // and hence errors.
      //
      // Arguably HWC should cope better with this but SF never sends more
      // than about 3 frames between suspend and resume.
      int numBlankFrames = mBlankFramesChoice.Get();
      int blankSleepUs = int(mBlankFrameSleepUsChoice.Get());
      HWCLOGD_COND(eLogHarness,
                   "Screen disable (suspend), sending %d frames, sleeping for "
                   "%fs and resuming",
                   numBlankFrames, double(blankSleepUs) / 1000000.0);

      uint32_t blankEvent = AsyncEvent::eBlank;
      uint32_t unblankEvent = AsyncEvent::eUnblank;

      blankEvent = mBlankTypeChoice.Get();

      if (blankEvent == AsyncEvent::eSuspend) {
        unblankEvent = AsyncEvent::eResume;
      } else if (blankEvent == (AsyncEvent::eBlank | AsyncEvent::eSuspend)) {
        unblankEvent = AsyncEvent::eUnblank | AsyncEvent::eResume;
      }

      SendEvent(blankEvent, mEventDelayChoice.Get());

      for (int j = 0; j < numBlankFrames; ++j) {
        frame.Send();
        RandomEvent();
      }

      // Workaround to a HWC bug.
      // Avoid testing the case where we meet the conditions for extended mode
      // when we resume
      // since (a) this won't happen in real life
      //       (b) this causes HWC to DPMS enable the panel, which is incorrect.
      ClearVideo();

      usleep(blankSleepUs);
      SendEvent(unblankEvent, mEventDelayChoice.Get());
      ++mNumSuspends;
    }
  }
}

void Hwch::RandomTest::RandomEvent() {
// TODO implement simulate hotplug in real HWC
  if (mVideoOptimizationModeChooser.IsEnabled()) {
    if (mVideoOptimizationModeChooser.Get() == 0) {
      Display::VideoOptimizationMode videoOptimizationMode =
          (Display::VideoOptimizationMode)mVideoOptimizationModeChoice.Get();
      HWCLOGV_COND(eLogHarness, "Setting video optimization mode %d",
                   videoOptimizationMode);
      SetVideoOptimizationMode(videoOptimizationMode,
                               mVideoOptimizationModeDelayChoice.Get());
      ++mNumVideoOptimizationModeChanges;
    }
  }

  if (mEsdRecoveryChooser.IsEnabled()) {
    uint32_t choice = mEsdRecoveryChooser.Get();
    if (choice == 0) {
      SendEvent(AsyncEvent::eESDRecovery, mEventDelayChoice.Get());
    }
  }
}

void Hwch::RandomTest::Tidyup() {
  mSystem.GetKernelEventGenerator().ClearContinuous();
  mSystem.GetKernelEventGenerator().Flush();
  mSystem.GetEventGenerator().Flush();
// TODO implement simulate hotplug in real HWC
  if (GetParam("nohup")) {
    Hwch::Frame endFrame(mInterface);
    Hwch::PngImage image("sample.png");
    Hwch::PngLayer pngLayer(image);
    Hwch::Display& display = Hwch::System::getInstance().GetDisplay(0);
    uint32_t displayWidth = display.GetLogicalWidth();
    uint32_t displayHeight = display.GetLogicalHeight();
    pngLayer.SetLogicalDisplayFrame(
        LogDisplayRect(0, 0, displayWidth, displayHeight));
    endFrame.Add(pngLayer);
    endFrame.Send(500);
  }
}

void Hwch::RandomTest::ClearVideo() {
}

void Hwch::RandomTest::ReportStatistics() {
  uint32_t numHotUnplugs;
  uint32_t numEsdRecoveryEvents;
  mSystem.GetKernelEventGenerator().GetCounts(numHotUnplugs,
                                              numEsdRecoveryEvents);

  printf("Suspends:                   %6d Mode changes:               %6d\n",
         mNumSuspends, mNumModeChanges);
  printf(
      "Extended mode transitions:  %6d Ext mode panel disables:    %6d Video "
      "opt mode changes:     %6d\n",
      mNumExtendedModeTransitions, mNumExtendedModePanelDisables,
      mNumVideoOptimizationModeChanges);
  printf("Hot unplugs:                %6d Esd recovery events:        %6d\n",
         numHotUnplugs, numEsdRecoveryEvents);

  printf("\n");
}
