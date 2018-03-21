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

// TODO get display info DRM base class?

#include "test_base.h"
#include "HwcTestLog.h"
#include "HwcTestConfig.h"
#include <ctype.h>

#undef LOG_TAG
#define LOG_TAG "VAL_HWC_SURFACE_SENDER"

HwcTestBase* HwcTestBase::mTheTestBase = 0;

HwcTestBase::HwcTestBase(int argc, char** argv)
    : mpDisplay(0),
      mNoShims(false),
      mValHwc(true),
      mValSf(false),
      mValDisplays(false),
      mValBuffers(false),
      mValHwcComposition(false) {
  HWCLOGI("Start of HwcTestBase ctor");
  mTheTestBase = this;

  mStartTime = 0;
  mCurrentTime = 0;
  mTestFrameCount = 0;
  mTestRunTime = 0;
  mTestRunTimeOverridden = false;
  mTestEndCondition = etetNone;

  SetArgs(argc, argv);
  if (!mNoShims) {
    HWCLOGI("Binder of HwcTestBase ctor");
    ConnectToShimBinder();
  }

  mpDisplay = new Display();

  HWCLOGI("End of HwcTestBase ctor");
}

HwcTestBase::~HwcTestBase() {
  mTheTestBase = 0;
}

void HwcTestBase::SetDefaultChecks(void) {
  mConfig.Initialise(mValHwc, mValDisplays, mValBuffers, mValSf, mValHwc);
  // Don't enable UX checks by default
}

int HwcTestBase::CreateSurface(SurfaceSender::SurfaceSenderProperties sSSP) {
  // This is done here so SurfaceSender does not need to worry about the
  // display once created
  if (sSSP.GetUseScreenWidth() == true) {
    sSSP.SetWidth(mpDisplay->GetWidth());
  }

  if (sSSP.GetUseScreenHeight() == true) {
    sSSP.SetHeight(mpDisplay->GetHeight());
  }

  SurfaceSender* sender = new SurfaceSender(sSSP);
  std::shared_ptr<SurfaceSender>* spSender = new std::shared_ptr<SurfaceSender>(sender);

  SurfaceSenders.push_back(spSender);

  // TODO
  return 0;
}

bool HwcTestBase::ContinueTest(void) {
  if (mTestEndCondition == etetRunTime) {
    // Get current time

    mCurrentTime = GetTime();

    if ((mCurrentTime - mStartTime) > mTestRunTime) {
      return false;
    }
  } else if (mTestEndCondition == etetFrameCount &&
             (mTestFrameCount > mFrameCount)) {
    return false;
  } else {
    return false;
  }

  return true;
}

void HwcTestBase::LogTestResult(HwcTestConfig& config) {
  // Copy priorities from config to result
  mResult.CopyPriorities(config);

  // Print the results
  mResult.Log(config, mTestName.c_str(), false);

  if (!mResult.IsGlobalFail()) {
    // TODO: Do we need to retain this format for some reason or can we discard
    // the unnecessary logging?
    HWCLOGI("Passed : 1");
    HWCLOGI("Failed : 0");
    HWCLOGI("Skipped: 0");
    HWCLOGI("Error  : 0");
  } else {
    HWCLOGI("Passed : 0");
    HWCLOGI("Failed : 1");
    HWCLOGI("Skipped: 0");
    HWCLOGI("Error  : 0");
  }
}

void HwcTestBase::LogTestResult() {
  LogTestResult(mConfig);
}

void HwcTestBase::SetTestEndType(eTestEndType type) {
  if (type == etetRunTime && mTestRunTime == 0) {
    HWCERROR(eCheckSessionFail, "Test runtime not set %d",
             (int32_t)mTestRunTime);
  }

  if (type >= etetNumberOfTestTypes) {
    HWCERROR(eCheckSessionFail, "Invalid Test type %d", (uint32_t)type);
  }

  mTestEndCondition = type;
  HWCLOGI("SetTestRunType %x", (uint32_t)mTestEndCondition);
}

HwcTestBase::eTestEndType HwcTestBase::GetTestEndType(void) {
  return mTestEndCondition;
}

