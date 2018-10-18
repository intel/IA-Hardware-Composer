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

#ifndef __HwchRandomModesTest_h__
#define __HwchRandomModesTest_h__

#include "HwchRandomTest.h"
#include "HwchLayers.h"

namespace Hwch {
class RandomModesTest : public RandomTest {
 public:
  RandomModesTest(Hwch::Interface& interface);
  virtual ~RandomModesTest();

  void ChooseExtendedMode();
  void DetermineExtendedModeExpectation();
  virtual void ClearVideo();
  virtual int RunScenario();

 private:
  Choice mExtendedModeChooser;
  MultiChoice<uint32_t> mVideoRateChoice;

  // True if full-screen video layer is showing
  bool mVideoPresent;

  // True if MDS will report video playing
  bool mVideoPlaying;

  // True if MDS will report input timeout
  bool mInputTimeout;

  // Enable flag for workaround:
  // If we say "video is playing" during mode change or resume, this
  // may cause errors in validation of extended mode state.
  // To prevent this (not particularly realistic) condition happening, we have
  // this flag to stop the harness from doing this.
  bool mAvoidVideoOnResumeOrModeChange;
  uint32_t mDontStartExtendedModeBefore;

  NV12VideoLayer* mVideoLayer;
};
}

#endif  // __HwchRandomModesTest_h__
