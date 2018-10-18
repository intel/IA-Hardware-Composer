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

#ifndef __HwcvalWork_h__
#define __HwcvalWork_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include "HwcTestDefs.h"
#include "EventQueue.h"
#include "utils/StrongPointer.h"

class HwcTestState;
class DrmShimBuffer;
class HwcTestKernel;

namespace Hwcval {
namespace Work {

class Item {
 public:
  int mFd;

 public:
  Item(int fd);
  virtual ~Item();
  virtual void Process() = 0;
};

class GemOpenItem : public Item {
 public:
  int mId;
  uint32_t mBoHandle;

 public:
  GemOpenItem(int fd, int id, uint32_t boHandle);
  virtual ~GemOpenItem();
  virtual void Process();
};

class GemCloseItem : public Item {
 public:
  uint32_t mBoHandle;

 public:
  GemCloseItem(int fd, uint32_t boHandle);
  virtual ~GemCloseItem();
  virtual void Process();
};

class GemCreateItem : public Item {
 public:
  uint32_t mBoHandle;

 public:
  GemCreateItem(int fd, uint32_t boHandle);
  virtual ~GemCreateItem();
  virtual void Process();
};

class GemWaitItem : public Item {
 public:
  uint32_t mBoHandle;
  int32_t mStatus;
  int64_t mDelayNs;

 public:
  GemWaitItem(int fd, uint32_t boHandle, int32_t status, int64_t delayNs);
  virtual ~GemWaitItem();
  virtual void Process();
};

class PrimeItem : public Item {
 public:
  uint32_t mBoHandle;
  int mDmaHandle;

 public:
  PrimeItem(int fd, uint32_t boHandle, int dmaHandle);
  virtual ~PrimeItem();
  virtual void Process();
};

class BufferFreeItem : public Item {
 public:
  HWCNativeHandle mHandle;

 public:
  BufferFreeItem(HWCNativeHandle handle);
  virtual ~BufferFreeItem();
  virtual void Process();
};

class Queue : public EventQueue<std::shared_ptr<Item>, HWCVAL_MAX_GEM_EVENTS> {
 public:
  Queue();
  virtual ~Queue();

  void Process();
};

}  // namespace Work
}  // namespace Hwcval

#endif  // __HwcvalWork_h__
