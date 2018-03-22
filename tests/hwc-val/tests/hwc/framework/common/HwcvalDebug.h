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
#include "public/spinlock.h"
#include "public/hwcutils.h"
#include "common/utils/hwcevent.h"
#include <sys/time.h>
using namespace hwcomposer;

namespace Hwcval {

#define MUTEX_CONDITION_DEBUG 0  // Debug mutex/conditions.

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
    SpinLock& spinlock_;
  };

  // lock if possible; returns 0 on success, error otherwise
  bool tryLock();

 private:
  friend class Condition;
  bool mbInit : 1;
  SpinLock mspinlock;
  pid_t mTid;
  timespec mAcqTime;
  uint32_t mWaiters;
};

class Condition {
 public:
  Condition();
  ~Condition();
  int waitRelative(Mutex& mutex, unsigned long timeout) {
  ALOGD_IF(MUTEX_CONDITION_DEBUG,
           "Condition %p waitRelative Enter mutex %p mTid/tid %d/%d", this,
           &mutex, mutex.mTid, gettid());
  ALOG_ASSERT(mbInit);
  ALOG_ASSERT(mutex.mTid == gettid());
  mutex.mTid = 0;
  mutex.incWaiter();
  mWaiters++;
  ALOGD_IF(MUTEX_CONDITION_DEBUG,
           "Condition %p waitRelative on mutex %p waiters %u/%u", this, &mutex,
           mWaiters, mutex.getWaiters());
  mutex.lock();
  int ret = HWCPoll(hwcevent.get_fd(), timeout);
  mutex.decWaiter();
  mWaiters--;
  ALOGD_IF(MUTEX_CONDITION_DEBUG,
           "Condition %p re-acquired mutex %p waiters %u/%u", this, &mutex,
           mWaiters, mutex.getWaiters());
  mutex.mTid = gettid();
  clock_gettime(CLOCK_REALTIME, &mutex.mAcqTime);
  return ret;
}
  int wait(Mutex& mutex);
  void signal();
  void broadcast();

 private:
  bool mbInit : 1;
  uint32_t mWaiters;
  HWCEvent hwcevent;
};

};  // namespace Hwcval

#endif  // INTEL_UFO_HWC_DEBUG_H
