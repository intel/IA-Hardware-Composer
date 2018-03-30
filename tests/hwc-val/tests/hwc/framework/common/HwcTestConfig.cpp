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

#include <binder/Parcel.h>
#include <string>
#include <utils/Timers.h>
#include <cutils/properties.h>
#include "HwcTestConfig.h"
#include "HwcTestLog.h"
#include <math.h>

const char* HwcTestConfig::mComponentNames[] = {
    // ALWAYS ensure this matches the values of the enumerate
    // HwcTestComponentType.
    "None", "Test", "HWC", "Buffers", "Displays", "SF", 0};

#define DECLARE_CHECK(enumId, component, defaultPriority, description, \
                      category)                                        \
  description,
const char* HwcTestConfig::mCheckDescriptions[] = {
#include "HwcTestCheckList.h"
    0};
#undef DECLARE_CHECK

#define DECLARE_CHECK(enumId, component, defaultPriority, description, \
                      category)                                        \
  #enumId,
const char* HwcTestConfig::mCheckNames[] = {
#include "HwcTestCheckList.h"
    0};
#undef DECLARE_CHECK

#define DECLARE_CHECK(enumId, component, defaultPriority, description, \
                      category)                                        \
  eComponent##component,
HwcTestComponentType HwcTestConfig::mCheckComponents[] = {
#include "HwcTestCheckList.h"
    eComponentNone};
#undef DECLARE_CHECK

#define DECLARE_CHECK(enumId, component, defaultPriority, description, \
                      category)                                        \
  #enumId,
static const char* formalNames[] = {
#include "HwcTestCheckList.h"
    0};
#undef DECLARE_CHECK

static std::string GetLongProp(const char* name) {
  // We are combining together the properties "name", "name_1", "name_2" etc.
  std::string result;
  for (int32_t pn = 0; pn < 10; ++pn) {
    std::string propName;
    if (pn > 0) {
      propName = std::string(name) + std::string(std::to_string(pn));
      result += " ";
    } else {
      propName = std::string(name);
    }

    char value[PROPERTY_VALUE_MAX + 1];

    if (!property_get(propName.c_str(), value, NULL)) {
      break;
    }

    result += value;
  }

  ALOGD("Long Property %s=\"%s\"", name, result.c_str());

  return result;
}

HwcTestConfig::HwcTestConfig()
    : mMinLogPriority(HWCVAL_DEFAULT_LOG_PRIORITY),
      mGlobalEnable(false),
      mBufferMonitorEnable(true),
      mModeExpect(eDontCare),
      mStableModeExpect(eDontCare) {
  char logPriorityStr[PROPERTY_VALUE_MAX];
  if (property_get("hwcval.default_log_priority", logPriorityStr, NULL)) {
    const char* priorities = "U-VDIWEFS";  // Matches Android priorities
    mMinLogPriority =
        (int)(strchr(priorities, (int)logPriorityStr[0]) - priorities);
    if (mMinLogPriority >= (int)strlen(priorities)) {
      mMinLogPriority = HWCVAL_DEFAULT_LOG_PRIORITY;
    }
  }

#define DECLARE_CHECK(enumId, component, defaultPriority, description, \
                      category)                                        \
  ANDROID_LOG_##defaultPriority,
  int priorities[] = {
#include "HwcTestCheckList.h"
      0};
#undef DECLARE_CHECK

#define DECLARE_CHECK(enumId, component, defaultPriority, description, \
                      category)                                        \
  eCat##category,
  int categories[] = {
#include "HwcTestCheckList.h"
      0};
