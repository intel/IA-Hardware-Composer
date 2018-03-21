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

#include "HwchTest.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include "iservice.h"
#include "hwcserviceapi.h"

using namespace hwcomposer;

// TestParams class
// Encapsulates command-line options
Hwch::TestParams::TestParams() : mpParams(0) {
}

Hwch::Test::Test(Hwch::Interface& interface)
    : mInterface(interface), mSystem(Hwch::System::getInstance()) {
  // No expectation as to cloning optimization since we can't
  // second guess how HWC will decide to perform cloning.
  // However, in a specific test, this can be set where cloning optimization is
  // expected.
  SetExpectedMode(HwcTestConfig::eDontCare);
}

Hwch::Test::~Test() {
  // Disconnect from the HWC Service Api
  if (mHwcsHandle) {
    HwcService_Disconnect(mHwcsHandle);
  }
}

void Hwch::TestParams::SetParams(ParamVec& params) {
  mpParams = &params;
}

const char* Hwch::TestParams::GetParam(const char* name) {
  if (!mpParams) {
    return 0;
  }
    ParamVecItr itr = mpParams->find(std::string(name));
    if(itr != mpParams->end()) {
    UserParam& param = itr->second;
    param.mChecked = true;

    std::string paramStr;

    if (param.mValue == "1") {
      paramStr = std::string(name);
    } else {
      paramStr =
          std::string(name) + param.mValue;
    }

    if (mUsedArgs.find(paramStr) < 0) {
      mUsedArgs += paramStr;
    }

    return param.mValue.c_str();
  } else {
    return 0;
  }
}

const char* Hwch::TestParams::GetStrParam(const char* name, const char* deflt) {
  // Safe for users as this will not return a null
  const char* str = GetParam(name);

  if (str) {
    return str;
  } else {
    return deflt;
  }
}

std::string Hwch::TestParams::GetStrParamLower(const char* name,
                                                    const char* deflt) {
  std::string result(GetStrParam(name, deflt));
  for (std::string::size_type i = 0; i < result.length(); ++i)
      result[i] =  std::tolower(result[i]);
  return result;
}

int Hwch::TestParams::GetIntParam(const char* name, int deflt) {
  const char* str = GetParam(name);

  if (str) {
    return atoi(str);
  } else {
    return deflt;
  }
}

float Hwch::TestParams::GetFloatParam(const char* name, float deflt) {
  const char* str = GetParam(name);

  if (str) {
    return atof(str);
  } else {
    return deflt;
  }
}

int64_t Hwch::TestParams::GetTimeParamUs(const char* name, int64_t deflt) {
  const char* str = GetParam(name);
  const char* p = str;
  int64_t result = deflt;

  if (str) {
    double f = atofinc(p);

    if (strncmpinc(p, "s") == 0) {
      result = f * HWCVAL_SEC_TO_US;
    } else if (strncmpinc(p, "ms") == 0) {
      result = f * HWCVAL_MS_TO_US;
    } else if (strncmpinc(p, "us") == 0) {
      result = f;
    } else if (strncmpinc(p, "ns") == 0) {
      result = f / HWCVAL_US_TO_NS;
    } else {
      // Assume ms by default
      result = f * HWCVAL_MS_TO_US;
    }
  }

  return result;
}

// returns true if a valid range parameter (i.e. -param=x-y) is found, and the
// values in the last two arguments.
// default is INT_MIN to INT_MAX.
bool Hwch::TestParams::GetRangeParam(const char* name, Hwch::Range& range) {
  const char* str = GetParam(name);

  if (str) {
    range = Hwch::Range(str);

    return true;
  }

  return false;
}

std::string& Hwch::TestParams::UsedArgs() {
  return mUsedArgs;
}

void Hwch::Test::SetName(const char* name) {
  mName = name;
}

const char* Hwch::Test::GetName() {
  return mName.c_str();
}

bool Hwch::Test::CheckMDSAndSetup(bool report) {
  if (mHwcsHandle) {
    return true;  // Already connected
  }

  // Attempt to connect to the new HWC Service Api
  mHwcsHandle = HwcService_Connect();
  if (!mHwcsHandle) {
    HWCERROR(eCheckSessionFail, "HWC Service Api could not connect to service");
    return false;
  }

  return true;
}

bool Hwch::Test::IsAutoExtMode() {
  return (HwcTestState::getInstance()->IsAutoExtMode());
}

status_t Hwch::Test::UpdateVideoState(int sessionId, bool isPrepared,
                                      uint32_t fps) {
  if (IsAutoExtMode()) {
    // In No-MDS mode there are no video sessions
    return OK;
  }

  status_t st = NAME_NOT_FOUND;

  if (CheckMDSAndSetup(false)) {
    st = HwcService_MDS_UpdateVideoState(mHwcsHandle, sessionId,
                                         isPrepared ? HWCS_TRUE : HWCS_FALSE);

    if (st == 0) {
      st = HwcService_MDS_UpdateVideoFPS(mHwcsHandle, sessionId, fps);
    }
  }

  return st;
}

