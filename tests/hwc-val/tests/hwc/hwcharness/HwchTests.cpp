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

#include "HwchTests.h"
#include "HwchPattern.h"
#include "HwchLayers.h"
#include "HwchPngImage.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

REGISTER_TEST(Basic)
Hwch::BasicTest::BasicTest(Hwch::Interface& interface) : Hwch::Test(interface) {
}

int Hwch::BasicTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("data", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eRed));
  Hwch::Layer layer2("Foreground Rectangle", 600, 400);
  layer2.SetLogicalDisplayFrame(LogDisplayRect(300, 200, 900, 600));
  layer2.SetPattern(new Hwch::HorizontalLinePtn(10.0, eGreen, eBlue));

  frame.Add(layer1);
  frame.Add(layer2);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(Camera)
Hwch::CameraTest::CameraTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::CameraTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::CameraLayer layer1;
  Hwch::CameraUILayer layer2;
  Hwch::NavigationBarLayer layer3;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(Dialog)
Hwch::DialogTest::DialogTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::DialogTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::WallpaperLayer layer1;
  Hwch::LauncherLayer layer2;
  Hwch::NavigationBarLayer layer3;
  Hwch::StatusBarLayer layer4;
  Hwch::DialogBoxLayer layer5;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(layer4);
  frame.Add(layer5);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(Gallery)
Hwch::GalleryTest::GalleryTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::GalleryTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::GalleryLayer layer1;
  Hwch::GalleryUILayer layer2;
  Hwch::NavigationBarLayer layer3;
  Hwch::MenuLayer layer4;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(layer4);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(Game)
Hwch::GameTest::GameTest(Hwch::Interface& interface) : Hwch::Test(interface) {
}

int Hwch::GameTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::GameFullScreenLayer layer1;
  Hwch::NavigationBarLayer layer2;
  Hwch::AdvertLayer layer3;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(Home)
Hwch::HomeTest::HomeTest(Hwch::Interface& interface) : Hwch::Test(interface) {
}

int Hwch::HomeTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::WallpaperLayer layer1;
  Hwch::LauncherLayer layer2;
  Hwch::NavigationBarLayer layer3;
  Hwch::StatusBarLayer layer4;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(layer4);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(Notification)
Hwch::NotificationTest::NotificationTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::NotificationTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::WallpaperLayer layer1;
  Hwch::LauncherLayer layer2;
  Hwch::NavigationBarLayer layer3;
  Hwch::StatusBarLayer layer4;
  Hwch::NotificationLayer layer5;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(layer4);
  frame.Add(layer5);

  frame.Send(200);
  return 0;
}

REGISTER_TEST(NV12FullVideo)
Hwch::NV12FullVideoTest::NV12FullVideoTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::NV12FullVideoTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::NV12VideoLayer layer1;

  frame.Add(layer1);
  frame.Send();

  UpdateVideoState(0, true);  // MDS says video is being played
  UpdateInputState(true, true, &frame);
  frame.Send(50);

  UpdateInputState(false, true, &frame);  // MDS says input has timed out
  frame.Send(100);

  UpdateInputState(true, true, &frame);  // MDS says display has been touched
  frame.Send(50);

  // Stop "running video" state for next test.
  UpdateVideoState(0, false);

  return 0;
}

REGISTER_TEST(NV12FullVideo2)
// Full screen video, but with a nav bar.
Hwch::NV12FullVideo2Test::NV12FullVideo2Test(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::NV12FullVideo2Test::RunScenario() {
  Hwch::Frame frame(mInterface);
  UpdateInputState(true, false);

  Hwch::NV12VideoLayer layer1;

  frame.Add(layer1);
  Hwch::NavigationBarLayer layer2;
  frame.Add(layer2);
  frame.Send();

  {
    TransparentFullScreenLayer transparent;
    frame.Add(transparent);
    frame.Send(60);

    // "Rotate" screen to all 4 orientations, twice.
    for (uint32_t r = 0; r < 8; ++r) {
      frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
      // Shrink and maintain aspect ratio

      int32_t screenWidth = mSystem.GetDisplay(0).GetLogicalWidth();
      int32_t screenHeight = mSystem.GetDisplay(0).GetLogicalHeight();

      if (screenWidth > screenHeight) {
        layer1.SetLogicalDisplayFrame(
            LogDisplayRect(0, 0, screenWidth, screenHeight));
      } else {
        int32_t h = (screenWidth * screenWidth) / screenHeight;
        int32_t o = (screenHeight - h) / 2;

        layer1.SetLogicalDisplayFrame(LogDisplayRect(0, o, screenWidth, o + h));
      }
      frame.Send(60);
    }
  }

  UpdateVideoState(0, true);  // MDS says video is being played
  UpdateInputState(true, true, &frame);

  frame.Send(50);
  UpdateInputState(false, true, &frame);  // MDS says input has timed out
  frame.Send(100);
  UpdateInputState(true, true, &frame);  // MDS says display has been touched
  frame.Send(50);

  // SimulateHotPlug(false);
  frame.Send(100);

  // SimulateHotPlug(true);
  frame.Send(500);

  // Stop "running video" state for next test.
  UpdateVideoState(0, false);
  SetExpectedMode(HwcTestConfig::eDontCare);

  return 0;
}

REGISTER_TEST(RotationAnimation)
Hwch::RotationAnimationTest::RotationAnimationTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

void Hwch::RotationAnimationTest::DoRotations(Hwch::Frame& frame,
                                              uint32_t num_rotations,
                                              uint32_t num_frames_to_send) {
  // Send unperturbed frames
  frame.Send(num_frames_to_send);

  for (uint32_t r = 0; r < num_rotations; ++r) {
    HWCLOGD(
        "RotationAnimation: Rotating panel by 90 degrees clockwise"
        "Rotation number %d of %d.",
        r + 1, num_rotations);
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90, true);
    frame.Send(num_frames_to_send);
  }

  HWCLOGD("RotationAnimation: Rotating panel by 180 degrees");
  frame.RotateBy(hwcomposer::HWCRotation::kRotate180, true);
  frame.Send(num_frames_to_send);

  HWCLOGD("RotationAnimation: Rotating panel by 270 degrees");
  frame.RotateBy(hwcomposer::HWCRotation::kRotate270, true);
  frame.Send(num_frames_to_send);
}

