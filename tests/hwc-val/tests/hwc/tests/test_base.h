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

#ifndef __HWC_TEST_BASE_H__
#define __HWC_TEST_BASE_H__

// TODO get display info DRM base class?

#include "SurfaceSender.h"
#include "display_info.h"

#include "HwcTestDefs.h"
#include "HwcTestConfig.h"
#include "hwcserviceapi.h"
#include <utils/SystemClock.h>
#include <cutils/properties.h>
#include <unistd.h>

#define NANO_TO_MICRO 1000

HwcTestResult* HwcGetTestResult();
HwcTestConfig* HwcGetTestConfig();

class HwcTestBase {
  // TODO Test place holder
  /// \futurework The idea 1

 public:
  HwcTestBase(int argc, char** argv);

  virtual ~HwcTestBase();

  /// Main entry point for test implmented in each test.
  /// The implementation should set the test end type and check and create some
  /// surfaces
  virtual int Run(void) = 0;

  /// The test must implement a function set the required shim checks
  /// These are then send by ShimSetUpSendSetChecks implemented in this class.
  virtual int SetChecks(void) = 0;

  /// Set all checks as enabled.
  // This function is provided for convince for use in SetChecks().
  void SetDefaultChecks(void);

  // Test setup functions
  /// Ways the test may end
  enum eTestEndType {
    etetNone,        // default so this can be test for not being set
    etetFrameCount,  // Not currently support frame count needs incrementing
    etetRunTime,
    etetUserDriven,
    etetNumberOfTestTypes
  };

  /// Error  from the test
  enum eTestErrorStatusType {
    etestNoError = 0,
    etestBinderError,
    etestUnknownRunType,
    etestIncorrectRunTypeSettingType
  };

  /// predefined test types
  enum eTestLength {
    etlTenSeconds = 10000,
  };

  // Test functions
  /// Set the check required
  int SetCheckEnable(bool enable);

  /// TODO
  void CheckTestEndType();

  /// Set the  run length of the test
  void SetTestRunTime(int64_t runTimeMs);
  /// Get the current time
  int64_t GetTime(void);

  /// Get the end  type of the test
  eTestEndType GetTestEndType(void);
  /// Set the way to end the test
  void SetTestEndType(eTestEndType type);

  /// Start test inlcudes checks that the test has a sesnibale set up.
  int StartTest(void);
  bool ContinueTest(void);
  /// Setup checks in shims
  status_t InitialiseChecks();
  /// Get completion status of checks and turn them off
  void DebriefChecks(bool disableAllChecks = true);
  /// Read configuration from shims
  void GetOldConfig(HwcTestConfig& config);
  /// Turn down logging level to reduce change of unattended system lockup
  void SetLoggingLevelToDefault();

  /// Preserve command-line arguments
  void SetArgs(int argc, char** argv);

  void LogTestResult(HwcTestConfig& config);
  void LogTestResult();

  /// Functions for managing surfaces  in the test
  /// Add a new surface to the SurfaceSenders vector
  int CreateSurface(SurfaceSender::SurfaceSenderProperties sSSP);

  /// Binder functions
  void ConnectToShimBinder(void);

  /// Debug - print surfaces
  void DumpSurfaces(SurfaceSender::SurfaceSenderProperties surfaceProperties);

  void PrintArgs() const;

  static HwcTestBase* GetTestBase();  // For logging

  HwcTestConfig& GetConfig();
  HwcTestResult& GetResult();

 private:
  /// A list of surface providing objects
  Vector<std::shared_ptr<SurfaceSender>*> SurfaceSenders;
  /// A display class to return display properties
  Display* mpDisplay;

 protected:
  std::string mTestName;
  HWCSHANDLE mHwcService;

  eTestEndType mTestEndCondition;
  /// Run length of test, need a better name
  uint32_t mTestFrameCount;
  /// Length the of time the test should run
  int64_t mTestRunTime;
  /// true if the test run time has been passed as a command line parameter
  bool mTestRunTimeOverridden;
  /// Updated when sending a frame, rename to Current... ? or something
  uint32_t mFrameCount;
  /// Time a call was made
  int64_t mStartTime;
  int64_t mCurrentTime;

  /// Command-line argument count
  int mArgc;
  /// Command-line arguments
  char** mArgv;

  /// Disable interface with shims
  bool mNoShims;

  // Check enable components
  bool mValHwc;
  bool mValSf;
  bool mValDisplays;
  bool mValBuffers;

  // Additional check enables
  bool mValHwcComposition;

  /// Stored pointer to this, for logging
  static HwcTestBase* mTheTestBase;
  HwcTestConfig mConfig;
  HwcTestResult mResult;
};

// For logging
inline HwcTestBase* HwcTestBase::GetTestBase() {
  return mTheTestBase;
}

inline HwcTestConfig& HwcTestBase::GetConfig() {
  return mConfig;
}

inline HwcTestResult& HwcTestBase::GetResult() {
  return mResult;
}

#endif  // __HWC_TEST_BASE_H__
