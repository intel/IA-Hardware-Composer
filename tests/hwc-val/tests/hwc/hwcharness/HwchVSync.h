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

#ifndef __HwcVSync_h__
#define __HwcVSync_h__

#include "HwcvalDebug.h"

namespace Hwch {

class VSync {
 public:
  VSync();
  virtual ~VSync();

  // Set the delay in microseconds between VSync and condition being signalled.
  void SetVSyncDelay(uint32_t delayus);

  // Set the timeout in microseconds for when VSync does not come.
  void SetTimeout(uint32_t timeoutus);

  // Set the period we will use when VSyncs don't come within the timeout.
  void SetRequestedVSyncPeriod(uint32_t periodus);

  // Stop handling VSyncs
  void Stop();

  void WaitForOffsetVSync();

  // VSync callback
  virtual void Signal(uint32_t disp);

  bool IsActive();

 private:
  // Delay in nanoseconds between VSync and condition being signalled.
  uint32_t mDelayns;

  // Timeout in ns for when VSyncs don't occur.
  uint32_t mTimeoutns;

  // Expected time between VSyncs. We will simulate this if the real ones don't
  // occur within the timeout period.
  uint32_t mRequestedVSyncPeriodns;

  timer_t mDelayTimer;

  Hwcval::Condition mCondition;
  Hwcval::Mutex mMutex;

  bool mActive;     // Vsyncs are happening
  bool mHaveTimer;  // Timer has been set up

  volatile int64_t mOffsetVSyncTime;
  int64_t mLastConsumedOffsetVSyncTime;

  static void TimerHandler(sigval_t value);
  void OffsetVSync();
  void DestroyTimer();
};

inline bool VSync::IsActive() {
  return mActive;
}
}

#endif  // __HwchVSync_h__