int Hwch::RotationAnimationTest::RunScenario() {
  uint32_t numRotations = GetIntParam("num_rotations", 4);
  uint32_t numFramesToSend = GetIntParam("num_frames_to_send", 30);

  Hwch::Frame frame(mInterface);
  Hwch::NV12VideoLayer videoLayer;
  Hwch::StatusBarLayer statusBarLayer;
  Hwch::NavigationBarLayer navBarLayer;
  Hwch::WallpaperLayer wallpaper;

  // Send a single RGBA frame. This is a WA to prevent DRM from hanging on BYT.
  frame.Clear();
  frame.Add(wallpaper);
  frame.Send(1);

  // Start the test
  frame.Clear();
  frame.Add(videoLayer);
  frame.Add(statusBarLayer);
  frame.Add(navBarLayer);

  SetExpectedMode(HwcTestConfig::eOn);
  UpdateVideoState(0, true);  // MDS says video is being played
  frame.Send(numFramesToSend);
  SetExpectedMode(HwcTestConfig::eOff);
  UpdateInputState(false);  // MDS says input has timed out

  DoRotations(frame, numRotations, numFramesToSend);

  UpdateInputState(true, true, &frame);  // MDS says display has been touched
  frame.Send(numFramesToSend);

  // Stop "running video" state for next test.
  SetExpectedMode(HwcTestConfig::eOn);
  UpdateVideoState(0, false);

  HWCLOGD("Starting presentation mode test ...");

  // Do some rotations in presentation mode
  frame.Clear();

  Hwch::WallpaperLayer presWallpaper;
  Hwch::DialogBoxLayer presDialogBox;

  frame.Add(videoLayer, 0);
  frame.Add(statusBarLayer, 0);
  frame.Add(navBarLayer, 0);
  frame.Add(presWallpaper, 1);
  frame.Add(presDialogBox, 1);
  frame.Send(numFramesToSend);

  DoRotations(frame, numRotations, numFramesToSend);

  return 0;
}

REGISTER_TEST(NV12PartVideo)
Hwch::NV12PartVideoTest::NV12PartVideoTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::NV12PartVideoTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::WallpaperLayer layer1;
  Hwch::LauncherLayer layer2;
  Hwch::NV12VideoLayer layer3;
  layer3.SetLogicalDisplayFrame(
      LogDisplayRect(MaxRel(-779), 260, MaxRel(-20),
                     260 + 460));  // Scale the video into a popout window
  Hwch::StatusBarLayer layer4;
  Hwch::NavigationBarLayer layer5;
  TransparentFullScreenLayer transparent;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(transparent);
  frame.Add(layer4);
  frame.Add(layer5);

  frame.Send(100);

  HWCLOGI("NV12PartVideoTest: removing all layers");
  frame.Remove(layer1);
  frame.Remove(layer2);
  frame.Remove(layer3);
  frame.Remove(transparent);
  frame.Remove(layer4);
  frame.Remove(layer5);

  HWCLOGI("NV12PartVideoTest: adding layers back in presentation mode");
  // Presentation mode screen 1
  frame.Add(layer1, 0);
  frame.Add(layer2, 0);
  frame.Add(layer3, 1);
  frame.Add(layer4, 0);
  frame.Add(layer5, 0);

  layer3.SetLogicalDisplayFrame(LogDisplayRect(0, 0, MaxRel(0), MaxRel(0)));
  frame.Send(100);

  return 0;
}

REGISTER_TEST(NV12PartVideo2)
Hwch::NV12PartVideo2Test::NV12PartVideo2Test(Interface& interface)
    : Hwch::Test(interface) {
}

