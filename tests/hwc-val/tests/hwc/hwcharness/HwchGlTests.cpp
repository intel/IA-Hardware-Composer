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
#include "HwchGlTests.h"
#include "HwchPattern.h"
#include "HwchGlPattern.h"
#include "HwchLayers.h"
#include "HwchPngImage.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include "HwchGlInterface.h"

// Image layer class
Hwch::PngGlLayer::PngGlLayer(Hwch::PngImage& png, float updateFreq,
                             uint32_t lineColour, uint32_t bgColour,
                             bool bIgnore)
    : Hwch::Layer(png.GetName(), 0, 0, HAL_PIXEL_FORMAT_RGBA_8888) {
  PngGlPtn* ptn = new PngGlPtn(updateFreq, lineColour, bgColour, bIgnore);
  ptn->Set(png);

  // Set gralloc buffer width and height to width and height of png image
  mWidth.mValue = png.GetWidth();
  mHeight.mValue = png.GetHeight();

  SetPattern(ptn);
  SetOffset(0, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
// Gl Tests
//
///////////////////////////////////////////////////////////////////////////////////////////////

REGISTER_TEST(GlBasicLine)
Hwch::GlBasicLineTest::GlBasicLineTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicLineTest::RunScenario() {
  HWCLOGI("GlBasicTestLine:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  Hwch::Layer layer2("GlLine", screenWidth, screenHeight);
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight));
  layer2.SetPattern(new Hwch::HorizontalLineGlPtn(10.0, eGreen, eBlue));

  frame.Add(layer1);
  frame.Add(layer2);

  frame.Send(200);

  HWCLOGI("GlBasicTestLine:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicClear)
Hwch::GlBasicClearTest::GlBasicClearTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicClearTest::RunScenario() {
  HWCLOGI("GlBasicClear:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  Hwch::Layer layer2("GlClear", 600, 400);
  layer2.SetLogicalDisplayFrame(LogDisplayRect(10, 10, 200, 200));
  layer2.SetPattern(new Hwch::ClearGlPtn(10.0, eBlue, eGreen));

  frame.Add(layer1);
  frame.Add(layer2);

  frame.Send(200);

  HWCLOGI("GlBasicClear:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicTexture)
Hwch::GlBasicTextureTest::GlBasicTextureTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicTextureTest::RunScenario() {
  HWCLOGI("GlBasicTexture:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  PngImage image("sample.png");
  Hwch::PngGlLayer layer2(image, 10.0, eGreen);
  layer2.SetLogicalDisplayFrame(LogDisplayRect(250, 10, 550, 350));

  frame.Add(layer1);
  frame.Add(layer2);

  frame.Send(200);

  HWCLOGI("GlBasicTexture:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicCombo1)
Hwch::GlBasicCombo1Test::GlBasicCombo1Test(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicCombo1Test::RunScenario() {
  HWCLOGI("GlBasicCombo1:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  Hwch::Layer layer2("GlClear", 600, 400);
  layer2.SetLogicalDisplayFrame(LogDisplayRect(10, 10, 200, 200));
  layer2.SetPattern(new Hwch::ClearGlPtn(10.0, eBlue, eGreen));

  Hwch::Layer layer3("GlLine", 600, 400);
  layer3.SetLogicalDisplayFrame(LogDisplayRect(10, 250, 110, 350));
  layer3.SetPattern(new Hwch::HorizontalLineGlPtn(10.0, eGreen, eBlue));

  PngImage image("sample.png");
  Hwch::PngGlLayer layer4(image, 10.0, eGreen);
  layer4.SetLogicalDisplayFrame(LogDisplayRect(250, 10, 550, 350));

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(layer4);

  frame.Send(200);

  HWCLOGI("GlBasicCombo1:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicCombo2)
Hwch::GlBasicCombo2Test::GlBasicCombo2Test(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicCombo2Test::RunScenario() {
  HWCLOGI("GlBasicCombo2:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  PngImage image("sample.png");
  Hwch::PngGlLayer layer2(image, 10.0, eGreen);
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight / 2));

  Hwch::Layer layer3("GlClear", screenWidth, screenHeight);
  layer3.SetLogicalDisplayFrame(
      LogDisplayRect(screenWidth / 2, 0, screenWidth, screenHeight / 2));
  layer3.SetPattern(new Hwch::ClearGlPtn(10.0, eBlue, eGreen));

  Hwch::Layer layer4("GlClear2", screenWidth, screenHeight);
  layer4.SetLogicalDisplayFrame(
      LogDisplayRect(0, screenHeight / 2, screenWidth / 2, screenHeight));
  layer4.SetPattern(new Hwch::ClearGlPtn(10.0, eGreen, eBlue));

  Hwch::Layer layer5("GlLine", screenWidth, screenHeight);
  layer5.SetLogicalDisplayFrame(LogDisplayRect(
      screenWidth / 2, screenHeight / 2, screenWidth, screenHeight));
  layer5.SetPattern(new Hwch::HorizontalLineGlPtn(10.0, eGreen, eBlue));

  frame.Add(layer1);
  frame.Add(layer2);
  frame.Add(layer3);
  frame.Add(layer4);
  frame.Add(layer5);

  frame.Send(200);

  HWCLOGI("GlBasicCombo1:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicCombo3)
Hwch::GlBasicCombo3Test::GlBasicCombo3Test(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicCombo3Test::RunScenario() {
  HWCLOGI("GlBasicCombo3:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  Hwch::Layer layer2("Glline", screenWidth, screenHeight);
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight / 2));
  layer2.SetPattern(new Hwch::HorizontalLineGlPtn(10.0, eRed, eBlue));

  Hwch::Layer layer3("GlClear", screenWidth, screenHeight);
  layer3.SetLogicalDisplayFrame(
      LogDisplayRect(screenWidth / 2, 0, screenWidth, screenHeight / 2));
  layer3.SetPattern(new Hwch::ClearGlPtn(10.0, eBlue, eGreen));

  Hwch::Layer layer4("GlClear2", screenWidth, screenHeight);
  layer4.SetLogicalDisplayFrame(
      LogDisplayRect(0, screenHeight / 2, screenWidth / 2, screenHeight));
  layer4.SetPattern(new Hwch::ClearGlPtn(10.0, eGreen, eBlue));

  Hwch::Layer layer5("GlLine", screenWidth, screenHeight);
  layer5.SetLogicalDisplayFrame(LogDisplayRect(
      screenWidth / 2, screenHeight / 2, screenWidth, screenHeight));
  layer5.SetPattern(new Hwch::HorizontalLineGlPtn(10.0, eGreen, eBlue));

  frame.Add(layer1);

  frame.Add(layer2);

  frame.Add(layer3);
  frame.Add(layer4);

  frame.Add(layer5);

  frame.Send(200);

  HWCLOGI("GlBasicCombo1:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicPixelDiscard)
Hwch::GlBasicPixelDiscardTest::GlBasicPixelDiscardTest(
    Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicPixelDiscardTest::RunScenario() {
  HWCLOGI("GlBasicPixelDiscardTest:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  PngImage image("sample.png");
  Hwch::PngGlLayer layer2(image, 10.0, eGreen, 0xE02D28FF, true);
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight));

  frame.Add(layer1);
  frame.Add(layer2);

  frame.Send(200);

  HWCLOGI("GlBasicPixelDiscard:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicViewport)
Hwch::GlBasicViewportTest::GlBasicViewportTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicViewportTest::RunScenario() {
  HWCLOGI("GlBasicViewport:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  PngImage image("sample.png");
  Hwch::PngGlLayer layer2(image, 10.0, eGreen);
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight / 2));

  Hwch::Layer layer3("GlClear", screenWidth, screenHeight);
  layer3.SetLogicalDisplayFrame(
      LogDisplayRect(screenWidth / 2, 0, screenWidth, screenHeight / 2));
  layer3.SetPattern(new Hwch::ClearGlPtn(10.0, eBlue, eGreen));

  Hwch::Layer layer4("GlClear2", screenWidth, screenHeight);
  layer4.SetLogicalDisplayFrame(
      LogDisplayRect(0, screenHeight / 2, screenWidth / 2, screenHeight));
  layer4.SetPattern(new Hwch::ClearGlPtn(10.0, eGreen, eBlue));

  Hwch::Layer layer5("GlLine", screenWidth, screenHeight);
  layer5.SetLogicalDisplayFrame(LogDisplayRect(
      screenWidth / 2, screenHeight / 2, screenWidth, screenHeight));
  layer5.SetPattern(new Hwch::HorizontalLineGlPtn(10.0, eGreen, eBlue));

  frame.Add(layer1);

  frame.Add(layer2);

  frame.Add(layer3);
  frame.Add(layer4);
  frame.Add(layer5);

  frame.Send(200);

  HWCLOGI("GlBasicCombo1:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicMovingLine)
Hwch::GlBasicMovingLineTest::GlBasicMovingLineTest(Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicMovingLineTest::RunScenario() {
  HWCLOGI("GlBasicMovingLine:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  Hwch::Layer layer("aLayer", screenWidth, screenHeight);
  layer.SetLogicalDisplayFrame(LogDisplayRect(0, 0, screenWidth, screenHeight));
  layer.SetPattern(new Hwch::HorizontalLineGlPtn(50.0, eRed, eBlue));

  frame.Add(layer1);
  frame.Add(layer);
  frame.Send(200);

  HWCLOGI("GlBasicMovingLine:: Exit");

  return 0;
}

REGISTER_TEST(GlBasicPixelDiscardNOP)
Hwch::GlBasicPixelDiscardNOPTest::GlBasicPixelDiscardNOPTest(
    Hwch::Interface& interface)
    : Hwch::OptionalTest(interface) {
}

int Hwch::GlBasicPixelDiscardNOPTest::RunScenario() {
  HWCLOGI("GlBasicPixelDiscardNOPTest:: Entry");

  Hwch::Frame frame(mInterface);

  int32_t screenWidth = mSystem.GetDisplay(0).GetWidth();
  int32_t screenHeight = mSystem.GetDisplay(0).GetHeight();

  Hwch::Layer layer1("Background", screenWidth, screenHeight);
  layer1.SetPattern(new Hwch::SolidColourPtn(eWhite));

  PngImage image("sample.png");
  Hwch::PngGlLayer layer2(image, 10.0, eGreen, 0xE02D28FF, true);
  layer2.SetLogicalDisplayFrame(
      LogDisplayRect(0, 0, screenWidth, screenHeight));

  frame.Add(layer1);
  frame.Add(layer2);

  frame.Send(200);

  HWCLOGI("GlBasicPixelDiscard:: Exit");

  return 0;
}
