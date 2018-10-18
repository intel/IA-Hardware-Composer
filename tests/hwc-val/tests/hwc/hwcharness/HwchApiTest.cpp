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

#include "HwchApiTest.h"
#include "HwchDefs.h"
#include "HwchPattern.h"
#include "HwchLayers.h"
#include "HwchBufferSet.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include <math.h>

REGISTER_TEST(Api)
Hwch::ApiTest::ApiTest(Hwch::Interface& interface)
    : Hwch::RandomTest(interface),
      mPatternMgr(mSystem.GetPatternMgr()),
      mTransformChoice(0, 7, "mTransformChoice"),
      mHwcAcquireDelayChoice(0, 15, "mHwcAcquireDelayChoice"),
      mBlankTypeChoice("mBlankTypeChoice"),
      mTileChoice("mTileChoice"),
      mRCChoice("mRCChoice"),
      mSkipChoice(0, 99, "mSkipChoice"),
      mMinBufWidth(1),
      mMinBufHeight(1),
      mMinCropWidth(1),
      mMinCropHeight(1),
      mMinDisplayFrameWidth(1),
      mMinDisplayFrameHeight(1),
      mScreenIsRotated90(false),
      mNoNV12(false)
      {
}

void Hwch::ApiTest::SetLayerBlending(Hwch::Layer* layer) {
  uint32_t blending = mBlendingChoice.Get();
  layer->SetBlending(blending);

  // If we are HWC_BLENDING_NONE, this value should have no effect.
  layer->SetPlaneAlpha(mAlphaChoice.Get());
}

void Hwch::ApiTest::SetLayerCrop(Hwch::Layer* layer, uint32_t format,
                                 uint32_t bufferWidth, uint32_t bufferHeight) {
  float cropWidth =
      BufferSizeChoice(mScreenLogWidth, mMinCropWidth, bufferWidth).Get();
  float cropHeight =
      BufferSizeChoice(mScreenLogHeight, mMinCropHeight, bufferHeight).Get();
  float cropX = CropAlignmentChoice(bufferWidth, cropWidth).Get();
  float cropY = CropAlignmentChoice(bufferHeight, cropHeight).Get();
  EnforceMinCrop(format, bufferWidth, bufferHeight, cropWidth, cropHeight);
  SetLayerCropInsideBuffer(layer, cropX, cropY, cropWidth, cropHeight,
                           bufferWidth, bufferHeight);
}

void Hwch::ApiTest::SetLayerCropInsideBuffer(Hwch::Layer* layer, float cropX,
                                             float cropY, float cropWidth,
                                             float cropHeight,
                                             uint32_t bufferWidth,
                                             uint32_t bufferHeight) {
  float cropXMax = min(cropX + cropWidth, float(bufferWidth));
  float cropYMax = min(cropY + cropHeight, float(bufferHeight));
  float cropXupd = max(float(0), cropXMax - cropWidth);
  float cropYupd = max(float(0), cropYMax - cropHeight);
  HWCLOGD_COND(
      eLogHarness, "SetLayerCropInsideBuffer (%f,%f) %fx%f -> (%f,%f,%f,%f)",
      double(cropX), double(cropY), double(cropWidth), double(cropHeight),
      double(cropXupd), double(cropYupd), double(cropXMax), double(cropYMax));

  layer->SetCrop(LogCropRect(cropXupd, cropYupd, cropXMax, cropYMax));
}

void Hwch::ApiTest::SetLayerDisplayFrame(Hwch::Layer* layer) {
  uint32_t transform = mTransformChoice.Get();

  Coord<int32_t> dfWidth;
  Coord<int32_t> dfHeight;
  Coord<int32_t> dfX;
  Coord<int32_t> dfY;

  float cropWidth =
      layer->GetCrop()
          .Width();  // NB this means that absolute co-ordinates must be used
  float cropHeight = layer->GetCrop().Height();

  uint32_t lsCropWidth;  // logical screen crop width
  uint32_t lsCropHeight;

  if (transform & hwcomposer::HWCTransform::kTransform90) {
    lsCropWidth = cropHeight;
    lsCropHeight = cropWidth;
  } else {
    lsCropWidth = cropWidth;
    lsCropHeight = cropHeight;
  }

  if (mDisplayFrameInsideScreen) {
    uint32_t minWidth = min<uint32_t>(
        mScreenLogWidth,
        max<uint32_t>(mMinDisplayFrameWidth, mMinLayerScale * lsCropWidth));
    uint32_t maxWidth =
        min<uint32_t>(mScreenLogWidth, mMaxLayerScale * lsCropWidth);
    uint32_t minHeight = min<uint32_t>(
        mScreenLogHeight,
        max<uint32_t>(mMinDisplayFrameHeight, mMinLayerScale * lsCropHeight));
    uint32_t maxHeight =
        min<uint32_t>(mScreenLogHeight, mMaxLayerScale * lsCropHeight);
    OnScreenDisplayFrameChoice dfXChoice(mScreenLogWidth, lsCropWidth, minWidth,
                                         maxWidth);
    OnScreenDisplayFrameChoice dfYChoice(mScreenLogHeight, lsCropHeight,
                                         minHeight, maxHeight);

    dfWidth = dfXChoice.Get();
    dfX = dfXChoice.GetOffset();
    dfHeight = dfYChoice.Get();
    dfY = dfYChoice.GetOffset();
  } else {
    FullDisplayFrameChoice dfXChoice(mScreenLogWidth, lsCropWidth,
                                     mMinDisplayFrameWidth, mMaxBufWidth);
    FullDisplayFrameChoice dfYChoice(mScreenLogHeight, lsCropHeight,
                                     mMinDisplayFrameHeight, mMaxBufHeight);

    dfWidth = dfXChoice.Get();
    dfX = dfXChoice.GetOffset();
    dfHeight = dfYChoice.Get();
    dfY = dfYChoice.GetOffset();
  }

  layer->SetTransform(transform);

  Coord<int32_t> dfRight = dfX + dfWidth;
  Coord<int32_t> dfBottom = dfY + dfHeight;
  layer->SetLogicalDisplayFrame(LogDisplayRect(dfX, dfY, dfRight, dfBottom));
}