void Hwch::NV12PartVideo2Test::TestLayer(Hwch::Frame& frame,
                                         Hwch::Layer& layer) {
  // NV12 layer with no scaling. For BXT, this should be put directly onto a
  // plane
  layer.SetLogicalDisplayFrame(
      LogDisplayRect(0, yOffset, layerWidth, layerHeight + yOffset));
  frame.Send(numToSend);

  // Downscale layer to 75% (constant aspect-ratio) - should use plane scaler
  layer.SetLogicalDisplayFrame(LogDisplayRect(0, yOffset, layerWidth * 0.75,
                                              (layerHeight * 0.75) + yOffset));
  frame.Send(numToSend);

  // Downscale layer to 40% (constant aspect-ratio) - should use composition
  layer.SetLogicalDisplayFrame(LogDisplayRect(0, yOffset, layerWidth * 0.40,
                                              (layerHeight * 0.40) + yOffset));
  frame.Send(numToSend);

  // Downscale layer to 75% (different aspect-ratio) - should use plane scaler
  layer.SetLogicalDisplayFrame(LogDisplayRect(0, yOffset, layerWidth * 0.75,
                                              (layerHeight * 0.7) + yOffset));
  frame.Send(numToSend);

  // Downscale layer to 40% (different aspect-ratio) - should use composition
  layer.SetLogicalDisplayFrame(LogDisplayRect(0, yOffset, layerWidth * 0.40,
                                              (layerHeight * 0.35) + yOffset));
  frame.Send(numToSend);
}

int Hwch::NV12PartVideo2Test::RunScenario() {
  // Check that we can run on this platform
  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  if (min(screenWidth, screenHeight) < layerWidth) {
    HWCERROR(eCheckScreenNotBigEnough,
             "Layer width (%d) is too big for panel in all rotations!",
             layerWidth);
    return 0;
  }

  // Declare a frame and some layers
  Hwch::Frame frame(mInterface);
  Hwch::NV12VideoLayer NV12Layer(layerWidth, layerHeight);
  Hwch::StatusBarLayer statusBarLayer;
  Hwch::NavigationBarLayer navBarLayer;

  // Perform test for all 4 rotations
  for (uint32_t r = 0; r < 4; ++r) {
    // Display a single partial screen NV12 layer
    if (strcmp(GetStrParam("nv12_back_of_stack", "disable"), "disable") != 0) {
      printf("Testing with NV12 layer at the back of the stack!\n");
      frame.Add(NV12Layer);
      TestLayer(frame, NV12Layer);
    }

    // Test with wallpaper
    Hwch::WallpaperLayer wallpaper;

    frame.Clear();
    frame.Add(wallpaper);
    frame.Add(NV12Layer);
    TestLayer(frame, NV12Layer);

    // Vary the NV12 in the Z-Order (4 planes on BXT)
    frame.Clear();
    frame.Add(wallpaper);
    frame.Add(NV12Layer);
    frame.Add(statusBarLayer);
    frame.Add(navBarLayer);
    TestLayer(frame, NV12Layer);

    frame.Clear();
    frame.Add(wallpaper);
    frame.Add(statusBarLayer);
    frame.Add(NV12Layer);
    frame.Add(navBarLayer);
    TestLayer(frame, NV12Layer);

    frame.Clear();
    frame.Add(wallpaper);
    frame.Add(statusBarLayer);
    frame.Add(navBarLayer);
    frame.Add(NV12Layer);
    TestLayer(frame, NV12Layer);

    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
  }

  return 0;
}

REGISTER_TEST(NetflixScaled)
// Netflix test. Dynamically scales source crop by a constant factor
Hwch::NetflixScaledTest::NetflixScaledTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

void Hwch::NetflixScaledTest::ScaleLayer(Hwch::Layer& layer,
                                         uint32_t screenWidth,
                                         uint32_t screenHeight, uint32_t step) {
  float total_steps = 1.0 / mScalingFactor;
  float scale = step / total_steps;

  ALOG_ASSERT(scale <= 1.0f);

  float scaled_width = screenWidth * scale;
  float scaled_height = screenHeight * scale;

  layer.SetCrop(Hwch::LogCropRect(0, 0, scaled_width, scaled_height));
}

int Hwch::NetflixScaledTest::RunScenario() {
  // Netflix allocates a full screen buffer for its video and
  // then dynamically adjusts the resolution of the video
  // according to network bandwidth.

  // Create a frame and a full-screen layer
  uint32_t screen_height = mSystem.GetDisplay(0).GetHeight();
  uint32_t screen_width = mSystem.GetDisplay(0).GetWidth();

  Hwch::Frame frame(mInterface);
  Hwch::NV12VideoLayer layer(screen_width, screen_height);
  frame.Add(layer);

  // Scale forwards (i.e. simulate favourable network conditions)
  float total_steps = 1.0 / mScalingFactor;
  for (int32_t i = 1; i <= total_steps; ++i) {
    ScaleLayer(layer, screen_width, screen_height, i);
    frame.Send(mNumToSend);
  }

  // Scale backwards (i.e. simulate a drop in bandwidth)
  for (int32_t i = total_steps; i; --i) {
    ScaleLayer(layer, screen_width, screen_height, i);
    frame.Send(mNumToSend);
  }

  // Scale randomly for a number of iterations
  uint32_t bucket_size = RAND_MAX / total_steps;

  for (int32_t i = 0; i < mNumRandomSteps; ++i) {
    uint32_t step = 0;
    do {
      step = (rand() / bucket_size) + 1;
    } while (step > total_steps);

    ScaleLayer(layer, screen_width, screen_height, step);
    frame.Send(mNumToSend);
  }

  return 0;
}

REGISTER_TEST(NetflixStepped)

// Netflix test. Scales source crop in steps observed by running real app.
Hwch::NetflixSteppedTest::NetflixSteppedTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
  ALOG_ASSERT(sizeof(mWidths) == sizeof(mHeights));
}

