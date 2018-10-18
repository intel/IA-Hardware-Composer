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

#include "HwchDirectPlanesTest.h"
#include "HwchDefs.h"
#include "HwchPattern.h"
#include "HwchLayers.h"
#include "HwchBufferSet.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include <math.h>

REGISTER_TEST(DirectPlanes)
Hwch::DirectPlanesTest::DirectPlanesTest(Hwch::Interface& interface)
    : Hwch::RandomTest(interface),
      mColourChoice("ColourChoice"),
      mTransformChoice("TransformChoice") {
}

Hwch::DirectPlanesTest::~DirectPlanesTest() {
}

Hwch::Layer* Hwch::DirectPlanesTest::CreateLayer(uint32_t d) {
  HWCLOGV_COND(eLogHarness, "CreateLayer for D%d", d);
  uint32_t fg = mColourChoice.Get();
  uint32_t bg = mColourChoice.Get();
  HWCLOGV_COND(eLogHarness, "Colours %d %d", fg, bg);
  Hwch::Layer* layer = new RGBALayer(MaxRel(0), MaxRel(0), 1.0,
                                     mColourChoice.Get(), mColourChoice.Get());
  HWCLOGV_COND(eLogHarness, "RGBALayer created");
  SetLayerCropDf(layer, d);
  SetLayerBlending(layer);
  SetLayerTransform(layer);

  return layer;
}

void Hwch::DirectPlanesTest::SetLayerCropDf(Hwch::Layer* layer, uint32_t d) {
  // Set layer crop and display frame such that there will be no scaling
  uint32_t width = mWidthChoice[d].Get();
  uint32_t height = mHeightChoice[d].Get();

  uint32_t cropX = Choice(0, mDw[d] - width, "cropX").Get();
  uint32_t cropY = Choice(0, mDh[d] - height, "cropY").Get();

  uint32_t dfX = Choice(0, mDw[d] - width, "dfX").Get();
  uint32_t dfY = Choice(0, mDh[d] - height, "dfY").Get();

  layer->SetCrop(LogCropRect(cropX, cropY, cropX + width, cropY + height));
  layer->SetLogicalDisplayFrame(
      LogDisplayRect(dfX, dfY, dfX + width, dfY + height));
}

void Hwch::DirectPlanesTest::SetLayerFullScreen(Hwch::Layer* layer,
                                                uint32_t d) {
  HWCVAL_UNUSED(d);

  // Set layer crop and display frame such that there will be no scaling
  layer->SetCrop(LogCropRect(0, 0, MaxRelF(0), MaxRelF(0)));
  layer->SetLogicalDisplayFrame(LogDisplayRect(0, 0, MaxRel(0), MaxRel(0)));

  // Main planes does not support rotation
  layer->SetTransform(hwcomposer::HWCTransform::kIdentity);
}

void Hwch::DirectPlanesTest::SetLayerBlending(Hwch::Layer* layer) {
  uint32_t blending = mBlendingChoice.Get();
  layer->SetBlending(blending);

  // Hardware cannot deal with plane alpha
  layer->SetPlaneAlpha(255);
}

void Hwch::DirectPlanesTest::SetLayerTransform(Hwch::Layer* layer) {
  uint32_t transform = mTransformChoice.Get();
  layer->SetTransform(transform);
}

bool Hwch::DirectPlanesTest::IsFullScreen(const Hwch::LogDisplayRect& ldr,
                                          uint32_t d) {
  return ((ldr.left.Phys(mDw[d]) == 0) && (ldr.top.Phys(mDh[d]) == 0) &&
          (ldr.right.Phys(mDw[d]) == mDw[d]) &&
          (ldr.bottom.Phys(mDh[d]) == mDh[d]));
}