Hwch::Layer* Hwch::ApiTest::CreatePFLayerInternal(const char* name,
                                                  uint32_t format,
                                                  uint32_t layerIndex) {
  uint32_t transform = mTransformChoice.Get();
  bool reallyRotated90 =
      ((transform & hwcomposer::HWCTransform::kTransform90) != 0);
  if (mScreenIsRotated90) {
    reallyRotated90 = !reallyRotated90;  // C++ has no logical XOR operator
  }

  Coord<int32_t> dfWidth;
  Coord<int32_t> dfHeight;
  Coord<int32_t> dfX;
  Coord<int32_t> dfY;

  int32_t minX;
  int32_t minY;
  int32_t maxX;
  int32_t maxY;

  int32_t dfWidthInSrc;
  int32_t dfHeightInSrc;
  uint32_t screenLogWidthInSrc;
  uint32_t screenLogHeightInSrc;
  float xscale;
  float yscale;

  HWCLOGD_COND(eLogHarness,
               "screenIsRotated90 %d transform %d reallyRotated90 %d",
               mScreenIsRotated90, transform, reallyRotated90);

  if (reallyRotated90) {
    screenLogWidthInSrc = mScreenHeight;
    screenLogHeightInSrc = mScreenWidth;
    yscale = mPanelFitterScale;
    xscale = mPanelFitterScaleChoice.GetY();
  } else {
    screenLogWidthInSrc = mScreenWidth;
    screenLogHeightInSrc = mScreenHeight;
    xscale = mPanelFitterScale;
    yscale = mPanelFitterScaleChoice.GetY();
  }

  if (mScreenIsRotated90) {
    mPanelFitterScaleChoice.GetDisplayFrameBounds(minY, minX, maxY, maxX);
  } else {
    mPanelFitterScaleChoice.GetDisplayFrameBounds(minX, minY, maxX, maxY);
  }

  HWCLOGD_COND(eLogHarness, "DisplayFrameBounds (%d, %d, %d, %d)", minX, minY,
               maxX, maxY);

  if ((layerIndex == 0) && ((minX >= 0) || (minY >= 0))) {
    // Back layer when we have decided to use letterbox or pillarbox mode
    dfX = Scaled(max(minX, 0), mScreenLogWidth);
    dfY = Scaled(max(minY, 0), mScreenLogHeight);
    dfWidth =
        Scaled((minX >= 0) ? (maxX - minX) : mScreenLogWidth, mScreenLogWidth);
    dfHeight = Scaled((minY >= 0) ? (maxY - minY) : mScreenLogHeight,
                      mScreenLogHeight);
  } else {
    if (minX < 0) {
      // Auto mode
      OnScreenDisplayFrameChoice dfXChoice(
          mScreenLogWidth, 0, mMinPFDisplayFrameWidth, mScreenLogWidth);
      dfWidth = dfXChoice.Get();
      dfX = dfXChoice.GetOffset();
    } else {
      // Letterbox or pillarbox mode
      HWCLOGD_COND(eLogHarness, "mMinPFDisplayFrameWidth %d minX %d maxX %d",
                   mMinPFDisplayFrameWidth, minX, maxX);
      int32_t w =
          Choice(mMinPFDisplayFrameWidth, maxX - minX, "letter/pillar w").Get();
      dfWidth = Scaled(w, mScreenLogWidth);
      HWCLOGD_COND(eLogHarness, "dfWidth=%d", w);
      dfX = Scaled(Choice(minX, maxX - w, "letter/pillar x").Get(),
                   mScreenLogWidth);
    }

    if (minY < 0) {
      OnScreenDisplayFrameChoice dfYChoice(
          mScreenLogHeight, 0, mMinPFDisplayFrameHeight, mScreenLogHeight);
      dfHeight = dfYChoice.Get();
      dfY = dfYChoice.GetOffset();
    } else {
      HWCLOGD_COND(eLogHarness, "mMinPFDisplayFrameHeight %d minY %d maxY %d",
                   mMinPFDisplayFrameHeight, minY, maxY);
      int32_t h = Choice(mMinPFDisplayFrameHeight, maxY - minY,
                         "letter/pillar h").Get();
      dfHeight = Scaled(h, mScreenLogHeight);
      HWCLOGD_COND(eLogHarness, "dfHeight=%d", h);
      dfY = Scaled(Choice(minY, maxY - h, "letter/pillar y").Get(),
                   mScreenLogHeight);
    }
  }

  float cropWidth;
  float cropHeight;

  if (transform & hwcomposer::HWCTransform::kTransform90) {
    dfWidthInSrc = dfHeight.Phys(mScreenLogHeight);
    dfHeightInSrc = dfWidth.Phys(mScreenLogWidth);
    cropWidth = dfWidthInSrc / yscale;
    cropHeight = dfHeightInSrc / xscale;
  } else {
    dfWidthInSrc = dfWidth.Phys(mScreenLogWidth);
    dfHeightInSrc = dfHeight.Phys(mScreenLogHeight);
    cropWidth = dfWidthInSrc / xscale;
    cropHeight = dfHeightInSrc / yscale;
  }

  char strbuf[HWCVAL_DEFAULT_STRLEN];
  char* p = dfWidth.WriteStr(strbuf, "%d");
  *p++ = 'x';
  dfHeight.WriteStr(p, "%d");
  HWCLOGD_COND(
      eLogHarness,
      "Format %s transform %d screen %dx%d df %s dfInSrc %dx%d scale %fx%f",
      FormatToStr(format), transform, mScreenWidth, mScreenHeight, strbuf,
      dfWidthInSrc, dfHeightInSrc, (double)xscale, (double)yscale);

  EnforceMinCrop(format, INT_MAX, INT_MAX, cropWidth, cropHeight);

  cropWidth = min<float>(cropWidth, HWCH_PANELFIT_MAX_SOURCE_WIDTH);
  cropHeight = min<float>(cropHeight, HWCH_PANELFIT_MAX_SOURCE_HEIGHT);

  // Now as we've adjusted the crop, we need to work the other way and
  // regenerate the display frame
  if (transform & hwcomposer::HWCTransform::kTransform90) {
    dfWidthInSrc = (cropWidth * yscale) + 0.5;
    dfHeightInSrc = (cropHeight * xscale) + 0.5;
    cropWidth = dfWidthInSrc / yscale;
    cropHeight = dfHeightInSrc / xscale;
    dfWidth = Scaled(dfWidthInSrc, mScreenLogHeight);
    dfHeight = Scaled(dfHeightInSrc, mScreenLogWidth);
  } else {
    dfWidthInSrc = (cropWidth * xscale) + 0.5;
    dfHeightInSrc = (cropHeight * yscale) + 0.5;
    dfWidth = Scaled(dfWidthInSrc, mScreenLogWidth);
    dfHeight = Scaled(dfHeightInSrc, mScreenLogHeight);
  }

  p = dfWidth.WriteStr(strbuf, "%d");
  *p++ = 'x';
  dfHeight.WriteStr(p, "%d");
  HWCLOGD_COND(
      eLogHarness,
      "CreatePfLayerInternal: crop (%f,%f) screenLogInSrc (%d,%d) df %s",
      double(cropWidth), double(cropHeight), screenLogWidthInSrc,
      screenLogHeightInSrc, strbuf);

  uint32_t bufferWidth =
      Choice(cropWidth, screenLogWidthInSrc * 2, "bufferWidth")
          .Get();  // TODO: more advanced way of determining a buffer size
  uint32_t bufferHeight =
      Choice(cropHeight, screenLogHeightInSrc * 2, "bufferHeight").Get();

  RoundSizes(format, bufferWidth, bufferHeight);
  HWCLOGV_COND(eLogHarness, "New buffer format 0x%x %s %dx%d", format,
               FormatToStr(format), bufferWidth, bufferHeight);

  Layer* layer = new Layer(name, bufferWidth, bufferHeight, format);

  layer->SetTransform(transform);

  float cropX = CropAlignmentChoice(bufferWidth, cropWidth).Get();
  float cropY = CropAlignmentChoice(bufferHeight, cropHeight).Get();
  EnforceMinCrop(format, bufferWidth - cropWidth, bufferHeight - cropHeight,
                 cropX, cropY);

  Coord<int32_t> dfRight = dfX + dfWidth;
  Coord<int32_t> dfBottom = dfY + dfHeight;

  // Contain display frame within the screen area
  if (dfRight.Phys(mScreenLogWidth) > mScreenLogWidth) {
    dfRight = Scaled(mScreenLogWidth, mScreenLogWidth);
    dfX = dfRight - dfWidth;

    if (dfX.Phys(mScreenLogWidth) < 0) {
      dfX = Scaled(0, mScreenLogWidth);
    }
  }

  if (dfBottom.Phys(mScreenLogHeight) > mScreenLogHeight) {
    dfBottom = Scaled(mScreenLogHeight, mScreenLogHeight);
    dfY = dfBottom - dfHeight;

    if (dfY.Phys(mScreenLogHeight) < 0) {
      dfY = Scaled(0, mScreenLogHeight);
    }
  }

  layer->SetLogicalDisplayFrame(LogDisplayRect(dfX, dfY, dfRight, dfBottom));
  SetLayerCropInsideBuffer(layer, cropX, cropY, cropWidth, cropHeight,
                           bufferWidth, bufferHeight);

  return layer;
}

