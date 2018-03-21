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

#ifndef __HwcTestDebug_h__
#define __HwcTestDebug_h__

#include "HwcTestDefs.h"
#include "HwcTestState.h"
#include "DrmShimBuffer.h"

enum { DUMP_BUFFER_TO_RAW = (1 << 0), DUMP_BUFFER_TO_TGA = (1 << 1) };

bool HwcTestDumpBufferToDisk(const char* pchFilename, uint32_t num,
                             HWCNativeHandle grallocHandle,
                             uint32_t outputDumpMask);

bool HwcTestDumpAuxBufferToDisk(const char* pchFilename, uint32_t num,
                                HWCNativeHandle grallocHandle);

bool HwcTestDumpMemBufferToDisk(const char* pchFilename, uint32_t num,
                                const void* handle,
                                uint32_t outputDumpMask,
                                uint8_t* pData);

#if HWCVAL_LOCK_DEBUG || defined(HWCVAL_LOCK_TRACE)
#include <utils/Mutex.h>
class HwcvalLock {
 public:
  HwcvalLock(const char* funcName, const char* mutexName, Hwcval::Mutex& mutex)
      : mLock(mutex)
#ifdef HWCVAL_LOCK_TRACE
        ,
        mTracer(ATRACE_TAG, funcName)
#endif
  {
    HWCLOGD_IF(HWCVAL_LOCK_DEBUG, "Thread %d Request lock mutex %s @ %p : %s",
               gettid(), mutexName, &mutex, funcName);
    mLock.lock();
    HWCLOGD_IF(HWCVAL_LOCK_DEBUG, "Thread %d Gained lock mutex %s @ %p : %s",
               gettid(), mutexName, &mutex, funcName);
  }

  HwcvalLock(const char* funcName, const char* mutexName, Hwcval::Mutex* mutex)
      : mLock(*mutex)
#ifdef HWCVAL_LOCK_TRACE
        ,
        mTracer(ATRACE_TAG, funcName)
#endif
  {
    HWCLOGD_IF(HWCVAL_LOCK_DEBUG, "Thread %d Request lock mutex %s @ %p : %s",
               gettid(), mutexName, mutex, funcName);
    mLock.lock();
    HWCLOGD_IF(HWCVAL_LOCK_DEBUG, "Thread %d Gained lock mutex %s @ %p : %s",
               gettid(), mutexName, mutex, funcName);
  }

  ~HwcvalLock() {
    HWCLOGD_IF(HWCVAL_LOCK_DEBUG, "Thread %d Unlocking mutex @ %p", gettid(),
               &mLock);
    mLock.unlock();
  }

 private:
  Hwcval::Mutex& mLock;
#ifdef HWCVAL_LOCK_TRACE
  android::ScopedTrace mTracer;
#endif
};

#define HWCVAL_LOCK(L, M)               \
  char L##name[1024];                   \
  strcpy(L##name, __PRETTY_FUNCTION__); \
  strcat(L##name, "-Mtx");              \
  HwcvalLock L(L##name, #M, M)

#else
#define HWCVAL_LOCK(L, M) Hwcval::Mutex::Autolock L(M)
#endif

#endif  // __HwcTestDebug_h__