int Hwch::NetflixSteppedTest::RunScenario() {
  // Netflix allocates a full screen buffer for its video and
  // then dynamically adjusts the resolution of the video
  // according to network bandwidth.

  // Create a frame and a full-screen layer
  Hwch::Frame frame(mInterface);
  Hwch::Display& d0 = mSystem.GetDisplay(0);

  if (GetParam("portrait") == 0) {
    // Switch to landscape mode
    // On CHV, default is portrait, and at time of writing this causes
    // panel fitter scalings which result in page flip timeouts and
    // the screen going black.
    if (d0.GetLogicalWidth() < d0.GetLogicalHeight()) {
      frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    }
  } else {
    if (d0.GetLogicalWidth() > d0.GetLogicalHeight()) {
      frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    }
  }

  uint32_t fullWidth = 1920;
  uint32_t fullHeight = 1200;

  // We have a fixed set of crop sizes, so the buffer size must be fixed too.
  // On CHV, screen size is only 1200x1920, so we can't crop that to 1920x1200
  // as that would be cropping outside the buffer.
  //
  // TODO: Check if the below implementation for portrait mode devices
  // matches what Netflix actually does, and update if necessary.
  // TODO: Loop through all 4 screen orientations.

  Hwch::NV12VideoLayer layer(fullWidth, fullHeight);
  frame.Add(layer);

  uint32_t screenWidth = d0.GetLogicalWidth();
  uint32_t screenHeight = d0.GetLogicalHeight();
  uint32_t h = screenHeight;
  uint32_t y = 0;

  if (screenWidth < screenHeight) {
    h = (screenWidth * fullHeight) / fullWidth;
    y = (screenHeight - h) / 2;
  }

  // Scale forwards (i.e. simulate favourable network conditions)
  for (int32_t i = 0; i < mNumSteps; ++i) {
    layer.SetCrop(Hwch::LogCropRect(0, 0, mWidths[i], mHeights[i]));
    layer.SetLogicalDisplayFrame(LogDisplayRect(0, y, screenWidth, y + h));

    frame.Send(mFramesToSendBeforeTransition);
  }

  // Scale backwards (i.e. simulate a drop in bandwidth)
  for (int32_t i = (mNumSteps - 1); i >= 0; --i) {
    layer.SetCrop(Hwch::LogCropRect(0, 0, mWidths[i], mHeights[i]));

    frame.Send(mFramesToSendBeforeTransition);
  }

  // Scale randomly for a number of iterations
  uint32_t bucket_size = RAND_MAX / mNumSteps;

  for (int32_t i = 0; i < mNumRandomSteps; ++i) {
    int32_t step = 0;
    do {
      step = rand() / bucket_size;
    } while (step >= mNumSteps);

    layer.SetCrop(Hwch::LogCropRect(0, 0, mWidths[step], mHeights[step]));

    frame.Send(mFramesToSendBeforeTransition);
  }

  return 0;
}

REGISTER_TEST(MovieStudio)
// Full screen video, but with a nav bar.
Hwch::MovieStudioTest::MovieStudioTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::MovieStudioTest::RunScenario() {
  Hwch::Frame frame(mInterface);
  int32_t screenheight = mSystem.GetDisplay(0).GetHeight();

  // Movie Studio in Portrait mode.
  Hwch::YV12VideoLayer layer1(1920, 1080);
  layer1.SetTransform(hwcomposer::HWCTransform::kTransform270);

  layer1.SetLogicalDisplayFrame(
      LogDisplayRect(145, 27, min(790, screenheight), MaxRel(-27)));
  layer1.SetBlending(HWC_BLENDING_NONE);

  Hwch::RGBALayer layer2(MaxRel(0), MaxRel(-eNavigationBarHeight), 1.0, ePurple,
                         Alpha(eWhite, 16));
  layer2.SetTransform(hwcomposer::HWCTransform::kTransform270);
  layer2.SetCrop(
      Hwch::LogCropRect(0, 38, MaxRelF(0), MaxRelF(-eNavigationBarHeight)));
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(38, 0, MaxRel(-eNavigationBarHeight), MaxRel(0)));

  Hwch::RGBALayer layer3(38, MaxRel(0), 1.0, eRed, Alpha(eDarkGrey, 16));
  layer3.SetLogicalDisplayFrame(LogDisplayRect(0, 0, 38, MaxRel(0)));

  // This is the Nav bar. But I'm not using the NavigationBarLayer because we
  // are in portrait mode and it seems that
  // Android populates the buffer without using a rotation in the layer.
  // This is not what we would do if we flag a rotation on the whole display.
  Hwch::RGBALayer layer4(72, MaxRel(0), 1.0, eBlue, Alpha(eDarkGreen, 16));
  layer4.SetLogicalDisplayFrame(
      LogDisplayRect(MaxRel(-eNavigationBarHeight), 0, MaxRel(0), MaxRel(0)));

  frame.Add(layer1, 0);
  frame.Add(layer2, 0);
  frame.Add(layer3, 0);
  frame.Add(layer4, 0);

  Hwch::NV12VideoLayer hdmi1(1920, 1088);
  hdmi1.SetCrop(Hwch::LogCropRect(0, 0, 1920, 1080));

