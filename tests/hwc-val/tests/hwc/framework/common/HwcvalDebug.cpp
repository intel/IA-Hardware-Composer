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

#include "HwcvalDebug.h"

namespace Hwcval {

Mutex::Mutex() : mbInit(1), mTid(0),  mWaiters(0) {
}

Mutex::Mutex(const char* name)
    : mbInit(1), mTid(0), mWaiters(0) {
}
Mutex::Mutex(int type, const char* name)
    : mbInit(1), mTid(0), mWaiters(0) {
  type = 0;  // remove compiler warning.
}

Mutex::~Mutex() {
  mbInit = 0;
  ALOG_ASSERT(mTid == 0);
  ALOG_ASSERT(!mWaiters);
}

int Mutex::lock() {
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Acquiring mutex %p thread %u", this,
           gettid());
  ALOG_ASSERT(mbInit);
  if (mTid == gettid()) {
    ALOGE("Thread %u has already acquired mutex %p", gettid(), this);
    ALOG_ASSERT(0);
  }
  ATRACE_INT_IF(MUTEX_CONDITION_DEBUG,
                (std::string("W-Mutex-") + std::to_string((long long) this)).c_str(), 1);
  timespec timeStart;
  clock_gettime(CLOCK_REALTIME, &timeStart);
  timespec timeNow, timeEla;
  int timecount;
  for (timecount = 0;; timecount++) {
    timespec timeNow;
    clock_gettime(CLOCK_REALTIME, &timeNow);
    mspinlock.lock();
    ALOGD_IF((MUTEX_CONDITION_DEBUG && timecount == 0),
             "Blocking on mutex %p thread %u", this, gettid());
    usleep(mSpinWait);
    timeEla.tv_sec = timeNow.tv_sec - timeStart.tv_sec;
    timeEla.tv_nsec = timeNow.tv_nsec - timeStart.tv_nsec;
    if (timeEla.tv_nsec > mLongTime) {
      ALOGE("Thread %u blocked by thread %u waiting for mutex %p", gettid(),
            mTid, this);
      timeStart = timeNow;
    }
    if (timecount * mSpinWait > mLongTime * 10) {
      ALOGE("Fatal Thread %u blocked by thread %u waiting for mutex %p",
            gettid(), mTid, this);
      ALOG_ASSERT(0);
    }
  }
  ATRACE_INT_IF(MUTEX_CONDITION_DEBUG,
                (std::string("W-Mutex-") + std::to_string((long long) this)).c_str(), 0);
  ATRACE_INT_IF(MUTEX_CONDITION_DEBUG,
                (std::string("A-Mutex-") + std::to_string((long long) this)).c_str(), 1);
  mTid = gettid();
  mAcqTime = timeNow;
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Acquired mutex %p thread %u", this,
           gettid());
  return 0;
}

int Mutex::unlock() {
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Releasing mutex %p thread %u", this,
           gettid());
  ALOG_ASSERT(mbInit);
  if (mTid != gettid()) {
    ALOGE("Thread %u has not acquired mutex %p [mTid %u]", gettid(), this,
          mTid);
    ALOG_ASSERT(0);
  }
  timespec timeNow;
  clock_gettime(CLOCK_REALTIME, &timeNow);
  uint64_t timeEla = (uint64_t)int64_t(timeNow.tv_nsec - mAcqTime.tv_nsec);
  ALOGE_IF(timeEla > mLongTime, "Thread %u held mutex %p for %" PRIu64 "ms",
           mTid, this, timeEla / 1000000);
  mTid = 0;
  ATRACE_INT_IF(MUTEX_CONDITION_DEBUG,
                (std::string("A-Mutex-") + std::to_string((long long) this)).c_str(), 0);
  mspinlock.unlock();
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Released mutex %p thread %u", this,
           gettid());
  return 0;
}

bool Mutex::tryLock() {
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Testing mutex %p thread %u", this, gettid());
  mspinlock.lock();
  return true;
}

bool Mutex::isHeld(void) {
  return (mTid == gettid());
}

void Mutex::incWaiter(void) {
  ALOG_ASSERT(mbInit);
  ALOG_ASSERT(mTid == mTid);
  ++mWaiters;
}

void Mutex::decWaiter(void) {
  ALOG_ASSERT(mbInit);
  ALOG_ASSERT(mTid == mTid);
  --mWaiters;
}

uint32_t Mutex::getWaiters(void) {
  return mWaiters;
}

Mutex::Autolock::Autolock(Mutex& m) : spinlock_(m.mspinlock) {
  m.mspinlock.lock();
}
Mutex::Autolock::~Autolock() {
  spinlock_.unlock();
}

Condition::Condition() : mbInit(1), mWaiters(0) {
}

Condition::~Condition() {
  mbInit = 0;
  ALOG_ASSERT(!mWaiters);
}
#if 0
int Condition::waitRelative(Mutex& mutex, unsigned long timeout) {
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
#endif
int Condition::wait(Mutex& mutex) {
  ALOGD_IF(MUTEX_CONDITION_DEBUG,
           "Condition %p wait Enter mutex %p mTid/tid %d/%d", this, &mutex,
           mutex.mTid, gettid());
  ALOG_ASSERT(mbInit);
  ALOG_ASSERT(mutex.mTid == gettid());
  mutex.mTid = 0;
  mutex.incWaiter();
  mWaiters++;
  ALOGD_IF(MUTEX_CONDITION_DEBUG,
           "Condition %p wait on mutex %p waiters %u/%u mTid/tid %d/%d", this,
           &mutex, mWaiters, mutex.getWaiters(), mutex.mTid, gettid());
  mutex.lock();
  int ret = hwcevent.Wait();
  mutex.decWaiter();
  mWaiters--;
  ALOGD_IF(MUTEX_CONDITION_DEBUG,
           "Condition %p re-acquired mutex %p waiters %u/%u mTid/tid %d/%d",
           this, &mutex, mWaiters, mutex.getWaiters(), mutex.mTid, gettid());
  mutex.mTid = gettid();
  clock_gettime(CLOCK_REALTIME, &mutex.mAcqTime);
  return ret;
}
void Condition::signal() {
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Condition %p signalled [waiters:%u]", this,
           mWaiters);
  ALOG_ASSERT(mbInit);
  hwcevent.Signal();
}
void Condition::broadcast() {
  ALOGD_IF(MUTEX_CONDITION_DEBUG, "Condition %p broadcast [waiters:%u]", this,
           mWaiters);
  ALOG_ASSERT(mbInit);
}

};  // namespace Hwcval
