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

#ifndef __EventQueue_h__
#define __EventQueue_h__

#include <utils/Condition.h>
#include <string>
#include "HwcTestState.h"

//*****************************************************************************
//
// EventQueue class - responsible for capturing and forwarding
// page flip and VBlank events
//
//*****************************************************************************

template <class C, int SIZE>
class EventQueue {
 public:
  EventQueue(const char* name = "Unknown");
  virtual ~EventQueue();

  // Push an entry on to the queue, overwriting one if it is full
  // If an entry was overwritten, return true and pass back the deleted entry
  bool Push(const C& entry, C* deletedEntry = 0);

  // Pop next entry from the queue
  bool Pop(C& entry);

  // Read the entry at the front of the queue, without popping it.
  // Return null if queue is empty.
  C* Front();

  // Likewise for the back of the queue
  C* Back();

  // Stop anything further being added to the queue
  void Closedown();

  // Access the queue name
  const char* Name();
  void SetName(const std::string& name);

  // Set the error to be generated when items are dropped because the queue is
  // full
  void SetQueueFullError(HwcTestCheckType check);

  // Empty the queue
  void Flush();

  uint32_t Size();
  uint32_t MaxSize();

  bool IsFull();

 protected:
  Hwcval::Mutex mEvMutex;

  // Event communication between our thread and caller's thread
  // Unlikely to have more than one in the queue, but it could happen
  uint32_t mNextFreeEvent;
  uint32_t mNextEventToRaise;
  char mEventQueue[SIZE * sizeof(C)];

  std::string mName;
  HwcTestCheckType mQueueFullError;
  bool mClosingDown;
};

template <class C, int SIZE>
EventQueue<C, SIZE>::EventQueue(const char* name)
    : mNextFreeEvent(0),
      mNextEventToRaise(0),
      mName(name),
      mQueueFullError(eCheckInternalError),
      mClosingDown(false) {
}

template <class C, int SIZE>
EventQueue<C, SIZE>::~EventQueue() {
}

template <class C, int SIZE>
C* EventQueue<C, SIZE>::Front() {
  Hwcval::Mutex::Autolock _l(mEvMutex);
  if (mNextEventToRaise == mNextFreeEvent) {
    HWCLOGD_COND(eLogEventQueue, "%s: Front: empty", Name());
    return 0;
  } else {
    HWCLOGD_COND(eLogEventQueue, "%s: Front: %d", Name(), mNextEventToRaise);
    return reinterpret_cast<C*>(mEventQueue) + mNextEventToRaise;
  }
}

template <class C, int SIZE>
C* EventQueue<C, SIZE>::Back() {
  Hwcval::Mutex::Autolock _l(mEvMutex);
  if (mNextEventToRaise == mNextFreeEvent) {
    HWCLOGD_COND(eLogEventQueue, "%s: Back: empty", Name());
    return 0;
  } else {
    uint32_t lastEvent;
    if (mNextFreeEvent == 0) {
      lastEvent = SIZE - 1;
    } else {
      lastEvent = mNextFreeEvent - 1;
    }
    HWCLOGD_COND(eLogEventQueue, "%s: Back: %d", Name(), lastEvent);
    return reinterpret_cast<C*>(mEventQueue) + lastEvent;
  }
}

template <class C, int SIZE>
bool EventQueue<C, SIZE>::Pop(C& entry) {
  uint32_t nextEventToRaise;
  uint32_t nextFreeEvent;

  {
    Hwcval::Mutex::Autolock _l(mEvMutex);
    nextEventToRaise = mNextEventToRaise;
    nextFreeEvent = mNextFreeEvent;

    if (nextEventToRaise == nextFreeEvent) {
      // Nothing to raise
      // HWCLOGD_COND(eLogEventQueue, "%s: nothing to pop", Name());
      return false;
    }
    mNextEventToRaise = (mNextEventToRaise + 1) % SIZE;

    HWCLOGD_COND(eLogEventQueue, "%s: pop @%d", Name(), nextEventToRaise);
    char* ptr = mEventQueue + sizeof(C) * nextEventToRaise;
    entry = *(reinterpret_cast<C*>(ptr));

    // Run destructor on event queue object, in case it has any smart pointers
    reinterpret_cast<C*>(ptr)->~C();
  }

  return true;
}

template <class C, int SIZE>
bool EventQueue<C, SIZE>::Push(const C& entry, C* deletedEntry) {
  uint32_t eventIx;
  bool entryWasDeleted = false;

  if (!mClosingDown) {
    {
      Hwcval::Mutex::Autolock _l(mEvMutex);
      eventIx = mNextFreeEvent;

      mNextFreeEvent = (mNextFreeEvent + 1) % SIZE;

      if (mNextEventToRaise == mNextFreeEvent) {
        HWCERROR(mQueueFullError,
                 "EventQueue %s has too many events - flushing one @%d", Name(),
                 mNextEventToRaise);

        // return the flushed one if required
        if (deletedEntry) {
          *deletedEntry = *(((C*)mEventQueue) + mNextEventToRaise);
        }

        entryWasDeleted = true;

        // destroy the flushed one
        (((C*)mEventQueue) + mNextEventToRaise)->~C();
        mNextEventToRaise = (mNextEventToRaise + 1) % SIZE;
      }

      // Copy construct the new entry
      new ((void*)(mEventQueue + (eventIx * sizeof(C)))) C{entry};

      HWCLOGD_COND(eLogEventQueue, "%s: push @ %d", Name(), eventIx);
    }
  }

  return entryWasDeleted;
}

template <class C, int SIZE>
void EventQueue<C, SIZE>::SetQueueFullError(HwcTestCheckType queueFullError) {
  mQueueFullError = queueFullError;
}

template <class C, int SIZE>
void EventQueue<C, SIZE>::Flush() {
  C entry;
  while (Size()) {
    Pop(entry);
  }
}

template <class C, int SIZE>
uint32_t EventQueue<C, SIZE>::Size() {
  return (mNextFreeEvent + SIZE - mNextEventToRaise) % SIZE;
}

template <class C, int SIZE>
uint32_t EventQueue<C, SIZE>::MaxSize() {
  return SIZE;
}

template <class C, int SIZE>
bool EventQueue<C, SIZE>::IsFull() {
  return (Size() == MaxSize());
}

template <class C, int SIZE>
void EventQueue<C, SIZE>::Closedown() {
  mClosingDown = true;
}

template <class C, int SIZE>
const char* EventQueue<C, SIZE>::Name() {
  return mName.c_str();
}

template <class C, int SIZE>
void EventQueue<C, SIZE>::SetName(const std::string& name) {
  mName = name;
}

#endif  // __EventQueue_h__