void Hwch::ApiTest::RoundSizes(uint32_t format, uint32_t& w, uint32_t& h) {
  mSystem.GetBufferFormatConfigManager().AdjustBufferSize(format, w, h);
}

void Hwch::ApiTest::EnforceMinCrop(uint32_t format, uint32_t bufferWidth,
                                   uint32_t bufferHeight, float& w, float& h) {
  mSystem.GetBufferFormatConfigManager().AdjustCropSize(format, bufferWidth,
                                                        bufferHeight, w, h);
}

Hwch::Layer* Hwch::ApiTest::CreateLayer(const char* name) {
  uint32_t format = mFormatChoice.Get();

  uint32_t width = mWidthChoice->Get();
  uint32_t height = BufferSizeChoice(mScreenHeight, mMinBufHeight,
                                     min(HWCH_MAX_PIXELS_PER_BUFFER / width,
                                         (uint32_t)mMaxBufHeight)).Get();
  RoundSizes(format, width, height);

  HWCLOGV_COND(eLogHarness, "New buffer format 0x%x %s %dx%d", format,
               FormatToStr(format), width, height);

  Layer* layer = new Layer(name, width, height, format);
  SetLayerCrop(layer, format, width, height);
  SetLayerDisplayFrame(layer);
  SetLayerBlending(layer);

  if (mRandomTiling) {
    layer->SetTile(mTileChoice.Get());
  }

  if (mRCEnabled) {
    layer->SetCompression(mRCChoice.Get());

    switch (layer->GetCompression()) {
      case Hwch::Layer::eCompressionAuto: {
        ++mNumRCLayersAuto;
        break;
      }
      case Hwch::Layer::eCompressionRC: {
        ++mNumRCLayersRC;
        break;
      }
      case Hwch::Layer::eCompressionCC_RC: {
        ++mNumRCLayersCC_RC;
        break;
      }
      case Hwch::Layer::eCompressionHint: {
        ++mNumRCLayersHint;
        break;
      }
    }

    ++mNumRCLayersCreated;
  }

  if (mSkipChoice.Get() < mSkipPercent) {
    // Shall we also randomly set the needBuffer flag?
    layer->SetSkip(true);
    ++mNumSkipLayersCreated;
  } else {
    ChoosePattern(layer);
  }

  layer->SetHwcAcquireDelay(mHwcAcquireDelayChoice.Get());

  return layer;
}

