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

#include "HwchFlickerTests.h"
#include "HwchPattern.h"
#include "HwchLayers.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

REGISTER_TEST(Flicker1)
Hwch::Flicker1Test::Flicker1Test(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

// Alternate between full-screen 16-bit and full-screen 32-bit layers.
int Hwch::Flicker1Test::RunScenario() {
  Hwch::Frame frame(mInterface);

  // 16-bit layer
  Hwch::GameFullScreenLayer game(MaxRel(0), MaxRel(0));

  // 32-bit layer
  Hwch::RGBALayer rgba(MaxRel(0), MaxRel(0));

  for (uint32_t i = 0; i < 20; ++i) {
    frame.Add(game);
    frame.Send(10);
    frame.Remove(game);

    frame.Add(rgba);
    frame.Send(10);
    frame.Remove(rgba);
  }

  return 0;
}

REGISTER_TEST(Flicker2)
Hwch::Flicker2Test::Flicker2Test(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

// Generate Max Fifo flicker by:
//   Generate layers which result in 16- and 32-bit planes being used at once
//   Wait for >0.6ms so kernel goes into idle mode
//   Then send just a 32-bit layer to the screen.

int Hwch::Flicker2Test::RunScenario() {
  Hwch::Frame frame(mInterface);

  // 16-bit layer
  Hwch::GameFullScreenLayer game(MaxRel(0), MaxRel(0));

  // 32-bit layers
  Hwch::NavigationBarLayer nav;
  Hwch::StatusBarLayer status;

  for (uint32_t i = 0; i < 20; ++i) {
    frame.Add(game);
    frame.Add(nav);
    frame.Add(status);
    frame.Send();

    // 0.8ms delay - stimulate idle mode
    usleep(800 * 1000);

    frame.Remove(game);
    frame.Send(3);
    frame.Remove(nav);
    frame.Remove(status);
  }

  return 0;
}

REGISTER_TEST(Flicker3)
Hwch::Flicker3Test::Flicker3Test(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

// Send a 16-bit layer to the screen 3 times
// then wait >0.6ms for kernel to go into idle mode.

int Hwch::Flicker3Test::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::GameFullScreenLayer game(MaxRel(0), MaxRel(0));
  frame.Add(game);

  for (uint32_t i = 0; i < 20; ++i) {
    frame.Send(3);

    // 0.8ms delay - stimulate idle mode
    usleep(800 * 1000);
  }

  return 0;
}
