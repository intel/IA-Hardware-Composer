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

#include "HwchModeTests.h"
#include "HwchPattern.h"
#include "HwchLayers.h"
#include "HwchPngImage.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

// REGISTER_TEST(VideoModes)
Hwch::VideoModesTest::VideoModesTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::VideoModesTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  bool doModeSet = true;
  Hwch::Display* display = &mSystem.GetDisplay(1);

  if (!display->IsConnected()) {
    doModeSet = false;
    display = &mSystem.GetDisplay(0);
  }

  uint32_t modeCount = display->GetModes();

  Hwch::NV12VideoLayer video;
  Hwch::WallpaperLayer wallpaper;

  // Set the video update frequency
  video.GetPattern().SetUpdateFreq(50);

  // Make sure HWC is fully started before we set mode
  frame.Add(wallpaper);
  frame.Send(10);

#define HWCVAL_RESET_PREFERRED_MODE_NOT_WORKING
#ifdef HWCVAL_RESET_PREFERRED_MODE_NOT_WORKING
  uint32_t entryMode;
  int st = display->GetCurrentMode(entryMode);
  ALOG_ASSERT(st);
#endif

  for (uint32_t m = 0; m < modeCount; ++m) {
    if (doModeSet) {
      HWCLOGD("Setting display mode %d/%d", m, modeCount);
      display->SetMode(m);
    }

    SetExpectedMode(HwcTestConfig::eOn);
    frame.Send(20);

    frame.Remove(wallpaper);
    UpdateVideoState(0, true, 50);  // MDS says video is being played
    frame.Add(video);

    frame.Send(30);
    UpdateInputState(false, true, &frame);  // MDS says input has timed out
    frame.Send(30);

    UpdateInputState(true, true, &frame);  // MDS says display has been touched
    frame.Send(20);

    frame.Remove(video);
    UpdateVideoState(0, false);
    frame.Add(wallpaper);

    frame.Send(20);
  }

  if (doModeSet) {
#ifdef HWCVAL_RESET_PREFERRED_MODE_NOT_WORKING
    HWCLOGD("Restoring entry mode");
    display->SetMode(entryMode);
#else
    HWCLOGD("Clearing display mode");
    display->ClearMode();
#endif
  }

  SetExpectedMode(HwcTestConfig::eDontCare);
  frame.Send(30);

  return 0;
}
