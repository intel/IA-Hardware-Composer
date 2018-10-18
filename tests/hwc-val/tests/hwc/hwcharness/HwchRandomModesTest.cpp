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

#include "HwchRandomModesTest.h"
#include "HwchDefs.h"
#include "HwchPattern.h"
#include "HwchLayers.h"
#include "HwchBufferSet.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include <math.h>

REGISTER_TEST(RandomModes)
Hwch::RandomModesTest::RandomModesTest(Hwch::Interface& interface)
    : Hwch::RandomTest(interface),
      mExtendedModeChooser(0, -1, "mExtendedModeChooser"),
      mVideoPresent(false),
      mVideoPlaying(false),
      mInputTimeout(false),
      mDontStartExtendedModeBefore(0) {
  mBlankFrameSleepUsChoice.SetMax(1 * HWCVAL_SEC_TO_US);
}

Hwch::RandomModesTest::~RandomModesTest() {
}

int Hwch::RandomModesTest::RunScenario() {
  ParseOptions();

  // With multiple simultaneous random events going on, HWC does not always
  // leave the panel
  // in the correct enable/disable state.
  // Reported as issues 172 and 173.
  SetCheckPriority(eCheckExtendedModePanelControl, ANDROID_LOG_WARN);

  // Too many simultaneous asynchronous events for this check to have meaning.
  SetCheckPriority(eCheckUnblankingLatency, ANDROID_LOG_WARN);

  // Small number of iterations by default, so "valhwch -all" does not take too
  // long.
  // For real testing, recommend thousands.
  uint32_t testIterations = GetIntParam("test_iterations", 20);

  int seed = mStartSeed;

  uint32_t maxFramesPerIteration = GetIntParam("max_frames_per_iteration", 100);
  Choice numFramesChoice(1, maxFramesPerIteration);  // Number of frames to send
                                                     // between each layout
                                                     // update

  uint32_t extendedModePeriod = GetIntParam("extended_mode_period", 0);
  mExtendedModeChooser.SetMax(extendedModePeriod - 1,
                              (extendedModePeriod == 0));

  // Set workaround flag
  mAvoidVideoOnResumeOrModeChange =
      (GetParam("avoid_video_on_resume_or_mode_change") != 0);

  Choice screenRotationChooser(0, 200);

  Hwch::Frame frame(mInterface);

  std::vector<Hwch::Layer*> layers;
  layers.push_back(new Hwch::WallpaperLayer);
  layers.push_back(new Hwch::StatusBarLayer);
  uint32_t videoLayerIx = layers.size();
  mVideoLayer = new Hwch::NV12VideoLayer;
  layers.push_back(mVideoLayer);
  layers.push_back(new Hwch::MenuLayer);

  Choice layerChoice(1, (1 << layers.size()) - 1, "layerChoice");

  mVideoRateChoice.Add(10);
  mVideoRateChoice.Add(15);
  mVideoRateChoice.Add(24);
  mVideoRateChoice.Add(25);
  mVideoRateChoice.Add(30);
  mVideoRateChoice.Add(50);
  mVideoRateChoice.Add(54);
  mVideoRateChoice.Add(60);

  for (uint32_t i = 0; i < testIterations; ++i) {
    HWCLOGD_COND(eLogHarness, ">>> Test Iteration %d <<<", i);

    uint32_t numDisplays = mInterface.NumDisplays();

    // Reseed every iteration - so we can repeat a part of the test.
    // Must remove all existing layers, so behaviour is consistent.
    Choice::Seed(seed++);

    for (uint32_t d = 0; d < numDisplays; ++d) {
      while (frame.NumLayers(d) > 0) {
        // Use "Remove" rather than "RemoveLayerAt" as this will destroy clones
        frame.Remove(*frame.GetLayer(0, d));
      }
    }

    uint32_t layersChosen = layerChoice.Get();
    mVideoPresent = false;

    for (uint32_t j = 0; j < layers.size(); ++j) {
      if ((layersChosen & (1 << j)) != 0) {
        frame.Add(*layers.at(j));

        if (j == videoLayerIx) {
          mVideoPresent = true;
        }
      }
    }

    DetermineExtendedModeExpectation();

    uint32_t numFrames = numFramesChoice.Get();

    for (uint32_t f = 0; f < numFrames; ++f) {
      frame.Send();

      if (!mNoRotation && (screenRotationChooser.Get() == 0)) {
        frame.RotateTo(mScreenRotationChoice.Get());
      }

      ChooseExtendedMode();
      ChooseScreenDisable(frame);
      RandomEvent();
    }
  }

  HWCLOGV_COND(eLogHarness,
               "Api test complete, reporting statistics and restoring state");
  if (!IsOptionEnabled(eOptBrief)) {
    HwcTestState::getInstance()->ReportPanelFitterStatistics(stdout);
    ReportStatistics();
  }

  SetExpectedMode(HwcTestConfig::eDontCare);
  UpdateInputState(true);
  UpdateVideoState(0, false);
  Tidyup();

  for (uint32_t i = 0; i < layers.size(); ++i) {
    delete layers.at(i);
  }

  return 0;
}