#undef DECLARE_CHECK

  // N.B. Property value maximum length is 91 characters currently, so there is
  // a limit to the
  // number of log enables/disables that can be specified using this method.
  // But it's enough for now.
  std::string logEnableStr = GetLongProp("hwcval.log.enable");
  std::string logDisableStr = GetLongProp("hwcval.log.disable");
  std::string logWarningStr = GetLongProp("hwcval.log.setwarning");

  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    // Reset to default check configuration
    mCheckConfigs[i] = HwcCheckConfig();
    mCheckConfigs[i].priority = priorities[i];
    mCheckConfigs[i].category = categories[i];

    char name[HWCVAL_DEFAULT_STRLEN];
    strcpy(name, formalNames[i]);
    strcat(name, " ");

    if (strstr(logEnableStr.c_str(), name) != 0) {
      HwcValLog(ANDROID_LOG_VERBOSE,
                "HwcTestConfig::HwcTestConfig() enabling log string %s",
                formalNames[i]);
      mCheckConfigs[i].enable = true;
    }

    if (strstr(logDisableStr.c_str(), name) != 0) {
      HwcValLog(ANDROID_LOG_VERBOSE,
                "HwcTestConfig::HwcTestConfig() disabling log string %s",
                formalNames[i]);
      mCheckConfigs[i].forceDisable = true;
    }

    if (strstr(logWarningStr.c_str(), name) != 0) {
      if (mCheckConfigs[i].priority > ANDROID_LOG_WARN) {
        HwcValLog(ANDROID_LOG_VERBOSE,
                  "HwcTestConfig::HwcTestConfig() Set warning log string %s",
                  formalNames[i]);
        mCheckConfigs[i].priority = ANDROID_LOG_WARN;
      }
    }
  }

  for (uint32_t i = 0; i < eComponentMax; ++i) {
    mComponentEnabled[i] = true;
  }
}

void HwcTestConfig::Initialise(bool valHwc, bool valDisplays,
                               bool valBufferAllocation, bool valSf,
                               bool valHwcComposition) {
  // Indicate which test components will cause test failure on error

  // Test failures should never be inhibited.
  SetComponentEnabled(eComponentTest, true, true);

  // At component level, we just configure which checks can cause test failure,
  // not actually which are enabled
  // or even their priority.
  SetComponentEnabled(eComponentHWC, true, valHwc);

  SetComponentEnabled(eComponentDisplays, true, valDisplays);
  SetComponentEnabled(eComponentBuffers, true, valBufferAllocation);
  SetComponentEnabled(eComponentSF, true, valSf);

  // Opt category is not enabled by any of the above. We do that now for any
  // checks we require.
  if (valHwcComposition) {
    SetCheck(eCheckHwcCompMatchesRef, true, true);
  }
  // Turn on the master switch
  mGlobalEnable = true;
}

void HwcTestConfig::DisableAllChecks() {
  mGlobalEnable = false;
}

void HwcTestConfig::SetCheck(HwcTestCheckType check, bool enable,
                             bool causesTestFail) {
  if (mCheckConfigs[check].forceDisable) {
    mCheckConfigs[check].enable = false;
    mCheckConfigs[check].causesTestFail = false;
  } else {
    mCheckConfigs[check].enable = enable;
    mCheckConfigs[check].causesTestFail = causesTestFail;
  }
}

void HwcTestConfig::SetComponentEnabled(uint32_t component, bool enable,
                                        bool causesTestFail) {
  mComponentEnabled[component] = causesTestFail;

  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    if (mCheckComponents[i] == component) {
      // Don't enable optional checks even if they are in the right category
      if (mCheckConfigs[i].category != eCatOpt) {
        SetCheck((HwcTestCheckType)i, enable, causesTestFail);
      }
    }
  }
}

bool HwcTestConfig::IsComponentEnabled(uint32_t component) {
  return mComponentEnabled[component];
}

const char* HwcTestConfig::GetComponentEnableStr(uint32_t component) {
  return IsComponentEnabled(component) ? "" : "[DISABLED]";
}

const char* HwcTestConfig::Str(PanelModeType panelMode) {
  switch (panelMode) {
    case HwcTestConfig::eOn:
      return "On";

    case HwcTestConfig::eOff:
      return "Off";

    case HwcTestConfig::eDontCare:
      return "Undefined";

    default:
      ALOG_ASSERT(0);
      return 0;
  }
}

// Convert check name to check type
HwcTestCheckType HwcTestConfig::CheckFromName(const char* checkName) {
  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    if (strstr(checkName, formalNames[i]) != 0) {
      return (HwcTestCheckType)i;
    }
  }

  return eCheckTestFail;
}

HwcTestResult::HwcTestResult()
    : mHwcCompValSkipped(0),
      mHwcCompValCount(0),
      mSfCompValSkipped(0),
      mSfCompValCount(0) {
  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    mCheckFailCount[i] = 0;
    mCheckEvalCount[i] = 0;
  }

  for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
    mPerDisplay[i].mMaxConsecutiveDroppedFrameCount = 0;
    mPerDisplay[i].mDroppedFrameCount = 0;
    mPerDisplay[i].mFrameCount = 0;
  }
}

