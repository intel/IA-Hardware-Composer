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

#ifndef __HwcvalWatchdog_h__
#define __HwcvalWatchdog_h__

#include <stdint.h>
#include "HwcTestState.h"
#include <string>

namespace Hwcval {

class Watchdog  {
 public:
  Watchdog(uint64_t ns, HwcTestCheckType check,
           const char* str = "Watchdog timer");
  Watchdog(Watchdog& rhs);

  void SetMessage(const std::string& str);
  void Start();
  void StartIfNotRunning();
  void Stop();
  int64_t GetStartTime();

 private:
  static void TimerHandler(sigval_t value);
  void TimerHandler();

  uint64_t mTimeoutNs;
  bool mHaveTimer;
  bool mRunning;
  timer_t mDelayTimer;
  int64_t mStartTime;

  HwcTestCheckType mCheck;
  std::string mMessage;
};

inline int64_t Watchdog::GetStartTime() {
  return mStartTime;
}

}  // namespace Hwcval

#endif  // __HwcvalWatchdog_h__