// This creates a layer with the same dimensions and location as the one
// underneath.
// It is RGBA and may or may not be transparent.
Hwch::Layer* Hwch::ApiTest::CreateOverlayLayer(const char* name,
                                               Hwch::Layer* inLayer,
                                               uint32_t colour) {
  uint32_t format = HAL_PIXEL_FORMAT_RGBA_8888;

  HWCLOGV_COND(eLogHarness, "New overlay buffer format 0x%x %s colour 0x%x",
               format, FormatToStr(format), colour);

  Layer* layer =
      new Layer(name, inLayer->GetWidth(), inLayer->GetHeight(), format);
  layer->SetCrop(inLayer->GetCrop());
  layer->SetLogicalDisplayFrame(inLayer->GetLogicalDisplayFrame());
  layer->SetBlending(HWC_BLENDING_PREMULT);
  layer->SetPattern(mPatternMgr.CreateSolidColourPtn(format, colour));

  layer->SetHwcAcquireDelay(mHwcAcquireDelayChoice.Get());

  return layer;
}

Hwch::Layer* Hwch::ApiTest::CreatePanelFitterLayer(const char* name,
                                                   uint32_t layerIndex) {
  uint32_t format = mPFFormatChoice.Get();

  // With panel fitter we decide the displayframe first, then work out the crop
  // using the scale factor
  Layer* layer = CreatePFLayerInternal(name, format, layerIndex);
  SetLayerBlending(layer);
  ChoosePattern(layer);

  layer->SetHwcAcquireDelay(mHwcAcquireDelayChoice.Get());

  return layer;
}

void Hwch::ApiTest::ChoosePattern(Hwch::Layer* layer) {
  uint32_t fg = Alpha(mColourChoice.Get(), mAlphaChoice.Get());
  uint32_t bg = Alpha(mColourChoice.Get(), mAlphaChoice.Get());

  if (fg == 0) {
    // Let's make half of all layers with transparent background transparent all
    // over
    if (mBoolChoice.Get() == 0) {
      bg = 0;
    }
  }

  while (fg == bg) {
    bg = Alpha(mColourChoice.Get(), mAlphaChoice.Get());
  }

  if (fg == bg) {
    layer->SetPattern(mPatternMgr.CreateSolidColourPtn(layer->GetFormat(), bg));
  } else {
    layer->SetPattern(mPatternMgr.CreateHorizontalLinePtn(
        layer->GetFormat(), mUpdateRateChoice.Get(), fg, bg));
  }
}