HwcTestResult& HwcTestResult::operator+=(HwcTestResult& rhs) {
  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    mCheckEvalCount[i] += rhs.mCheckEvalCount[i];
    mCheckFailCount[i] += rhs.mCheckFailCount[i];
  }

  for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
    mPerDisplay[i].mMaxConsecutiveDroppedFrameCount +=
        rhs.mPerDisplay[i].mMaxConsecutiveDroppedFrameCount;
    mPerDisplay[i].mDroppedFrameCount += rhs.mPerDisplay[i].mDroppedFrameCount;
    mPerDisplay[i].mFrameCount = rhs.mPerDisplay[i].mFrameCount;
  }

  mHwcCompValSkipped += rhs.mHwcCompValSkipped;
  mHwcCompValCount += rhs.mHwcCompValCount;
  mSfCompValSkipped += rhs.mSfCompValSkipped;
  mSfCompValCount += rhs.mSfCompValCount;

  mStartTime = rhs.mStartTime;
  mEndTime = rhs.mEndTime;

  return *this;
}

void HwcTestResult::CopyPriorities(HwcTestConfig& config) {
  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    mFinalPriority[i] = config.mCheckConfigs[i].priority;
    mCausesTestFail[i] = config.mCheckConfigs[i].causesTestFail;
  }
}

/// Set final check priority
void HwcTestResult::SetFinalPriority(HwcTestCheckType check, int priority) {
  mFinalPriority[check] = priority;
}

/// Set final check priority conditionally
/// to reducedPriority if failure count <= maxNormCount
void HwcTestResult::ConditionalDropPriority(HwcTestCheckType check,
                                            uint32_t maxNormCount,
                                            int reducedPriority) {
  if (mCheckFailCount[check] <= maxNormCount) {
    mFinalPriority[check] = reducedPriority;
  }
}

/// Set final check priority conditionally
/// to increasedPriority if failure count > maxNormCount
void HwcTestResult::ConditionalRevertPriority(HwcTestConfig& config,
                                              HwcTestCheckType check,
                                              uint32_t maxNormCount) {
  if (mCheckFailCount[check] > maxNormCount) {
    mFinalPriority[check] = config.mCheckConfigs[check].priority;
  }
}

bool HwcTestResult::IsGlobalFail() {
  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    if ((mCheckFailCount[i] > 0) && (mFinalPriority[i] >= ANDROID_LOG_ERROR) &&
        mCausesTestFail[i]) {
      return true;
    }
  }
  return false;
}

void HwcTestResult::Reset(HwcTestConfig* config) {
  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    if ((config == 0) ||
        (config->mCheckConfigs[i].category != eCatStickyTest)) {
      mCheckEvalCount[i] = 0;
      mCheckFailCount[i] = 0;
    }
  }

  mHwcCompValSkipped = 0;
  mHwcCompValCount = 0;
  mSfCompValSkipped = 0;
  mSfCompValCount = 0;

  mStartTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