int Hwch::DirectPlanesTest::RunScenario() {
  ParseOptions();

  // We full expect that rotations will not be correctly mapped to the display
  // in this test, so we'll make it a warning.
  // This is because (at time of writing) HWC does not change the rotation of an
  // existing buffer.
  SetCheckPriority(eCheckPlaneTransform, ANDROID_LOG_WARN);

  // A nice selection of colours so we can see what is going on
  mColourChoice.Add(Hwch::eRed);
  mColourChoice.Add(Hwch::eGreen);
  mColourChoice.Add(Hwch::eBlue);
  mColourChoice.Add(Hwch::eYellow);
  mColourChoice.Add(Hwch::eCyan);
  mColourChoice.Add(Hwch::ePurple);
  mColourChoice.Add(Hwch::eGrey);
  mColourChoice.Add(Hwch::eLightRed);
  mColourChoice.Add(Hwch::eLightGreen);
  mColourChoice.Add(Hwch::eLightCyan);
  mColourChoice.Add(Hwch::eLightPurple);
  mColourChoice.Add(Hwch::eLightGrey);
  mColourChoice.Add(Hwch::eDarkRed);
  mColourChoice.Add(Hwch::eDarkGreen);
  mColourChoice.Add(Hwch::eDarkBlue);
  mColourChoice.Add(Hwch::eDarkCyan);
  mColourChoice.Add(Hwch::eDarkPurple);
  mColourChoice.Add(Hwch::eDarkGrey);

  // Only use transforms which can be replicated in hardware
  mTransformChoice.Add(hwcomposer::HWCTransform::kIdentity);
  mTransformChoice.Add(hwcomposer::HWCTransform::kTransform180);

  // Not doing COVERAGE
  mBlendingChoice.Add(HWC_BLENDING_PREMULT);
  mBlendingChoice.Add(HWC_BLENDING_NONE);

  int seed = mStartSeed;
  int reseedCount = 1;  // seed first iteration

  uint32_t numFrames = GetIntParam("num_frames", 500);
  uint32_t numLayers = GetIntParam("num_layers", 3);

  uint32_t cropPeriod = GetIntParam("crop_period", 100);
  uint32_t blendingPeriod = GetIntParam("blending_period", 100);
  uint32_t transformPeriod = GetIntParam("transform_period", 10);
  uint32_t newLayerPeriod = GetIntParam("new_layer_period", 20);

  Choice changeCropChooser(0, cropPeriod, "changeCropChooser");
  Choice changeBlendingChooser(0, blendingPeriod, "changeBlendingChooser");
  Choice changeTransformChooser(0, transformPeriod, "changeTransformChooser");
  Choice removeLayerChooser(0, newLayerPeriod, "removeLayerChooser");
  Choice layerChoice(0, numLayers - 1, "layerChoice");

  uint32_t numDisplays = mInterface.NumDisplays();

  // Set up the frame
  Hwch::Frame frame(mInterface);

  for (uint32_t d = 0; d < numDisplays; ++d) {
    Hwch::Display& disp = mSystem.GetDisplay(d);

    if (disp.IsConnected()) {
      mDw[d] = disp.GetLogicalWidth();
      mDh[d] = disp.GetLogicalHeight();
      mWidthChoice[d].Setup(32, mDw[d], "WidthChoice");
      mHeightChoice[d].Setup(32, mDh[d], "HeightChoice");

      for (uint32_t ly = 0; ly < numLayers; ++ly) {
        HWCLOGV_COND(eLogHarness, "Creating D%d.%d", d, ly);
        Layer* layer = CreateLayer(d);
        frame.Add(*layer, d);
      }

      uint32_t fullScreenLayerIx = layerChoice.Get();
      SetLayerFullScreen(frame.GetLayer(fullScreenLayerIx, d), d);
    }
  }

  frame.Send();

  // Get initial composition counts
  uint32_t hwcEntryCount =
      HwcGetTestResult()->GetEvalCount(eCountHwcComposition);

  // Iterate for however long caller requested
  for (uint32_t i = 1; i < numFrames; ++i) {
    HWCLOGD_COND(eLogHarness, ">>> Frame %d <<<", i);

    // Reseed every <reseed_count> frames - so we can repeat a part of the test.
    // Must remove all existing layers, so behaviour is consistent.
    if (--reseedCount <= 0) {
      reseedCount = mClearLayersPeriod;
      Choice::Seed(seed++);
    }

    for (uint32_t d = 0; d < numDisplays; ++d) {
      if (frame.NumLayers(d) > 0) {
        // Always make sure one layer is full screen
        bool reassignFullScreen = false;

        if (removeLayerChooser.Get() == 0) {
          uint32_t ly = layerChoice.Get();
          const LogDisplayRect& ldr =
              frame.GetLayer(ly, d)->GetLogicalDisplayFrame();
          reassignFullScreen = IsFullScreen(ldr, d);
          delete frame.RemoveLayerAt(ly, d);

          Layer* layer = CreateLayer(d);
          frame.AddAt(layerChoice.Get(), *layer, d);
        }

        if (changeCropChooser.Get() == 0) {
          uint32_t ly = layerChoice.Get();
          Hwch::Layer* layer = frame.GetLayer(ly, d);
          const LogDisplayRect& ldr = layer->GetLogicalDisplayFrame();
          if (IsFullScreen(ldr, d)) {
            reassignFullScreen = true;
          }
          SetLayerCropDf(layer, d);
        }

        if (changeBlendingChooser.Get() == 0) {
          uint32_t ly = layerChoice.Get();
          Hwch::Layer* layer = frame.GetLayer(ly, d);
          SetLayerBlending(layer);
        }

        if (changeTransformChooser.Get() == 0) {
          uint32_t ly = layerChoice.Get();
          Hwch::Layer* layer = frame.GetLayer(ly, d);
          const LogDisplayRect& ldr =
              frame.GetLayer(ly, d)->GetLogicalDisplayFrame();

          // Main plane does not support transforms
          if (!IsFullScreen(ldr, d)) {
            SetLayerTransform(layer);
          }
        }

        if (reassignFullScreen) {
          uint32_t ly = layerChoice.Get();
          SetLayerFullScreen(frame.GetLayer(ly, d), d);
        }
      }
    }

    frame.Send();

    RandomEvent();
  }

  // Get final composition counts, ignoring anything done in first frame
  uint32_t hwcCount =
      HwcGetTestResult()->GetEvalCount(eCountHwcComposition) - hwcEntryCount;
  HWCCHECK(eCheckUnnecessaryComposition);
  if (hwcCount > 0) {
    HWCERROR(eCheckUnnecessaryComposition,
             "HWC used composition unnecessarily, HWC %d ", hwcCount);
  }

  HWCLOGV_COND(
      eLogHarness,
      "DirectPlanes test complete, reporting statistics and restoring state");
  if (!IsOptionEnabled(eOptBrief)) {
    printf("Hwc compositions:           %6d\n", hwcCount);
    ReportStatistics();
  }

  Tidyup();

  // Delete remaining layers
  for (uint32_t d = 0; d < MAX_DISPLAYS; ++d) {
    while (frame.NumLayers(d) > 0) {
      delete frame.RemoveLayerAt(0, d);
    }
  }

  return 0;
}