int Hwch::ApiTest::RunScenario() {
  ParseOptions();

  mDisplayFrameInsideScreen =
      (GetParam("display_frame_not_inside_screen") == 0);

  uint32_t maxAcquireDelay =
      GetTimeParamUs("max_acquire_delay", 15000) /
      HWCVAL_MS_TO_US;  // maximum acquire delay given to layers (in ms)

  // Whether to disable adding NV12 layers
  mNoNV12 = (GetParam("no_NV12") != 0);
  bool noYTiledNV12 = (GetParam("no_y_tiled_NV12") != 0);
  bool noYUY2 = (GetParam("no_YUY2") != 0);

  // Maximum buffer size. Use smaller numbers to encourage the test to generate
  // more layers.
  mMaxBufWidth = GetIntParam("max_buffer_width", HWCH_MAX_BUFFER_WIDTH);
  mMaxBufHeight = GetIntParam("max_buffer_height", HWCH_MAX_BUFFER_HEIGHT);

  bool noSleeps = (GetParam("no_sleeps") != 0);
  int maxLayers = GetIntParam("max_layers", HWCH_APITEST_MAX_LAYERS);
  uint32_t maxRam = GetIntParam("max_ram", HWCH_MAX_RAM_USAGE);
  uint32_t maxFramesPerIteration = GetIntParam("max_frames_per_iteration", 100);

  bool panelFitter =
      (GetParam("panel_fitter") !=
       0);  // create layer lists which encourage the use of panel fitter

  // For panel fitter tests, minimum scale factor the tests will generate
  float minScaleFactor =
      GetFloatParam("panel_fitter_validation_min_scale_factor",
                    HWCH_PANELFITVAL_MIN_SCALE_FACTOR);

  // For panel fitter tests, minimum scale factor HWC will use panel fitter for
  float minPFScaleFactor =
      GetFloatParam("panel_fitter_validation_min_supported_scale_factor",
                    HWCH_PANELFITVAL_MIN_PF_SCALE_FACTOR);

  // For panel fitter tests, maximum scale factor HWC will use panel fitter for
  float maxPFScaleFactor =
      GetFloatParam("panel_fitter_validation_min_supported_scale_factor",
                    HWCH_PANELFITVAL_MAX_PF_SCALE_FACTOR);

  // For panel fitter tests, maximjm scale factor the tests will generate
  float maxScaleFactor =
      GetFloatParam("panel_fitter_validation_max_scale_factor",
                    HWCH_PANELFITVAL_MAX_SCALE_FACTOR);

  // What percentage of skip layers shall we generate?
  mSkipPercent = max(GetIntParam("skip_percent", 0), 0);

  // Include transparency filter tests
  bool testTransparencyFilter = (GetParam("transparency_filter") != 0);

  // Exclude presentation mode
  bool testPresentationMode = (GetParam("no_presentation_mode") == 0);

  // Small number of iterations by default, so "valhwch -all" does not take too
  // long.
  // For real testing, recommend thousands.
  uint32_t testIterations = GetIntParam("test_iterations", 20);

  int forceTransform = GetIntParam("force_transform", -1);
  if (forceTransform >= 0) {
    mTransformChoice.SetMin(forceTransform);
    mTransformChoice.SetMax(forceTransform);
  }

  mRandomTiling = (GetParam("no_random_tiling") == 0);

  // Check to see if Render Compression has been enabled
  mRCEnabled = (GetParam("random_render_compression") != 0);

  // Minimum and maximum scale factors. Defaults mean that other limitations
  // will be more
  // significant.
  mMinLayerScale = GetFloatParam("min_layer_scale", 0.001);
  mMaxLayerScale = GetFloatParam("max_layer_scale", 1000);

  // Number of transform (rotation/flip) errors to allow
  uint32_t allowedTransformErrors = GetIntParam("allow_transform_errors", 0);

  // Api test creates large number of layers with crazy and unrealistic
  // combinations of
  // buffer and layer sizes, parameters and alignments.
  // As a result, PartitionedComposer has not been optimized to cope with these
  // parameters
  // and so sometimes composition is extremely slow.
  // Alternatively, it may be that our current practice of filling the buffers
  // from the CPU
  // rather than the GPU causes locking issues in the kernel which can make it
  // very slow
  // for GL to access the buffers in partitioned composer.
  //
  // The result of this is that OnSet may exceed the normal 200ms time limit we
  // have applied.
  // To prevent spurious test failure, we therefore downgrade this error to a
  // warning, in
  // this test only.
  SetCheckPriority(eCheckOnSetLatency, ANDROID_LOG_WARN);
  SetCheckPriority(eCheckUnblankingLatency, ANDROID_LOG_WARN);

  // Force all display frames to be inside screen area
  // Avoids bogus problems caused by mode changes.
  SetCheck(eOptDispFrameAlwaysInsideScreen, true);

  int seed = mStartSeed;
  int clearLayersCount = 1;  // seed first iteration

  mScreenWidth = mSystem.GetDisplay(0).GetWidth();
  mScreenHeight = mSystem.GetDisplay(0).GetHeight();
  HWCLOGD_COND(eLogHarness, "ApiTest: Screen %dx%d", mScreenWidth,
               mScreenHeight);

  LogIntChoice layerCountChoice(1, maxLayers);

  mFormatChoice.Add(HAL_PIXEL_FORMAT_RGBA_8888);
  mFormatChoice.Add(HAL_PIXEL_FORMAT_RGBA_8888);
  mFormatChoice.Add(HAL_PIXEL_FORMAT_RGBA_8888);
  mFormatChoice.Add(HAL_PIXEL_FORMAT_RGBA_8888);

  // Formats not supported by Pattern Fill will be commented out for now
  mFormatChoice.Add(HAL_PIXEL_FORMAT_BGRA_8888);
  mFormatChoice.Add(HAL_PIXEL_FORMAT_RGBX_8888);
  mFormatChoice.Add(HAL_PIXEL_FORMAT_RGB_565);

  // Add the NV12 formats unless the user has disabled them on the command line
  if (mNoNV12) {
    HWCLOGD_COND(eLogHarness, "ApiTest: Disabling NV12");
  } else {
    if (!noYTiledNV12) {
      // Tiled NV12 weighted higher than linear by the double calls
      mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL);
      mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL);
      mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL);
      mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL);
    }

    mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12);
    mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12);
    mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12);
    mFormatChoice.Add(HAL_PIXEL_FORMAT_NV12);
  }

  if (!noYUY2) {
    mFormatChoice.Add(HAL_PIXEL_FORMAT_YCbCr_422_I);
  }

  // Panel fitter mode format choices
  // Not bothering with NV12 because this tends to result in VPP compositions
  mPFFormatChoice.Add(HAL_PIXEL_FORMAT_RGBA_8888);
  mPFFormatChoice.Add(HAL_PIXEL_FORMAT_RGBX_8888);
  mPFFormatChoice.Add(HAL_PIXEL_FORMAT_RGB_565);

  mWidthChoice =
      new BufferSizeChoice(mScreenWidth, mMinBufWidth, HWCH_MAX_BUFFER_WIDTH);
  mBlendingChoice.Add(HWC_BLENDING_PREMULT);
  mBlendingChoice.Add(HWC_BLENDING_NONE);
  // Gary says COVERAGE is not used and is not well defined so no point in
  // testing it.

  mUpdateRateChoice.Add(1);
  mUpdateRateChoice.Add(2);
  mUpdateRateChoice.Add(4);
  mUpdateRateChoice.Add(8);
  mUpdateRateChoice.Add(15);
  mUpdateRateChoice.Add(24);
  mUpdateRateChoice.Add(30);
  mUpdateRateChoice.Add(60);

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

  if (testTransparencyFilter) {
    mColourChoice.Add(0);  // Fully transparent
  }

  mTileChoice.Add(Hwch::Layer::eLinear);
  mTileChoice.Add(Hwch::Layer::eXTile);
  mTileChoice.Add(Hwch::Layer::eYTile);
  mTileChoice.Add(Hwch::Layer::eAnyTile);
  mTileChoice.Add(Hwch::Layer::eAnyTile);

  // Render Compression choices
  mRCChoice.Add(Hwch::Layer::CompressionType::eCompressionAuto);
  mRCChoice.Add(Hwch::Layer::CompressionType::eCompressionRC);
  // mRCChoice.Add(Hwch::Layer::CompressionType::eCompressionCC_RC);
  mRCChoice.Add(Hwch::Layer::CompressionType::eCompressionHint);

  mMinPFDisplayFrameWidth = mScreenWidth / 10;
  mMinPFDisplayFrameHeight = mScreenHeight / 10;
  mPanelFitterScaleChoice.SetScreenSize(mScreenWidth, mScreenHeight);
  mPanelFitterScaleChoice.SetLimits(minScaleFactor, minPFScaleFactor,
                                    maxScaleFactor, maxPFScaleFactor);
  mHwcAcquireDelayChoice.SetMax(maxAcquireDelay);

  // Only if enabled by -transparency_filter:
  // 1 video layer in 4 has a transparent overlay added with same dimensions
  // another 1 in 4 has a non-transparent overlay with same dimensions
  Choice videoOverlayChooser(0, 3);

  Choice pauseChooser(0, 600);  // One in 600 probability of a pause
  Choice pauseDurationUsChoice(1, 6000000);          // Up to 6 seconds
  Choice numFramesChoice(1, maxFramesPerIteration);  // Number of frames to send
                                                     // between each layout
                                                     // update

  Choice screenRotationChooser(0, 200);

  Choice updateCropChooser(0, 5);
  Choice updateDisplayFrameChooser(0, 5);
  Choice updateBlendingChooser(0, 5);
  Choice panelFitterValChooser(panelFitter ? 0 : 1, 1);

  Hwch::Frame frame(mInterface);
  uint32_t layerCreateCount =
      100;  // Start from 101 to avoid confusion with other numbering schemes

  // Ensure we don't go into extended mode and create spurious errors
  UpdateInputState(true, false);


  for (uint32_t i = 0; i < testIterations; ++i) {
    HWCLOGD_COND(eLogHarness, ">>> Test Iteration %d <<<", i);

    uint32_t numDisplays = mInterface.NumDisplays();

    // Remove all existing layers every mClearLayersPeriod iterations, so
    // behaviour is consistent.
    if (--clearLayersCount <= 0) {
      for (uint32_t d = 0; d < numDisplays; ++d) {
        while (frame.NumLayers(d) > 0) {
          delete frame.RemoveLayerAt(0, d);
        }
      }


      clearLayersCount = mClearLayersPeriod;
    }

    // Reseed every iteration for best consistency
    Choice::Seed(seed++);
    uint32_t numFrames = numFramesChoice.Get();

    mScreenIsRotated90 = frame.IsRotated90();
    mScreenLogWidth = mSystem.GetDisplay(0).GetLogicalWidth();
    mScreenLogHeight = mSystem.GetDisplay(0).GetLogicalHeight();

    // Choose whether to use presentation mode and share out the ram allowance
    // accordingly
    bool presentationMode = testPresentationMode ? mBoolChoice.Get() : false;

    uint32_t minDisp = 0;
    uint32_t maxDisp;
    uint32_t ramPerDisp;
    uint32_t maxRamSoFar = 0;

    if (!presentationMode) {
      // Select clone mode
      maxDisp = 1;
      ramPerDisp = maxRam;

      // We are in clone mode, so we can't have any presentation mode layers on
      // D1 or D2
      for (uint32_t d = 1; d < numDisplays; ++d) {
        HWCLOGV_COND(eLogHarness, "D%d: deleting presentation mode layers", d);
        for (int l = frame.NumLayers(d) - 1; l >= 0; --l) {
          Layer* layer = frame.GetLayer(l, d);
          if (!layer->IsForCloning() && !layer->IsAClone()) {
            delete frame.RemoveLayerAt(l, d);
          }
        }
      }

      // All layers on D0 must be marked for cloning
      // (Some of these will be deleted at the next stage).
      for (uint32_t l = 0; l < frame.NumLayers(0); ++l) {
        Layer* layer = frame.GetLayer(l, 0);
        layer->SetForCloning(true);
      }
    } else {
      // Presentation mode
      // We can leave some cloned layers, it will do no harm.
      maxDisp = numDisplays;
      ramPerDisp = maxRam / numDisplays;
    }

    for (uint32_t d = minDisp; d < maxDisp; ++d) {
      maxRamSoFar += ramPerDisp;
      int layersRemaining = frame.NumLayers(d);
      int requiredLayerCount = layerCountChoice.Get();

      bool panelFitterVal = (panelFitterValChooser.Get() == 0);
      int layersToKeep;

      if (panelFitterVal) {
        layersToKeep = 0;
        mPanelFitterScale = mPanelFitterScaleChoice.Get();
      } else {
        Choice layersToKeepChoice(0, min(layersRemaining, requiredLayerCount));
        layersToKeep = layersToKeepChoice.Get();
      }

      HWCLOGV_COND(eLogHarness, "D%d: Deleting layers till we have %d/%d", d,
                   layersToKeep, layersRemaining);
      while (layersRemaining > layersToKeep) {
        int layerToRemove = Choice(0, layersRemaining - 1).Get();

        delete frame.RemoveLayerAt(layerToRemove, d);
        // because of the delete, any clones will also be removed
        --layersRemaining;
      }

      HWCLOGV_COND(eLogHarness,
                   "D%d: Assessing RAM used by remaining %d layers", d,
                   layersRemaining);
      uint32_t ram = 0;

      for (int l = 0; l < layersRemaining; ++l) {
        ram += frame.GetLayer(l, d)->GetMemoryUsage();
      }

      HWCLOGD_COND(eLogHarness,
                   "D%d Mode: %s remaining layers: %d required: %d", d,
                   presentationMode ? "presentation" : "clone",
                   frame.NumLayers(d), requiredLayerCount);
      ALOG_ASSERT(int(frame.NumLayers(d)) == layersRemaining);
      ALOG_ASSERT(requiredLayerCount <= maxLayers);

      if ((ram >= maxRamSoFar) && (layersRemaining == 0)) {
        HWCLOGI_COND(eLogHarness,
                     "No RAM available to create one layer on D%d. ram=%d, "
                     "maxRamSoFar=%d, requiredLayerCount=%d",
                     ram, maxRamSoFar, requiredLayerCount);
      }

      while ((layersRemaining < requiredLayerCount) && (ram < maxRamSoFar)) {
        char nameBuf[20];
        sprintf(nameBuf, "TestLayer %d", ++layerCreateCount);
        HWCLOGV_COND(eLogHarness, "D%d: Creating %s", d, nameBuf);

        int layerZOrder = Choice(0, layersRemaining).Get();
        Layer* layer;

        if (panelFitterVal) {
          layer = CreatePanelFitterLayer(nameBuf, layersRemaining);
          ++mNumPanelFitterLayersCreated;
        } else {
          layer = CreateLayer(nameBuf);
          ++mNumNormalLayersCreated;
        }

        HWCLOGV_COND(eLogHarness, "D%d: Adding layer at Z=%d disp=%d", d,
                     layerZOrder, (presentationMode ? d : -1));
        frame.AddAt(layerZOrder, *layer, (presentationMode ? d : -1));

        ram += layer->GetMemoryUsage();
        ++layersRemaining;

        if (testTransparencyFilter && layer->HasNV12Format()) {
          if (ram < maxRam) {
            int v = videoOverlayChooser.Get();
            if (v < 2) {
              strcat(nameBuf, "+");
              if (v == 0) {
                // Add transparent overlay with same dimensions as original
                layer = CreateOverlayLayer(nameBuf, layer, 0);
              } else {
                // Add non-transparent overlay with same dimensions as original
                layer = CreateOverlayLayer(nameBuf, layer, mColourChoice.Get());
              }

              frame.AddAt(layerZOrder + 1, *layer, (presentationMode ? d : -1));
              ram += layer->GetMemoryUsage();
              ++layersRemaining;
            }
          }
        }
      }
    }

    for (uint32_t f = 0; f < numFrames; ++f) {
      frame.Send();

      // 1 frame in n, have a random delay.
      if (!noSleeps && (pauseChooser.Get() == 0)) {
        usleep(pauseDurationUsChoice.Get());
      }

      if (!mNoRotation && (screenRotationChooser.Get() == 0)) {
        frame.RotateTo(mScreenRotationChoice.Get());
      }

      uint32_t d = presentationMode
                       ? Choice(0, mInterface.NumDisplays() - 1,
                                "presentation mode display").Get()
                       : 0;
      int layersRemaining = frame.NumLayers(d);

      if (layersRemaining > 0) {
        Choice layerChoice(0, layersRemaining - 1, "layerChoice");
        uint32_t l = layerChoice.Get();
        Layer* layer = frame.GetLayer(l, d);

        if (updateCropChooser.Get() == 0) {
          HWCLOGD_COND(eLogHarness, "Modifying layer crop %d (%s)", l,
                       layer->GetName());

          uint32_t w = layer->GetWidth().mValue;
          uint32_t h = layer->GetHeight().mValue;
          SetLayerCrop(layer, layer->GetFormat(), w, h);
        }

        if (updateDisplayFrameChooser.Get() == 0) {
          HWCLOGD_COND(eLogHarness,
                       "Modifying layer display frame and transform %d (%s)", l,
                       layer->GetName());
          SetLayerDisplayFrame(layer);
        }

        if (updateBlendingChooser.Get() == 0) {
          HWCLOGD_COND(eLogHarness, "Modifying layer blending %d (%s)", l,
                       layer->GetName());
          SetLayerBlending(layer);
        }
      } else {
        HWCLOGD_COND(eLogHarness, "NO LAYERS on D%d!", d);
      }

      ChooseScreenDisable(frame);

      RandomEvent();
    }
  }

  // Delete remaining layers
  for (uint32_t d = 0; d < MAX_DISPLAYS; ++d) {
    while (frame.NumLayers(d) > 0) {
      delete frame.RemoveLayerAt(0, d);
    }
  }

  HWCLOGV_COND(eLogHarness,
               "Api test complete, reporting statistics and restoring state");
  if (!IsOptionEnabled(eOptBrief)) {
    HwcTestState::getInstance()->ReportPanelFitterStatistics(stdout);
    ReportStatistics();
  }

  if (allowedTransformErrors > 0) {
    // Set the number of transform errors below which we only issue a warning
    ConditionalDropPriority(eCheckPlaneTransform, allowedTransformErrors,
                            ANDROID_LOG_WARN);
  }

  Tidyup();

  return 0;
}

void Hwch::ApiTest::ReportStatistics() {
  uint32_t numHotUnplugs;
  uint32_t numEsdRecoveryEvents;
// TODO implement simulate hotplug in real HWC
  printf("Layers created:    normal:  %6d Panel Fitter optimized:     %6d\n",
         mNumNormalLayersCreated, mNumPanelFitterLayersCreated);

  // Only print the RC statistics if RC is enabled
  if (mRCEnabled) {
    printf("RC layers created:          %6d RC layers with 'Auto':      %6d\n",
           mNumRCLayersCreated, mNumRCLayersAuto);
    printf(
        "RC layers with 'RC':        %6d RC layers with 'CC_RC':     %6d RC "
        "layers with 'Hint':      %6d\n",
        mNumRCLayersRC, mNumRCLayersCC_RC, mNumRCLayersHint);
  }

  printf(
      "Suspends:                   %6d Mode changes:               %6d Video "
      "opt mode changes:     %6d\n",
      mNumSuspends, mNumModeChanges, mNumVideoOptimizationModeChanges);
  printf("Hot unplugs:                %6d Esd recovery events:        %6d\n",
         numHotUnplugs, numEsdRecoveryEvents);

  printf("\n");
}