void HwcTestBase::ConnectToShimBinder(void) {

  mHwcService = HwcService_Connect();

  if (mHwcService == NULL) {
    HWCERROR(eCheckSessionFail, "Error getting mHwcService");
    printf("TEST FAIL: SHIMS NOT INSTALLED\n");
    exit(1);
  }
}

void HwcTestBase::CheckTestEndType() {
  if (mTestEndCondition == etetFrameCount) {
    if (mTestFrameCount == 0) {
      HWCERROR(eCheckSessionFail, "No frame count");
    }
  } else if (mTestEndCondition == etetRunTime) {
    if (mTestRunTime == 0) {
      HWCERROR(eCheckSessionFail, "No test run time set");
    }
  } else if (mTestEndCondition == etetUserDriven) {
    // No parameter needed
  } else {
    // No valid test type
    HWCERROR(eCheckSessionFail, "Invalid test type: %d",
             (uint32_t)mTestEndCondition);
  }
}

void HwcTestBase::SetTestRunTime(int64_t runTimeMs) {
  if (mTestRunTimeOverridden) {
    HWCLOGI(
        "HwcTestBase::SetTestRunTime - request to run %lldms ignored, using "
        "command line override of %lldms",
        runTimeMs, mTestRunTime);
  } else {
    mTestRunTime = runTimeMs;
  }
}

int64_t HwcTestBase::GetTime() {
  return android::elapsedRealtime();
}

status_t HwcTestBase::InitialiseChecks() {
  if (!mNoShims)
    SetChecks();
  CheckTestEndType();

  status_t initOK = NO_ERROR;

  HWCLOGI("HwcTestBase::InitialiseChecks mNoShims=%u", mNoShims);
  if (!mNoShims) {
    // Send test configuration to shims and reset failure counts
    uint32_t display = 0;  //need to set correct value
    uint32_t display_mode_index = 0; //need to set correct value
    HwcService_DisplayMode_SetMode(mHwcService, display, display_mode_index);
  }

  return initOK;
}

void HwcTestBase::DebriefChecks(bool disableAllChecks) {
  HWCLOGI("HwcTestBase::DebriefChecks()");
  if (!mNoShims) {
    HwcTestResult testResult;

    //support required from hwcservice
    // Combine remote errors with local failures
    HWCLOGF("Not implemented");
    mResult += testResult;
  }
}

void HwcTestBase::GetOldConfig(HwcTestConfig& config) {
  if (!mNoShims) {
    //support required from hwcservice
    HWCLOGF("Not implemented");
  }
}

void HwcTestBase::SetLoggingLevelToDefault() {
  mConfig.mMinLogPriority = ANDROID_LOG_WARN;
    //support required from hwcservice
    HWCLOGF("Not implemented");
}

int HwcTestBase::StartTest(void) {
  HWCLOGI("HwcTestBase::StartTest() mTestName=%s", mTestName.c_str());

  status_t initOK = InitialiseChecks();

  if (initOK != NO_ERROR) {
    HWCERROR(eCheckSessionFail, "Binder error: %d", initOK);
    LogTestResult();
    return 1;
  }

  // If some  error occurred in the setup do not run the test
  if (!mResult.IsGlobalFail()) {
    mStartTime = GetTime();

    for (uint32_t i = 0; i < SurfaceSenders.size(); ++i) {
      SurfaceSender* ss = SurfaceSenders[i]->get();

      ss->Start();
    }

    // If setup was ok start test loop
    // TODO remove
    bool keepGoing = true;

    // Start surfaces
    while (keepGoing) {
      for (uint32_t i = 0; i < SurfaceSenders.size(); ++i) {
        SurfaceSender* ss = SurfaceSenders[i]->get();

        if (!ss->Iterate()) {
          HWCERROR(eCheckSurfaceSender,
                   "HwcTestBase::StartTest - ERROR: test aborted");
          keepGoing = false;
          break;
        }
      }

      // ChecktoSee if the test should continue
      // Could this to the frame count increment
      if (keepGoing) {
        keepGoing = ContinueTest();
      }
    }

    HWCLOGD("Disabling surface sender");
    for (uint32_t i = 0; i < SurfaceSenders.size(); ++i) {
      SurfaceSender* ss = SurfaceSenders[i]->get();

      ss->End();
    }

    HWCLOGD("Getting debrief");
    DebriefChecks();
  } else {
    // The flow only got here if a error was already set so there is no need
    // to record the error again.
    printf("Setup error occurred TEST NOT RUN.");
  }

  // If the control flow got here everything was super
  LogTestResult();
  return mResult.IsGlobalFail() ? 1 : 0;
}

