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

#ifndef __HwchApiTest_h__
#define __HwchApiTest_h__

#include "HwchRandomTest.h"
#include "HwchLayerChoice.h"

namespace Hwch {
class PatternMgr;

class ApiTest : public RandomTest {
 public:
  ApiTest(Hwch::Interface& interface);

  virtual int RunScenario();

 protected:
  virtual void ReportStatistics();

 private:
  void SetLayerCrop(Layer* layer, uint32_t format, uint32_t width,
                    uint32_t height);
  void SetLayerCropInsideBuffer(Hwch::Layer* layer, float cropX, float cropY,
                                float cropWidth, float cropHeight,
                                uint32_t bufferWidth, uint32_t bufferHeight);
  void SetLayerDisplayFrame(Layer* layer);
  void SetLayerBlending(Layer* layer);
  void RoundSizes(uint32_t format, uint32_t& w, uint32_t& h);
  void EnforceMinCrop(uint32_t format, uint32_t bufferWidth,
                      uint32_t bufferHeight, float& w, float& h);
  void ChoosePattern(Hwch::Layer* layer);
  Hwch::Layer* CreatePFLayerInternal(const char* name, uint32_t format,
                                     uint32_t layerIndex);
  Hwch::Layer* CreatePanelFitterLayer(const char* name, uint32_t layerIndex);
  Layer* CreateLayer(const char* name);
  Layer* CreateOverlayLayer(const char* name, Layer* layer, uint32_t colour);

  PatternMgr& mPatternMgr;

  PanelFitterScaleChoice mPanelFitterScaleChoice;
  Choice mTransformChoice;
  MultiChoice<uint32_t> mBlendingChoice;
  AlphaChoice mAlphaChoice;
  MultiChoice<uint32_t> mFormatChoice;
  MultiChoice<uint32_t>
      mPFFormatChoice;  // format choices for panel fitter mode
  BufferSizeChoice* mWidthChoice;
  MultiChoice<uint32_t> mColourChoice;
  MultiChoice<uint32_t> mUpdateRateChoice;
  Choice mHwcAcquireDelayChoice;
  MultiChoice<uint32_t> mBlankTypeChoice;
  MultiChoice<uint32_t> mTileChoice;
  MultiChoice<Hwch::Layer::CompressionType> mRCChoice;
  Choice mSkipChoice;

  // Maximum buffer height and width
  uint32_t mMaxBufWidth;
  uint32_t mMaxBufHeight;

  // These are the minima that the test will generate.
  // They will effectively be overriden by the global minima
  // obtained in HwcHarness::SetBufferConfig()
  uint32_t mMinBufWidth;
  uint32_t mMinBufHeight;

  uint32_t mMinCropWidth;
  uint32_t mMinCropHeight;

  uint32_t mMinDisplayFrameWidth;
  uint32_t mMinDisplayFrameHeight;

  uint32_t mMinPFDisplayFrameWidth;
  uint32_t mMinPFDisplayFrameHeight;

  int32_t mScreenWidth;
  int32_t mScreenHeight;
  int32_t mScreenLogWidth;
  int32_t mScreenLogHeight;

  double mMinLayerScale;
  double mMaxLayerScale;

  bool mScreenIsRotated90;
  bool mNoNV12;

  bool mDisplayFrameInsideScreen;

  float mPanelFitterScale;

  // Percentage of layers created as skip layers
  int32_t mSkipPercent;

  bool mRandomTiling;
  bool mRCEnabled = false;
};
}

#endif  // __HwchApiTest_h__