// Calculate the y values for the video on HDMI to ensure it will fit on any
// screen.
// For 1280x1024 case at least, this matches the original scenario.
#pragma GCC diagnostic ignored "-Wstrict-overflow"
  int32_t hdmiHeight = mSystem.GetDisplay(1).IsConnected()
                           ? mSystem.GetDisplay(1).GetHeight()
                           : 1024;
  int32_t videoHeight = 720;

  if (videoHeight > hdmiHeight) {
    videoHeight = hdmiHeight;
  }

  int32_t videoTop = 152;
  if (videoTop + videoHeight > hdmiHeight) {
    videoTop = hdmiHeight - videoHeight;

    if (videoTop < 0) {
      videoTop = 0;
      videoHeight = hdmiHeight;
    }
  }

  hdmi1.SetLogicalDisplayFrame(
      LogDisplayRect(0, videoTop, MaxRel(0), videoTop + videoHeight));
  hdmi1.SetBlending(HWC_BLENDING_NONE);
  Hwch::RGBALayer hdmi2(MaxRel(0), MaxRel(0), eDarkGreen, Alpha(eBlue, 32));
  hdmi2.SetLogicalDisplayFrame(LogDisplayRect(0, 0, MaxRel(0), MaxRel(0)));

  if (mSystem.GetDisplay(1).IsConnected()) {
    frame.Add(hdmi1, 1);
    frame.Add(hdmi2, 1);
  }

  frame.Send();

  SetExpectedMode(HwcTestConfig::eOn);
  UpdateVideoState(0, true);  // MDS says video is being played
  frame.Send(200);

  // Set the input to timed out.
  // This won't cause D0 to turn off since we are in presentation mode.
  UpdateInputState(false, false);
  frame.Send(100);

  // Resume "input active" state
  UpdateInputState(true, false);
  frame.Send(50);
  UpdateVideoState(0, false);

  for (uint32_t r = 0; r < 4; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  return 0;
}

REGISTER_TEST(PanelFitter)
Hwch::PanelFitterTest::PanelFitterTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::PanelFitterTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  uint32_t videoWidth = 1280;
  uint32_t videoHeight = 720;

  Hwch::Display& disp = mSystem.GetDisplay(0);
  if (disp.GetWidth() < disp.GetHeight()) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
  }

  uint32_t screenWidth = mSystem.GetDisplay(0).GetLogicalWidth();
  uint32_t screenHeight = mSystem.GetDisplay(0).GetLogicalHeight();

  uint32_t dfWidth = screenWidth;
  uint32_t dfHeight = screenHeight;

  if (dfHeight > dfWidth) {
    dfHeight = (screenWidth * videoHeight) / videoWidth;
  }

  double xScale = double(dfWidth) / videoWidth;
  double yScale = double(dfHeight) / videoHeight;

  if ((videoWidth == dfWidth) || (videoHeight == dfHeight)) {
    // Let's change the DF so we exercise the panel fitter
    dfWidth = (screenWidth * 0.9);
    dfHeight = (screenHeight * 0.9);
  } else if (videoWidth < dfWidth) {
    // We will upscale - must not exceed 150% though

    if ((xScale > 1.5) || (yScale > 1.5)) {
      double scale = min(1.5, min(xScale, yScale));
      dfWidth = min(uint32_t(videoWidth * scale), screenWidth);
      dfHeight = min(uint32_t(videoHeight * scale), screenHeight);
    }
  } else {
    // We will downscale, but not below 66%
    ALOG_ASSERT((xScale > 0.66) && (yScale > 0.66));
  }

  uint32_t x = (screenWidth - dfWidth) / 2;
  uint32_t y = (screenHeight - dfHeight) / 2;

  LogDisplayRect ldr(x, y, x + dfWidth, y + dfHeight);

  {
    Hwch::WallpaperLayer layer1;
    Hwch::LauncherLayer layer2;
    Hwch::NV12VideoLayer layer3(videoWidth, videoHeight);

    // Note: scaling must be 66%-150% for panel fitter to be enabled
    layer3.SetLogicalDisplayFrame(ldr);  // Scale the video
    Hwch::StatusBarLayer layer4;
    Hwch::NavigationBarLayer layer5;

    // frame.Add(layer1);
    // frame.Add(layer2);
    frame.Add(layer3);
    frame.Send(100);

    frame.Add(layer4);
    frame.Add(layer5);
    frame.Send(100);

    frame.AddBefore(&layer3, layer1);
    frame.AddBefore(&layer3, layer2);
    frame.Send(100);
  }

  // Scale factor too large, panel fitter won't be used
  Hwch::NV12VideoLayer smallVideo(1200, 600);
  smallVideo.SetLogicalDisplayFrame(ldr);
  frame.Add(smallVideo);
  frame.Send(100);
  frame.Remove(smallVideo);

  // should use panel fitter
  Hwch::NV12VideoLayer quiteBigVideo(2400, 1600);
  quiteBigVideo.SetLogicalDisplayFrame(ldr);
  frame.Add(quiteBigVideo);
  frame.Send(100);
  frame.Remove(quiteBigVideo);

  // should not use panel fitter
  Hwch::NV12VideoLayer veryBigVideo(3000, 2000);
  veryBigVideo.SetLogicalDisplayFrame(ldr);
  frame.Add(veryBigVideo);
  frame.Send(100);

  return 0;
}