status_t Hwch::Test::UpdateInputState(bool inputActive,
                                      bool expectPanelEnableAsInput,
                                      Hwch::Frame* frame) {
  if (IsAutoExtMode()) {
    HWCLOGD(
        "UpdateInputState: extmodeauto: inputActive %d "
        "expectPanelEnableAsInput %d",
        inputActive, expectPanelEnableAsInput);

    if (expectPanelEnableAsInput) {
      SetExpectedMode(HwcTestConfig::eDontCare);
    }

    // Turn the keypress generator on or off as appropriate
    mSystem.GetInputGenerator().SetActive(inputActive);

    if (frame) {
      frame->Send(10);
    }

    mSystem.GetInputGenerator().Stabilize();

    if (expectPanelEnableAsInput) {
      SetExpectedMode(inputActive ? HwcTestConfig::eOn : HwcTestConfig::eOff);
    }

    return OK;
  } else {
    HWCLOGD(
        "UpdateInputState: NOT extmodeauto: inputActive %d "
        "expectPanelEnableAsInput %d",
        inputActive, expectPanelEnableAsInput);
  }

  if (CheckMDSAndSetup(false)) {
    if (expectPanelEnableAsInput) {
      SetExpectedMode(inputActive ? HwcTestConfig::eOn : HwcTestConfig::eOff);
    }

#ifndef HWCVAL_TARGET_HAS_MULTIPLE_DISPLAY
    return HwcService_MDS_UpdateInputState(
        mHwcsHandle, inputActive ? HWCS_TRUE : HWCS_FALSE);
#endif
  }

  return NAME_NOT_FOUND;
}

void Hwch::Test::SetExpectedMode(HwcTestConfig::PanelModeType modeExpect) {
  HWCLOGV_COND(eLogVideo, "Hwch::Test::SetExpectedMode %s",
               HwcTestConfig::Str(modeExpect));
  HwcGetTestConfig()->SetModeExpect(modeExpect);
}

HwcTestConfig::PanelModeType Hwch::Test::GetExpectedMode() {
  return HwcGetTestConfig()->GetModeExpect();
}

bool Hwch::Test::SimulateHotPlug(bool connected, uint32_t displayTypes,
                                 uint32_t delayUs) {
  std::shared_ptr<AsyncEvent::Data> pData = std::shared_ptr<AsyncEvent::Data>(new AsyncEvent::HotPlugEventData(displayTypes));
  return SendEvent(connected ? AsyncEvent::eHotPlug : AsyncEvent::eHotUnplug,
                   pData, delayUs);
}

bool Hwch::Test::SetVideoOptimizationMode(
    Display::VideoOptimizationMode videoOptimizationMode, uint32_t delayUs) {
  return false;
}

void Hwch::Test::SetCheckPriority(HwcTestCheckType check, int priority) {
  HwcTestResult& result = *HwcGetTestResult();

  if (check < eHwcTestNumChecks) {
    result.mFinalPriority[check] = priority;
  }
}

void Hwch::Test::SetCheck(HwcTestCheckType check, bool enable) {
  HwcTestResult& result = *HwcGetTestResult();
  HwcTestConfig& config = *HwcGetTestConfig();

  if (check < eHwcTestNumChecks) {
    config.mCheckConfigs[check].enable = enable;
    result.mCausesTestFail[check] &= enable;
  }
}

// Set check priority conditionally to reducedPriority if failure count <=
// maxNormCount
void Hwch::Test::ConditionalDropPriority(HwcTestCheckType check,
                                         uint32_t maxNormCount,
                                         int reducedPriority) {
  HwcGetTestResult()->ConditionalDropPriority(check, maxNormCount,
                                              reducedPriority);
}

bool Hwch::Test::IsAbleToRun() {
  return true;
}

bool Hwch::Test::IsOptionEnabled(HwcTestCheckType check) {
  return HwcTestState::getInstance()->IsOptionEnabled(check);
}

// Generate an event.
// delayUs is negative to happen immediately on the main thread;
// zero to happen immediately on the event generator thread;
// positive to happen after the stated delay on the event generator thread.
//
bool Hwch::Test::SendEvent(uint32_t eventType, int32_t delayUs) {
  return mSystem.AddEvent(eventType, delayUs);
}

bool Hwch::Test::SendEvent(uint32_t eventType,
                           std::shared_ptr<Hwch::AsyncEvent::Data> eventData,
                           int32_t delayUs) {
  return mSystem.AddEvent(eventType, eventData, delayUs);
}

bool Hwch::Test::Blank(
    bool blank,  // true for blank, false for unblank
    bool power,  // whether to update the power state to match
    int32_t delayUs) {
  uint32_t t = blank ? AsyncEvent::eBlank : AsyncEvent::eUnblank;

  if (power) {
    t |= blank ? AsyncEvent::eSuspend : AsyncEvent::eResume;
  }

  return SendEvent(t, delayUs);
}

int Hwch::Test::Run() {
  // Unblank if previously blanked
  for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
    if (mSystem.GetDisplay(i).IsConnected()) {
      if (mInterface.IsBlanked(i)) {
        mInterface.Blank(i, 0);
      }
    }
  }

  if (IsAutoExtMode()) {
    // Stop us dropping into extended mode if we don't want to
    mSystem.GetInputGenerator().SetActive(true);
  }

  // retrieve Reference Composer composition flag
  mSystem.SetNoCompose(GetParam("no_compose") != 0);


  RunScenario();

  // Send a blank frame to allow buffers used in the test to be deleted
  mSystem.FlushRetainedBufferSets();

  if (GetParam("blank_after")) {
    for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
      if (mSystem.GetDisplay(i).IsConnected()) {
        mInterface.Blank(i, 1);
      }
    }
  }

  return 0;
}

Hwch::BaseReg* Hwch::BaseReg::mHead = 0;

Hwch::BaseReg::~BaseReg() {
}

Hwch::OptionalTest::OptionalTest(Hwch::Interface& interface)
    : Hwch::Test(interface) {
}

bool Hwch::OptionalTest::IsAbleToRun() {
  return false;
}
