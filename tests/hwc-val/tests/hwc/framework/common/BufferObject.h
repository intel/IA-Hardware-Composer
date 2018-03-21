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

#ifndef __BufferObject_h__
#define __BufferObject_h__

#include <stdint.h>
#include <utils/KeyedVector.h>

#include "HwcTestDefs.h"

class DrmShimPlane;
class DrmShimBuffer;

class HwcTestBufferObject  {
 public:
  std::shared_ptr<DrmShimBuffer> mBuf;
  int mFd;
  uint32_t mBoHandle;

 public:
  HwcTestBufferObject(int fd, uint32_t boHandle);
  HwcTestBufferObject(const HwcTestBufferObject& rhs);
  virtual ~HwcTestBufferObject();

  char* IdStr(char* str, uint32_t len = HWCVAL_DEFAULT_STRLEN - 1);
  int FullIdStr(char* str, uint32_t len = HWCVAL_DEFAULT_STRLEN - 1);
  virtual HwcTestBufferObject* Dup();
};

#endif  // __BufferObject_h__