void HwcTestResult::SetEndTime() {
  mEndTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

void HwcTestResult::SetStartEndTime(int64_t startTime, int64_t endTime) {
  mStartTime = startTime;
  mEndTime = endTime;
}

int HwcTestResult::ReportE(HwcTestCheckType check, HwcTestConfig* config) {
  if (!config) {
    return ANDROID_LOG_ERROR;
  }

  SetFail(check);
  int priority = config->mCheckConfigs[check].priority;
  HwcValLog(priority, "%s", HwcTestConfig::mCheckDescriptions[check]);

  if (priority == ANDROID_LOG_FATAL) {
    Hwcval::ValCallbacks::DoExit();
  }

  return priority;
}

void HwcTestResult::Log(HwcTestConfig& config, const char* testName,
                        bool brief) {
  double time = double(mEndTime - mStartTime) / 1000000000.0;

  if (brief) {
    LogTestPassFail(testName);
  }

  for (uint32_t component = eComponentMin; component < eComponentMax;
       ++component) {
    bool componentTitleNeeded = false;
    bool componentTitleNeededInBriefOutput = false;
    for (uint32_t check = 0; check < eHwcTestNumChecks; ++check) {
      if ((HwcTestConfig::GetComponent(check) == component) &&
          config.IsEnabled((HwcTestCheckType)check)) {
        if (mCheckFailCount[check] > 0) {
          componentTitleNeeded = true;
          if (((mFinalPriority[check] >= ANDROID_LOG_ERROR) ||
               (config.mCheckConfigs[check].category ==
                eCatPriWarn))  // or this is a priority warning
              &&
              mCausesTestFail[check]) {
            componentTitleNeededInBriefOutput = true;
          }
        }
      }
    }

    if (!componentTitleNeeded) {
      continue;
    }

    const char* componentTitlePrefix = "";
    if (brief) {
      if (componentTitleNeededInBriefOutput) {
        componentTitlePrefix = "  ";
      } else {
        componentTitlePrefix = "##";
      }
    }

    printf("%sCOMPONENT: %s %s\n", componentTitlePrefix,
           HwcTestConfig::mComponentNames[component],
           config.GetComponentEnableStr(component));

    // Print which checks failed, highest priority firt
    for (uint32_t priority = ANDROID_LOG_FATAL; priority >= ANDROID_LOG_INFO;
         --priority) {
      const char* priorityStr;
      const char* prefix = componentTitlePrefix;
      switch (priority) {
        case ANDROID_LOG_WARN:
          priorityStr = "warnings";
          if (brief) {
            prefix = "##";
          }
          break;

        case ANDROID_LOG_ERROR:
          priorityStr = "errors";
          break;

        case ANDROID_LOG_FATAL:
          priorityStr = "fatal errors";
          break;

        default:
          priorityStr = "messages";
          if (brief) {
            prefix = "##";
          }
      }

      for (uint32_t check = 0; check < eHwcTestNumChecks; ++check) {
        if ((HwcTestConfig::GetComponent(check) == component) &&
            config.IsEnabled((HwcTestCheckType)check) &&
            (mFinalPriority[check] == priority)) {
          const char* checkPrefix = mCausesTestFail[check] ? prefix : "##";

          if (mCheckFailCount[check] > 0) {
            if (mCheckEvalCount[check] > 0) {
              printf("%s    %s: %d/%d %s\n", checkPrefix,
                     HwcTestConfig::GetDescription(check),
                     mCheckFailCount[check], mCheckEvalCount[check],
                     priorityStr);
            } else {
              printf("%s    %s: %d %s\n", checkPrefix,
                     HwcTestConfig::GetDescription(check),
                     mCheckFailCount[check], priorityStr);
            }
          }
        }
      }
    }

    printf("%s\n", componentTitlePrefix);
  }

  const char* prefix = "";
  if (brief) {
    prefix = "##";
  }

  if (config.IsEnabled(eCheckHwcCompMatchesRef)) {
    printf("%sHWC Composition: %d done, %d skipped\n", prefix, mHwcCompValCount,
           mHwcCompValSkipped);
  }

  if (config.IsEnabled(eCheckSfCompMatchesRef)) {
    printf("%sSF Composition: %d done, %d skipped\n", prefix, mSfCompValCount,
           mSfCompValSkipped);
  }

  for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
    HwcTestResult::PerDisplay* perDisp = mPerDisplay + i;

    double fps = perDisp->mFrameCount / time;

    printf("D%d: %sFrames: %d in %3.1fs (%2.1ffps)\n", i, prefix,
           perDisp->mFrameCount, time > 0 ? time : 0.0, isnan(fps) ? 0.0 : fps);

    if (!brief && perDisp->mDroppedFrameCount > 0) {
      printf("D%d: %d dropped frames (max %d consecutive)\n", i,
             perDisp->mDroppedFrameCount,
             perDisp->mMaxConsecutiveDroppedFrameCount);
    }
  }

  if (!brief) {
    LogTestPassFail(testName);
    printf("\n");
  }
}

void HwcTestResult::LogTestPassFail(const char* testName) {
  if (!IsGlobalFail()) {
    printf("*** Test PASSED: %s\n", testName);
  } else {
    printf("*** Test FAILED: %s\n", testName);
  }
}

void Hwcval::ValCallbacks::Set(Hwcval::ValCallbacks* valCallbacks) {
  mInstance = valCallbacks;
}

void Hwcval::ValCallbacks::DoExit() {
  if (mInstance) {
    mInstance->Exit();
  }
}

Hwcval::ValCallbacks* Hwcval::ValCallbacks::mInstance = 0;
