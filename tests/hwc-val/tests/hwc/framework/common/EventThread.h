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

#ifndef __EventThread_h__
#define __EventThread_h__

#include "EventQueue.h"
#include "hwcthread.h"
#include "HwcTestUtil.h"

//*****************************************************************************
//
// EventQueue class - responsible for capturing and forwarding
// page flip and VBlank events
//
//*****************************************************************************

template <class C, int SIZE>
class EventThread : public EventQueue<C, SIZE>, public hwcomposer::HWCThread {
 public:
  EventThread(const char* name = "Unknown");
  virtual ~EventThread();

  // Push an entry on to the queue, overwriting one if it is full
  void Push(const C& entry);

  // Pop next entry from the queue, waiting for one if there is none
  bool ReadWait(C& entry);

  // Ensure the thread is running
  void EnsureRunning();

  // Abort
  void Stop();

  // Overrideable join
  void JoinThread();

  virtual void onFirstRef();

  void HandleRoutine(){}
 protected:
  Hwcval::Condition mCondition;
  Hwcval::Mutex mMutex;

  bool mThreadRunning;

  volatile int32_t mContinueHandleEvent;
};

template <class C, int SIZE>
EventThread<C, SIZE>::EventThread(const char* name)
//Fix priority should be equivalent be android::PRIORITY_URGENT_DISPLAY 
//+ android::PRIORITY_MORE_FAVORABLE
    : hwcomposer::HWCThread(2, name), EventQueue<C, SIZE>(name), mThreadRunning(false) {
}

template <class C, int SIZE>
EventThread<C, SIZE>::~EventThread() {
}

template <class C, int SIZE>
bool EventThread<C, SIZE>::ReadWait(C& entry) {
  HWCLOGV_COND(eLogEventHandler, "EventThread %s::ReadWait entry",
               this->Name());
  mContinueHandleEvent = true;
  uint32_t count = 0;

  while (mContinueHandleEvent && !this->Pop(entry)) {
    // HWCLOGV_COND(eLogEventHandler,"EventThread %s: Nothing to pop",
    // this->Name());

    // For this wait, we could use mEvMutex, but then we need to place a lock on
    // that for the whole of
    // this function and provide a PopNoLock function for us to call.
    Hwcval::Mutex::Autolock lock(mMutex);
    if (mCondition.waitRelative(mMutex, 1000000) && (((++count) % 100) == 0)) {
      HWCLOGV_COND(eLogEventHandler, "EventThread %s: No event within %dms",
                   this->Name(), count);
    }
  }

  HWCLOGV_COND(eLogEventHandler, "EventThread %s::ReadWait exit %s",
               this->Name(), mContinueHandleEvent ? "true" : "false");
  return mContinueHandleEvent;
}

template <class C, int SIZE>
void EventThread<C, SIZE>::Push(const C& entry) {
  EventQueue<C, SIZE>::Push(entry);

  mCondition.signal();
}

template <class C, int SIZE>
void EventThread<C, SIZE>::onFirstRef() {
}

template <class C, int SIZE>
void EventThread<C, SIZE>::EnsureRunning() {
    HWCLOGD_COND(eLogEventHandler, "EventThread %s::EnsureRunning",
                 this->Name());

    mThreadRunning = true;
}

template <class C, int SIZE>
void EventThread<C, SIZE>::Stop() {
  HWCLOGD("EventThread %s::Stop()", this->Name());
  mContinueHandleEvent = false;
  mCondition.signal();
  Exit();
}

template <class C, int SIZE>
void EventThread<C, SIZE>::JoinThread() {
}

#endif  // __EventThread_h__
