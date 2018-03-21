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

#include "DrmShimBuffer.h"
#include "BufferObject.h"
#include "HwcTestState.h"

// HwcTestBufferObject

#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
static uint32_t bufferObjectCount = 0;
#endif

static void Count() {
#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
  if (++bufferObjectCount > 500) {
    HWCLOGW("%d buffer objects created.", bufferObjectCount);
  }
#endif
}

HwcTestBufferObject::HwcTestBufferObject(int fd, uint32_t boHandle)
    : mFd(fd), mBoHandle(boHandle) {
  Count();
  HWCLOGD_COND(eLogBuffer,
               "HwcTestBufferObject::HwcTestBufferObject() Created bo@%p",
               this);
}

HwcTestBufferObject::HwcTestBufferObject(const HwcTestBufferObject& rhs)
    : mBuf(rhs.mBuf),
      mFd(rhs.mFd),
      mBoHandle(rhs.mBoHandle) {
  Count();
  HWCLOGD_COND(eLogBuffer,
               "HwcTestBufferObject::HwcTestBufferObject(&rhs) Created bo@%p",
               this);
}

HwcTestBufferObject::~HwcTestBufferObject() {
#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
  --bufferObjectCount;
#endif
  HWCLOGD_COND(eLogBuffer,
               "HwcTestBufferObject::~HwcTestBufferObject Deleted bo@%p", this);
}

char* HwcTestBufferObject::IdStr(char* str, uint32_t len) {
  FullIdStr(str, len);
  return str;
}

int HwcTestBufferObject::FullIdStr(char* str, uint32_t len) {
  uint32_t n =
      snprintf(str, len, "bo@%p fd %d boHandle 0x%x", this, mFd, mBoHandle);

  return n;
}

HwcTestBufferObject* HwcTestBufferObject::Dup() {
  return new HwcTestBufferObject(*this);
}
