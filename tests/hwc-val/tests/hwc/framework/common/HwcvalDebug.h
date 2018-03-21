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

#ifndef INTEL_HWCVAL_DEBUG_H
#define INTEL_HWCVAL_DEBUG_H

#include <cutils/log.h>

#include <hardware/hwcomposer2.h>
#include <string>

// Trace support
#include <utils/Trace.h>

// Mutex support
#include <utils/Mutex.h>
#include <utils/Condition.h>

namespace android {
class Mutex;
class Condition;
};

namespace Hwcval {

#define MUTEX_CONDITION_DEBUG 0  // Debug mutex/conditions.

using namespace android;

// This ScopedTrace function compiles away properly when disabled. Android's one
// doesnt, it
// leaves strings and atrace calls in the code.
class HwcvalScopedTrace {
 public:
  inline HwcvalScopedTrace(bool bEnable, const char* name) : mbEnable(bEnable) {
    if (mbEnable)
      atrace_begin(ATRACE_TAG_GRAPHICS, name);
  }

  inline ~HwcvalScopedTrace() {
    if (mbEnable)
      atrace_end(ATRACE_TAG_GRAPHICS);
  }

 private:
  bool mbEnable;
};

// Conditional variants of the macros in utils/Trace.h
#define ATRACE_INT_IF(enable, name, value) \
  do {                                     \
    if (enable) {                          \
      ATRACE_INT(name, value);             \
    }                                      \
  } while (0)
#define ATRACE_EVENT_IF(enable, name) \
  do {                                \
    ATRACE_INT_IF(enable, name, 1);   \
    ATRACE_INT_IF(enable, name, 0);   \
  } while (0)

// Wrapper Mutex and Condition classes that add some debug and trap deadlocks.
class Mutex {
 public:
  static const uint64_t mLongTime = 1000000000;  //< 1 second.
  static const uint32_t mSpinWait = 1000;        //< 1 millisecond.
  Mutex();
  Mutex(const char* name);
  Mutex(int type, const char* name = NULL);
  ~Mutex();
  int lock();
  int unlock();
  bool isHeld(void);
  void incWaiter(void);
  void decWaiter(void);
  uint32_t getWaiters(void);
  class Autolock {
   public:
    Autolock(Mutex& m);
    ~Autolock();

   private:
    Mutex& mMutex;
  };

  // lock if possible; returns 0 on success, error otherwise
  status_t tryLock();

 private:
  friend class Condition;
  bool mbInit : 1;
  android::Mutex mMutex;
  pid_t mTid;
  nsecs_t mAcqTime;
  uint32_t mWaiters;
};

class Condition {
 public:
  Condition();
  ~Condition();
  int waitRelative(Mutex& mutex, nsecs_t timeout);
  int wait(Mutex& mutex);
  void signal();
  void broadcast();

 private:
  bool mbInit : 1;
  uint32_t mWaiters;
  android::Condition mCondition;
};

};  // namespace Hwcval

#endif  // INTEL_UFO_HWC_DEBUG_H
