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

#ifndef __HwchWatchdogThread_h__
#define __HwchWatchdogThread_h__

#include "HwchDefs.h"
#include "HwcTestConfig.h"
#include "hwcthread.h"

class HwcTestRunner;

namespace Hwch {
class WatchdogThread : public hwcomposer::HWCThread, public Hwcval::ValCallbacks {
 public:
  WatchdogThread(HwcTestRunner* runner);
  virtual ~WatchdogThread();

  void Set(uint32_t minMinutes, float minFps);
  void Start();
  void Stop();
  void Exit();
  void requestExitAndWait();
 private:
  // Thread functions
  void HandleRoutine();

  // Private data
  // Minimum run time in ns before checks start
  uint64_t mMinNs;

  // Minimum frame rate in fps to be achieved after the minimum test run time
  // has expired
  float mMinFps;

  // Time the test started
  volatile int64_t mStartTime;

  // The test runner
  HwcTestRunner* mRunner;
};
}

#endif  // __HwchWatchdogThread_h__