void HwcTestBase::SetArgs(int argc, char** argv) {
  mArgc = argc;
  mArgv = argv;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-no_shims") == 0) {
      mNoShims = true;
    } else if (strncmp(argv[i], "-t=", 3) == 0) {
      int s = atoi(&argv[i][3]);
      if (s > 0) {
        mTestRunTime = int64_t(s) * 1000;
        mTestRunTimeOverridden = true;
      }
    } else if (strcmp(argv[i], "-crc") == 0) {
      mConfig.SetCheck(eCheckCRC);
    } else if (strcmp(argv[i], "-val_hwc_composition") == 0) {
      mValHwcComposition = true;
    } else if (strcmp(argv[i], "-no_val_hwc") == 0) {
      mValHwc = false;
    } else if (strcmp(argv[i], "-val_sf") == 0) {
      mValSf = true;
    } else if (strcmp(argv[i], "-val_displays") == 0) {
      mValDisplays = true;
    } else if (strcmp(argv[i], "-val_buffer_allocation") == 0) {
      mValBuffers = true;
    } else if (strncmp(argv[i], "-log_pri=", 9) == 0) {
      const char* logPri = argv[i] + 9;
      int priority;

      switch (toupper(*logPri)) {
        case 'V':
          priority = ANDROID_LOG_VERBOSE;
          break;

        case 'D':
          priority = ANDROID_LOG_DEBUG;
          break;

        case 'I':
          priority = ANDROID_LOG_INFO;
          break;

        case 'W':
          priority = ANDROID_LOG_WARN;
          break;

        case 'E':
          priority = ANDROID_LOG_ERROR;
          break;

        case 'F':
          priority = ANDROID_LOG_FATAL;
          break;

        default:
          priority = ANDROID_LOG_ERROR;
      }

      mConfig.mMinLogPriority = priority;
    }
  }
}

void HwcTestBase::PrintArgs() const {
  printf("command line arguments:-\n");
  printf("-no_shims            # disables the shims during the test\n");
  printf("-t=<s>               # overrides the test run time to <s> seconds\n");
  printf(
      "-val_hwc_composition # Enable validtion of HWC composition against "
      "reference composer using SSIM\n");
  printf(
      "-log_pri=<p>         # Sets the minimum priority to appear in the log. "
      "<p>=V|D|I|W|E|F\n");
  printf("\n");
}

void HwcTestBase::DumpSurfaces(
    SurfaceSender::SurfaceSenderProperties surfaceProperties) {
  HWCLOGI("Surface %s layer %d", surfaceProperties.GetSurfaceName(),
          surfaceProperties.GetLayer());
  HWCLOGI("  Use screen w:%d h:%d", surfaceProperties.GetUseScreenWidth(),
          surfaceProperties.GetUseScreenHeight());
  HWCLOGI("  wxh: %dx%d, offset: %dx%d", surfaceProperties.GetWidth(),
          surfaceProperties.GetHeight(), surfaceProperties.GetXOffset(),
          surfaceProperties.GetYOffset());
  HWCLOGI(" cs %d, colour %x", surfaceProperties.GetColorSpace(),
          surfaceProperties.GetRGBAColor());
}

HwcTestConfig defaultTestConfig;
HwcTestResult defaultTestResult;

HwcTestResult* HwcGetTestResult() {
  HwcTestBase* testBase = HwcTestBase::GetTestBase();
  if (testBase) {
    return &(testBase->GetResult());
  } else {
    return &defaultTestResult;
  }
}

HwcTestConfig* HwcGetTestConfig() {
  HwcTestBase* testBase = HwcTestBase::GetTestBase();
  if (testBase) {
    return &(testBase->GetConfig());
  } else {
    return &defaultTestConfig;
  }
}

// Include logging
// No hardware composer log available in test.
#undef HWCVAL_LOG_ANDROIDONLY
#define HWCVAL_LOG_ANDROIDONLY
#define HWCVAL_IN_TEST
#include "HwcTestLog.cpp"
