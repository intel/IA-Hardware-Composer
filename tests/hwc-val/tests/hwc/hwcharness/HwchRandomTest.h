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

#ifndef __HwchRandomTest_h__
#define __HwchRandomTest_h__

#include "HwchTest.h"
#include "HwchDisplayChoice.h"

namespace Hwch {
class RandomTest : public Test {
 public:
  RandomTest(Hwch::Interface& interface);
  virtual ~RandomTest();
  void ParseOptions();

 protected:
  void ChooseScreenDisable(Hwch::Frame& frame);
  void RandomEvent();
  void Tidyup();
  virtual void ClearVideo();
  virtual void ReportStatistics();

  Choice mBoolChoice;
  MultiChoice<uint32_t> mBlankTypeChoice;
  Choice mScreenDisableChooser;
  Choice mBlankFramesChoice;
  LogarithmicChoice mBlankFrameSleepUsChoice;
  Choice mHotPlugChooser;
  Choice mEsdRecoveryChooser;
  Choice mModeChangeChooser;
  Choice mModeChoice;
  Choice mVideoOptimizationModeChooser;
  Choice mVideoOptimizationModeChoice;

  // Which display will we hot plug?
  MultiChoice<uint32_t> mHotPlugDisplayTypeChoice;

  // suspend/resume
  EventDelayChoice mEventDelayChoice;
  LogIntChoice mModeChangeDelayChoice;
  LogIntChoice mHotPlugDelayChoice;
  LogIntChoice mVideoOptimizationModeDelayChoice;

  bool mNoRotation;
  MultiChoice<hwcomposer::HWCRotation> mScreenRotationChoice;

  // Seeding
  int mStartSeed;
  int mClearLayersPeriod;

  // Which display types are plugged?
  uint32_t mPlugged;

  // Statistics
  uint32_t mNumNormalLayersCreated;
  uint32_t mNumPanelFitterLayersCreated;
  uint32_t mNumSkipLayersCreated;
  uint32_t mNumSuspends;
  uint32_t mNumFencePolicySelections;
  uint32_t mNumModeChanges;
  uint32_t mNumExtendedModeTransitions;
  uint32_t mNumExtendedModePanelDisables;
  uint32_t mNumEsdRecoveryEvents;
  uint32_t mNumVideoOptimizationModeChanges;

  // RC Statistics
  uint32_t mNumRCLayersCreated;
  uint32_t mNumRCLayersAuto;
  uint32_t mNumRCLayersRC;
  uint32_t mNumRCLayersCC_RC;
  uint32_t mNumRCLayersHint;
};
}

#endif  // __HwchRandomTest_h__
