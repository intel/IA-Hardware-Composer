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
#include <vector>
#include <map>
#include <string>
#include <utils/Mutex.h>
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestDefs.h"
#include "HwcTestUtil.h"
#include "HwchWatchdogThread.h"
#include "HwchDisplaySpoof.h"
#include "HwcvalStatistics.h"

#include <dirent.h>

class HwcTestRunner : public Hwch::TestParams {
 private:
  Hwch::Interface& mInterface;
  std::vector<std::string> mTestNames;
  std::vector<std::string> mAvoidNames;
  std::vector<Hwch::Test*> mTests;
  Hwch::Test* mCurrentTest;
  HwcTestState* mState;

  std::map<std::string, HwcTestResult> mResults;
  uint32_t mNumPasses;
  uint32_t mNumFails;
  std::string mFailedTests;
  int64_t mStartTime;
  int64_t mEndTime;
  Hwch::ParamVec mParams;
  std::string mLogName;
  std::string mHwclogPath;
  bool mBrief;
  bool mNoShims;

  uint32_t mTestNum;
  std::string mTestName;
  std::string mArgs;  // all-test arguments for logging
  bool mAllTests;
  bool mHWCLReplay;
  bool mDSReplay;
  uint32_t mDSReplayNumFrames;
  uint32_t mReplayMatch;
  const char* mReplayFileName;
  bool mReplayNoTiming;
  bool mReplayTest;
  float mWatchdogFps;

  Hwch::DisplaySpoof mDisplayFailSpoof;

  Hwcval::Mutex mExitMutex;
  Hwch::WatchdogThread mWatchdog;
  Hwch::System& mSystem;

  // Statistics
  Hwcval::Statistics::Value<double> mRunTimeStat;

 public:
  HwcTestRunner(Hwch::Interface& interface);
  int getargs(int argc, char** argv);
  void SetBufferConfig();
  void SetRunnerParams();
  void EntryPriorityOverride();
  void LogTestResult();
  void LogTestResult(const char* testName, const char* args);
  void WriteCsvFile();
  void WriteDummyCsvFile();
  void ParseCSV(const char* p, std::vector<std::string>& sv);
  void CombineFiles(int err);
  int CreateTests();
  int RunTests();
  void CRCTerminate(HwcTestConfig& config);
  void ExitChecks();
  void LogSummary();
  void Lock();
  void Unlock();
  void ConfigureState();

  // Enable/disable display spoof
  void EnableDisplayFailSpoof(const char* str);

  // Configure stalls based on command-line options
  void ConfigureStalls();

  // Configure frames where inputs are to be dumped
  void ConfigureFrameDump();

  // Check version consistency and report
  void ReportVersion();

 private:
  Hwch::Test* NextTest();
  FILE* OpenCsvFile();
  void ConfigureStall(Hwcval::StallType ix, const char* optionName);
  FILE* mStatsFile;
};