REGISTER_TEST(FlipRot)
Hwch::FlipRotTest::FlipRotTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::FlipRotTest::RunScenario() {
  Hwch::Frame frame(mInterface);
  frame.SetHwcAcquireDelay(GetTimeParamUs("delay"));

  Hwch::CameraLayer layer1;
  frame.Add(layer1);
  frame.Send(30);

  for (uint32_t r = 0; r < 8; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  layer1.SetTransform(hwcomposer::HWCTransform::kReflectX);
  HWCLOGI("Camera layer FlipH");

  for (uint32_t r = 0; r < 4; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  layer1.SetTransform(hwcomposer::HWCTransform::kReflectY);
  HWCLOGI("Camera layer FlipV");

  for (uint32_t r = 0; r < 4; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  Hwch::CameraUILayer layer2;
  HWCLOGI("Adding Camera UI");
  frame.Add(layer2);

  for (uint32_t r = 0; r < 4; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  HWCLOGD("Leaving Hwch::FlipRotTest::RunScenario");
  return 0;
}

REGISTER_TEST(Smoke)
Hwch::SmokeTest::SmokeTest(Hwch::Interface& interface) : Hwch::Test(interface) {
}

#define NOT_BRIEF(X)                 \
  if (!IsOptionEnabled(eOptBrief)) { \
    X                                \
  }

int Hwch::SmokeTest::RunScenario() {
  Hwch::Frame frame(mInterface);
  int delay = GetTimeParamUs("delay");

  if (GetIntParam("invalid", 99) != 99) {
    printf("-invalid specified!\n");
  }
  if (GetFloatParam("finvalid", 99999.0) < 99999.0) {
    printf("-finvalid specified!\n");
  }
  if (strcmp(GetStrParam("sinvalid", "default"), "default") != 0) {
    printf("-sinvalid specified!\n");
  }

  // Allow specific failure to be set to test the check handling
  const char* checkToFail = GetParam("force_fail");
  if (checkToFail) {
    HwcTestCheckType check = HwcGetTestConfig()->CheckFromName(checkToFail);
    HWCERROR(check, "Failure forced by -force_fail option");
  }

  bool useSuspendResume = false;
  std::string suspendMethod(GetStrParam("screen_disable_method"));
  if (suspendMethod.find("both") >= 0) {
    useSuspendResume = true;
  }

  Hwch::WallpaperLayer layer1;
  Hwch::LauncherLayer layer2;
  Hwch::NV12VideoLayer layer3;
  layer3.SetHwcAcquireDelay(delay);
  Hwch::StatusBarLayer layer4;
  Hwch::NavigationBarLayer layer5;

  frame.Add(layer1);
  frame.Send(10);
  frame.Add(layer2);
  frame.Send(10);
  frame.Add(layer3);

  for (int32_t i = 0; i < 100; ++i) {
    layer3.SetLogicalDisplayFrame(LogDisplayRect(
        Scaled(220 + i, 1920), Scaled(260 - i, 1280),
        Scaled(220 + 758 + 2 * i, 1920),
        Scaled((int32_t)(260 + 460), 1280)));  // Scale and offset the video
    frame.Send();
  }
  layer2.SendForward();
  frame.Send(10);

  frame.Add(layer4);
  frame.Add(layer5);

  frame.Send(10);

  {
    NOT_BRIEF(printf("Menu added to screen\n");)
    Hwch::MenuLayer layer6;
    frame.Add(layer6);
    frame.Send(10);
    NOT_BRIEF(printf("Menu removed from screen\n");)
  }
  frame.Send(10);

  Hwch::GalleryLayer layer7;
  Hwch::GalleryUILayer layer8;
  frame.Add(layer7);
  frame.Add(layer8);
  NOT_BRIEF(printf("Gallery & GalleryUI added\n");)
  frame.Send(10);

  NOT_BRIEF(printf("GalleryUI sent to back\n");)
  layer8.SendToBack();
  frame.Send(10);

  Hwch::NotificationLayer layer9;
  frame.Add(layer9);
  frame.Send(10);

  if (GetParam("big_no_blank") == 0) {
    Blank(true, useSuspendResume);
    frame.Send(3);
    usleep(50000);
    Blank(false, useSuspendResume);
  }

  Hwch::DialogBoxLayer layer10;
  frame.Add(layer10);
  frame.Send(10);

  NOT_BRIEF(printf("Video brought to front\n");)
  layer3.SendToFront();
  frame.Send(10);

  NOT_BRIEF(printf("Video sent behind the dialog\n");)
  layer3.SendBackward();
  frame.Send(30);

  for (uint32_t r = 0; r < 16; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  Hwch::CameraLayer layer11;
  frame.Add(layer11);
  Hwch::CameraUILayer layer12;
  frame.Add(layer12);
  frame.Send(30);

  for (uint32_t r = 0; r < 4; ++r) {
    frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
    frame.Send(30);
  }

  // Add in acquire fence on FB target - could force fence merge
  frame.SetHwcAcquireDelay(delay);

  // This will break cloning
  // This probably is a HWC bug
  // If it isn't we could use SetExpectedMode to indicate that clone mode is not
  // expected
  // when a flipped layer is rotated
  // or simply set the expectation to eDontCare.
  layer11.SetTransform(hwcomposer::HWCTransform::kReflectX);

  for (hwcomposer::HWCRotation rot = hwcomposer::HWCRotation::kRotateNone;
       rot < hwcomposer::HWCRotation::kMaxRotate; ++rot) {
    frame.RotateTo(rot);
    frame.Send(30);
  }

  HWCLOGD("Leaving Hwch::SmokeTest::RunScenario");
  return 0;
}

REGISTER_TEST(PartComp)
Hwch::PartCompTest::PartCompTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::PartCompTest::RunScenario() {
  Hwch::Frame frame(mInterface);
  int delay = GetTimeParamUs("delay");

  Hwch::WallpaperLayer layer1;
  Hwch::LauncherLayer layer2;
  Hwch::NV12VideoLayer layer3;
  layer3.SetHwcAcquireDelay(delay);
  Hwch::StatusBarLayer layer4;
  Hwch::NavigationBarLayer layer5;

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);

  layer3.SetLogicalDisplayFrame(
      LogDisplayRect(Scaled(220 + 30, 1920), Scaled(260 - 30, 1280),
                     Scaled(220 + 758 + 2 * 30, 1920),
                     Scaled(260 + 460, 1280)));  // Scale and offset the video
  layer2.SendForward();

  frame.Add(layer4);
  frame.Add(layer5);

  Hwch::GalleryLayer layer7;
  Hwch::GalleryUILayer layer8;
  frame.Add(layer7);
  frame.Add(layer8);
  layer8.SendToBack();

  Hwch::NotificationLayer layer9;
  frame.Add(layer9);

  Hwch::DialogBoxLayer layer10;
  frame.Add(layer10);

  layer3.SendToFront();

  layer3.SendBackward();
  frame.Send(100);

  return 0;
}

REGISTER_TEST(Png)
Hwch::PngTest::PngTest(Hwch::Interface& interface) : Hwch::Test(interface) {
}

int Hwch::PngTest::RunScenario() {
  const char* filename1 = "sample.png";

  Hwch::Frame frame(mInterface);
  Hwch::PngImage image(filename1);

  if (!image.IsLoaded()) {
    HWCERROR(eCheckTestFail, "Failed reading input png file");
    return 1;
  }

  Hwch::PngLayer layer1(image, 60.0, eRed);

  layer1.SetLogicalDisplayFrame(LogDisplayRect(0, 0, MaxRel(0), MaxRel(0)));
  frame.Add(layer1);
  frame.Send(2);

  return 0;
}

#define NUM_LAYERS 15

// this function returns a random number within the (min,max) interval
// the call to srand ensures that the number is different every time
static int RandSize(int min, int max) {
  // srand((unsigned)time(0));
  return ((rand() % (max - min)) + min);
}

REGISTER_TEST(TransparencyComposition)
Hwch::TransparencyCompositionTest::TransparencyCompositionTest(
    Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::TransparencyCompositionTest::RunScenario() {
  // Ensure consistent results - so that frame n always has the same content
  // in each run.
  mSystem.SetUpdateRateFixed(true);

  const char* filename1 = "sample.png";
  PngImage image(filename1);
  if (!image.IsLoaded()) {
    HWCERROR(eCheckTestFail, "Failed reading input png file");
    return 1;
  }

  uint32_t imageWidth = image.GetWidth();
  uint32_t imageHeight = image.GetHeight();

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();
  PngLayer* layer[NUM_LAYERS];

  // int number_of_layers = GetIntParam("num_layers");

  Hwch::Frame frame(mInterface);

  for (int i = 0; i < NUM_LAYERS; i++) {
    int random_freq = (rand() % 60) + 1;  // random frequency in the range 0-60

    layer[i] = new Hwch::PngLayer(image, random_freq, eRed);

    // Decide the size of the display I want on the screen (= rectangle)
    // it must be a random number between 25% and 100% of the screen

    int min_width_value = (screenWidth * 25) / 100;  // 25% of screen width
    int max_width_value = screenWidth;
    int min_height_value = (screenHeight * 25) / 100;  // 25% of screen height
    int max_height_value = screenHeight;

    int random_display_width = RandSize(min_width_value, max_width_value);
    int random_display_height = RandSize(min_height_value, max_height_value);

    int width_left = screenWidth - random_display_width;
    int height_left = screenHeight - random_display_height;

    // Decide the random origin of the image
    int random_origin_x = RandSize(0, width_left);
    int random_origin_y = RandSize(0, height_left);

    layer[i]->SetLogicalDisplayFrame(
        LogDisplayRect(random_origin_x, random_origin_y,
                       (random_origin_x + random_display_width),
                       (random_origin_y + random_display_height)));

    // Choose a crop rectangle
    if ((i & 1) == 0) {
      uint32_t cropWidth = RandSize(imageWidth / 10, imageWidth);
      uint32_t cropHeight = RandSize(imageHeight / 10, imageHeight);

      uint32_t cropX = RandSize(0, imageWidth - cropWidth);
      uint32_t cropY = RandSize(0, imageHeight - cropHeight);

      layer[i]->SetCrop(Hwch::LogCropRect(cropX, cropY, cropX + cropWidth,
                                          cropY + cropHeight));
    }

    // Random flip/rotation
    int transform = RandSize(0, 8);
    layer[i]->SetTransform(transform);

    frame.Add(*layer[i]);

    frame.Send(50);
    frame.WaitForCompValToComplete();
  }

  frame.Send(200);

  for (int i = 0; i < NUM_LAYERS; i++) {
    delete layer[i];
    frame.Send(50);
    frame.WaitForCompValToComplete();
  }

  return 0;
}

REGISTER_TEST(Skip)
Hwch::SkipTest::SkipTest(Hwch::Interface& interface) : Hwch::Test(interface) {
}

int Hwch::SkipTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::SkipLayer skip1;
  Hwch::StatusBarLayer status;
  Hwch::NavigationBarLayer nav;
  Hwch::NV12VideoLayer video;

  frame.Add(skip1);
  frame.Add(status);
  frame.Add(nav);
  frame.Send(60);

  frame.Add(video);
  frame.Send(60);

  frame.Remove(skip1);
  Hwch::SkipLayer skip2(true);
  frame.Add(skip2);
  frame.Send(60);

  return 0;
}

REGISTER_TEST(PanelFitterStress)
// Try various deviations on equal x&y ratios to see what causes problems
Hwch::PanelFitterStressTest::PanelFitterStressTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::PanelFitterStressTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  uint32_t screenWidth = mSystem.GetDisplay(0).GetLogicalWidth();
  uint32_t screenHeight = mSystem.GetDisplay(0).GetLogicalHeight();

  // Use -source_landscape on a portrait mode device if you want the source to
  // be 16x9 aspect ratio,
  // and rotated on to the target where it will be 9x16.
  if (GetParam("source_landscape")) {
    if (screenWidth < screenHeight) {
      frame.RotateBy(hwcomposer::HWCRotation::kRotate90);
      screenWidth = mSystem.GetDisplay(0).GetLogicalWidth();
      screenHeight = mSystem.GetDisplay(0).GetLogicalHeight();
    }
  }

  uint32_t topMargin = 10;

  Hwch::RGBALayer layer1(screenWidth, screenHeight + topMargin, 60.0, eRed,
                         eGreen, ePurple);
  layer1.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight));
  frame.Add(layer1);

  frame.Send(30);

  if (screenWidth < screenHeight) {
    uint32_t tgtHeight = screenHeight;
    uint32_t tgtWidth = ((double(screenHeight) * 9.0) / 16.0) + 0.5;
    ALOG_ASSERT(tgtWidth <= screenWidth);

    uint32_t minCh = screenHeight / 1.5;

    for (uint32_t ch = minCh; ch < screenHeight; ++ch) {
      uint32_t cw = ((double(ch) * 9.0) / 16.0) + 0.5;
      layer1.SetCrop(LogCropRect(0, topMargin, cw, ch + topMargin));
      layer1.SetLogicalDisplayFrame(LogDisplayRect(0, 0, tgtWidth, tgtHeight));
      frame.Send();
    }

  } else {
    uint32_t tgtWidth = screenWidth;
    uint32_t tgtHeight = ((double(screenWidth) * 9.0) / 16.0) + 0.5;
    ALOG_ASSERT(tgtHeight <= screenHeight);

    uint32_t minCw = screenWidth / 1.5;

    for (uint32_t cw = minCw; cw < screenWidth; ++cw) {
      uint32_t ch = ((double(cw) * 9.0) / 16.0) + 0.5;
      layer1.SetCrop(LogCropRect(0, topMargin, cw, ch + topMargin));
      layer1.SetLogicalDisplayFrame(LogDisplayRect(0, 0, tgtWidth, tgtHeight));
      frame.Send();
    }
  }
  return 0;
}

REGISTER_TEST(SmallDf)
// Try various deviations on equal x&y ratios to see what causes problems
Hwch::SmallDfTest::SmallDfTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::SmallDfTest::RunScenario() {
  Hwch::Frame frame(mInterface);

  Hwch::WallpaperLayer wallpaper;
  Hwch::CameraLayer camera;
  camera.SetCrop(LogCropRect(0, 0, 5, 492));
  camera.SetLogicalDisplayFrame(LogDisplayRect(0, 0, 1200, 4));
  camera.SetTransform(hwcomposer::HWCTransform::kReflectY);
  frame.Add(wallpaper);
  frame.Add(camera);
  frame.Send(100);

  return 0;
}

REGISTER_TEST(RenderCompression)
// Try various deviations on equal x&y ratios to see what causes problems
Hwch::RenderCompressionTest::RenderCompressionTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

int Hwch::RenderCompressionTest::RunScenario() {
  // An attempt to repro VAH-287
  Hwch::Frame frame(mInterface);

  Hwch::WallpaperLayer wallpaper;
  wallpaper.SetFormat(HAL_PIXEL_FORMAT_RGBX_8888);
  wallpaper.SetCompression(Layer::eCompressionRC);
  Hwch::LauncherLayer launcher;
  launcher.SetCompression(Layer::eCompressionRC);
  Hwch::SkipLayer skip;
  skip.SetBlending(HWC_BLENDING_PREMULT);
  skip.SetCrop(LogCropRect(-0.2, -0.2, -0.8, -0.8));
  skip.SetPlaneAlpha(0x99);

  RGBALayer fg(1920, 100, 10.0, eDarkBlue, eYellow);
  fg.SetCompression(Layer::eCompressionRC);

  frame.Add(wallpaper);
  frame.Send(30);

  frame.Add(launcher);
  frame.Add(skip);
  // frame.Add(fg);

  frame.Send(30);

  return 0;
}
