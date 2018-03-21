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

#ifndef __HwchTests_h__
#define __HwchTests_h__

#include "HwchTest.h"

namespace Hwch {
class BasicTest : public Test {
 public:
  BasicTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class CameraTest : public Test {
 public:
  CameraTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class DialogTest : public Test {
 public:
  DialogTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GalleryTest : public Test {
 public:
  GalleryTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GameTest : public Test {
 public:
  GameTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class HomeTest : public Test {
 public:
  HomeTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NotificationTest : public Test {
 public:
  NotificationTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NV12FullVideoTest : public OptionalTest {
 public:
  NV12FullVideoTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NetflixScaledTest : public Test {
  const float mScalingFactor = 0.1;
  const int32_t mNumToSend = 20;
  const int32_t mNumRandomSteps = 20;

  void ScaleLayer(Hwch::Layer& layer, uint32_t screenWidth,
                  uint32_t screenHeight, uint32_t step);

 public:
  NetflixScaledTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NetflixSteppedTest : public Test {
  // These arrays define a series of steps that Netflix has been
  // observed to traverse. Note, that these arrays are intended to
  // be indexed by the same value, so mWidths[i] and mHeights[i]
  // should always be defined for the same i.
  static const int32_t mNumSteps = 8;
  const float mWidths[mNumSteps] = {320.0, 384.0,  512.0,  640.0,
                                    800.0, 1024.0, 1280.0, 1920.0};
  const float mHeights[mNumSteps] = {240.0, 288.0, 384.0, 480.0,
                                     480.0, 600.0, 800.0, 1080.0};

  const int32_t mFramesToSendBeforeTransition = 100;
  const int32_t mNumRandomSteps = 20;

 public:
  NetflixSteppedTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NV12FullVideo2Test : public Test {
 public:
  NV12FullVideo2Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NV12PartVideoTest : public Test {
 public:
  NV12PartVideoTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class NV12PartVideo2Test : public Test {
 private:
  // BXT panel resolution is 1080x1920. We need a layer that will fit
  // comfortably across all devices. Use 720p (16:9) at 50% scale.
  const int32_t layerWidth = 960;
  const int32_t layerHeight = 540;
  const int32_t yOffset = 300;
  const int32_t numToSend = 30;

  void TestLayer(Hwch::Frame& frame, Hwch::Layer& layer);

 public:
  NV12PartVideo2Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

// Movie studio on the panel, and a video running on the HDMI
class MovieStudioTest : public Test {
 public:
  MovieStudioTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class PanelFitterTest : public Test {
 public:
  PanelFitterTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class FlipRotTest : public Test {
 public:
  FlipRotTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class RotationAnimationTest : public Test {
 private:
  void DoRotations(Hwch::Frame& frame, uint32_t num_rotations,
                   uint32_t num_frames_to_send);

 public:
  RotationAnimationTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class SmokeTest : public Test {
 public:
  SmokeTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class PartCompTest : public Test {
 public:
  PartCompTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class PngTest : public Test {
 public:
  PngTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class TransparencyCompositionTest : public OptionalTest {
 public:
  TransparencyCompositionTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class SkipTest : public Test {
 public:
  SkipTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class PanelFitterStressTest : public OptionalTest {
 public:
  PanelFitterStressTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class SmallDfTest : public OptionalTest {
 public:
  SmallDfTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class RenderCompressionTest : public Test {
 public:
  RenderCompressionTest(Hwch::Interface& interface);

  virtual int RunScenario();
};
}

#endif  // __HwchTests_h__
