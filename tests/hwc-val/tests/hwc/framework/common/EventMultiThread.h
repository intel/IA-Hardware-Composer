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

#ifndef __EventMultiThread_h__
#define __EventMultiThread_h__

#include "EventThread.h"
#include "hwcthread.h"
#include "HwcTestUtil.h"

// Forward declaration
template <class C, int SIZE, int THREADS>
class EventMultiThread;

// Helper class for reading events from the event queue.
template <class C, int SIZE, int THREADS>
class EventReaderThread : public hwcomposer::HWCThread {
 public:
  EventReaderThread();
  virtual ~EventReaderThread();
  void Start(const char* name, EventMultiThread<C, SIZE, THREADS>* controller);

  virtual void onFirstRef();
  void HandleRoutine();

 private:
  EventMultiThread<C, SIZE, THREADS>* mController;
  std::string mName;
};

//*****************************************************************************
//
// EventMultiThread class - contains a queue on to which events can be pushed
// Events are then dispatched to one of several event threads using the Do
// function.
//
//*****************************************************************************

template <class C, int SIZE, int THREADS>
class EventMultiThread : public EventThread<C, SIZE> {
 public:
  static const int mReaderThreads = THREADS - 1;
  EventMultiThread(const char* name = "Unknown");
  virtual ~EventMultiThread();

  // Caller must override to provide the function that is called when there is
  // work to do.
  virtual void Do(C& entry) = 0;

  // Abort
  void Stop();

  // Main thread function, though it doesn't have to do anything here
  void HandleRoutine();

 private:
  std::string mName;
  typedef EventReaderThread<C, SIZE, THREADS> EvReader;
  EvReader mReaders[mReaderThreads];
};

// EventReaderThread implementation
template <class C, int SIZE, int THREADS>
//Priority needs to be corrected and should be equvivalent to 
// android::PRIORITY_URGENT_DISPLAY + android::PRIORITY_MORE_FAVORABLE
EventReaderThread<C, SIZE, THREADS>::EventReaderThread() : hwcomposer::HWCThread(2,"name"){
}

template <class C, int SIZE, int THREADS>
EventReaderThread<C, SIZE, THREADS>::~EventReaderThread() {
}

template <class C, int SIZE, int THREADS>
void EventReaderThread<C, SIZE, THREADS>::Start(
    const char* name, EventMultiThread<C, SIZE, THREADS>* controller) {
  // Configure
  mName = name;
  mController = controller;

}

template <class C, int SIZE, int THREADS>
void EventReaderThread<C, SIZE, THREADS>::onFirstRef() {
}

template <class C, int SIZE, int THREADS>
void EventReaderThread<C, SIZE, THREADS>::HandleRoutine() {
  C event;
  if(mController->ReadWait(event)) {
    mController->Do(event);
  }

}

// EventMultiThread implementation
template <class C, int SIZE, int THREADS>
EventMultiThread<C, SIZE, THREADS>::EventMultiThread(const char* name)
    : EventThread<C, SIZE>::EventThread(name), mName(name) {
  for (uint32_t i = 0; i < mReaderThreads; ++i) {
    std::string threadName = mName + std::to_string(i);

    mReaders[i].Start(threadName.c_str(), this);
  }
}

template <class C, int SIZE, int THREADS>
EventMultiThread<C, SIZE, THREADS>::~EventMultiThread() {
}

template <class C, int SIZE, int THREADS>
void EventMultiThread<C, SIZE, THREADS>::HandleRoutine() {
  C event;
  if (this->ReadWait(event)) {
    Do(event);
  }

}

template <class C, int SIZE, int THREADS>
void EventMultiThread<C, SIZE, THREADS>::Stop() {
  HWCLOGD("EventMultiThread %s::Stop()", mName.c_str());
  for (uint32_t i = 0; i < mReaderThreads; ++i) {
    mReaders[i].Stop();
  }

  EventThread<C, SIZE>::Stop();
}

#endif  // __EventMultiThread_h__
