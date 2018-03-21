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

#include "HwcvalThreadTable.h"
#include "HwcTestDefs.h"
#include "HwcTestState.h"

// Thread state hash table
static uint32_t mTids[HWCVAL_THREAD_TABLE_SIZE];
static const char* mThreadState[HWCVAL_THREAD_TABLE_SIZE];

void Hwcval::InitThreadStates() {
  // Do not use logging in this function, or we will recurse
  for (uint32_t i = 0; i < HWCVAL_THREAD_TABLE_SIZE; ++i) {
    mTids[i] = 0;
    mThreadState[i] = "";
  }
}

const char* Hwcval::SetThreadState(const char* str) {
  uint32_t tid = gettid();
  uint32_t hash = tid % HWCVAL_THREAD_TABLE_SIZE;

  // Find the thread in the table
  uint32_t i = hash;
  do {
    if (tid == mTids[i]) {
      const char* oldState = mThreadState[i];
      mThreadState[i] = str;
      return oldState;
    }

    if (++i >= HWCVAL_THREAD_TABLE_SIZE) {
      i = 0;
    }
  } while (i != hash);

  // Thread not found, so add it to the table
  i = hash;
  do {
    if (mTids[i] == 0) {
      mTids[i] = tid;
      mThreadState[i] = str;
      return "";
    }

    if (++i >= HWCVAL_THREAD_TABLE_SIZE) {
      i = 0;
    }
  } while (i != hash);

  HWCLOGI("Thread table full.");
  return "";
}

void Hwcval::ReportThreadStates() {
  HWCLOGD("ReportThreadStates");
  for (uint32_t i = 0; i < HWCVAL_THREAD_TABLE_SIZE; ++i) {
    if (mTids[i]) {
      HWCLOGA("Thread %d: %s", mTids[i], mThreadState[i]);
    }
  }
}

Hwcval::PushThreadState::PushThreadState(const char* threadState) {
  mOldThreadState = SetThreadState(threadState);
}

Hwcval::PushThreadState::~PushThreadState() {
  SetThreadState(mOldThreadState);
}