void Hwch::RandomModesTest::ChooseExtendedMode() {
  if (mExtendedModeChooser.IsEnabled()) {
    if ((HwcTestState::getInstance()->GetHwcFrame(0) >=
         mDontStartExtendedModeBefore) &&
        (mExtendedModeChooser.Get() == 0)) {
      // Perform an extended mode state change
      // Decide if we want to be in extended mode, at what video rate, and
      // whether there is input timeout
      mVideoPlaying = mVideoPresent ? mBoolChoice.Get() : false;
      uint32_t videoRate = mVideoRateChoice.Get();
      mInputTimeout = mBoolChoice.Get();

      UpdateVideoState(0, mVideoPlaying, mVideoPlaying ? videoRate : 0);
      mVideoLayer->GetPattern().SetUpdateFreq(videoRate);
      UpdateInputState(!mInputTimeout);

      DetermineExtendedModeExpectation();

      ++mNumExtendedModeTransitions;
    } else if (mVideoPlaying && !mVideoPresent) {
      mVideoPlaying = false;
      UpdateVideoState(0, mVideoPlaying, 0);
      SetExpectedMode(HwcTestConfig::eOn);
      ++mNumExtendedModeTransitions;
    }
  }
}

void Hwch::RandomModesTest::DetermineExtendedModeExpectation() {
  // Tell the shims whether to expect extended mode panel disable
  HwcTestConfig::PanelModeType oldExpect = GetExpectedMode();
  if (mVideoPresent && mVideoPlaying && mInputTimeout) {
    HWCLOGV_COND(
        eLogHarness,
        "Panel disable expected: NV12 present, video playing, input timed out");
    SetExpectedMode(HwcTestConfig::eOff);

    if (oldExpect != HwcTestConfig::eOff) {
      ++mNumExtendedModePanelDisables;
    }
  } else {
    HWCLOGV_COND(
        eLogHarness,
        "Panel disable NOT expected: NV12 %spresent, video %splaying, input %s",
        mVideoPresent ? "" : "NOT ", mVideoPlaying ? "" : "NOT ",
        mInputTimeout ? "timed out" : "active");

    SetExpectedMode(HwcTestConfig::eOn);
  }
}

void Hwch::RandomModesTest::ClearVideo() {
  if (mAvoidVideoOnResumeOrModeChange) {
    if (mVideoPlaying) {
      mVideoPlaying = false;
      SetExpectedMode(HwcTestConfig::eOn);
      UpdateVideoState(0, false);
    }

    mDontStartExtendedModeBefore = HwcTestState::getInstance()->GetHwcFrame(0) +
                                   HWCVAL_EXTENDED_MODE_CHANGE_WINDOW;
  }
}
