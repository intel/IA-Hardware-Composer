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

#ifndef __HwchDirectPlanesTest_h__
#define __HwchDirectPlanesTest_h__

#include "HwchRandomTest.h"
#include "HwchLayerChoice.h"

namespace Hwch {
class DirectPlanesTest : public RandomTest {
 public:
  DirectPlanesTest(Hwch::Interface& interface);
  virtual ~DirectPlanesTest();

  virtual int RunScenario();

  bool IsFullScreen(const Hwch::LogDisplayRect& ldr, uint32_t d);
  Layer* CreateLayer(uint32_t d);
  void SetLayerCropDf(Hwch::Layer* layer, uint32_t d);
  void SetLayerFullScreen(Hwch::Layer* layer, uint32_t d);
  void SetLayerBlending(Hwch::Layer* layer);
  void SetLayerTransform(Hwch::Layer* layer);

 private:
  int32_t mDw[MAX_DISPLAYS];
  int32_t mDh[MAX_DISPLAYS];

  MultiChoice<uint32_t> mColourChoice;
  Choice mWidthChoice[MAX_DISPLAYS];
  Choice mHeightChoice[MAX_DISPLAYS];
  MultiChoice<uint32_t> mTransformChoice;
  MultiChoice<uint32_t> mBlendingChoice;
  AlphaChoice mAlphaChoice;
};
}

#endif  // __HwchDirectPlanesTest_h__
