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

#ifndef __HwcvalLayerListQueue_h__
#define __HwcvalLayerListQueue_h__

#include "DrmShimBuffer.h"
#include "HwcTestDefs.h"
#include "EventQueue.h"
#include "HwcvalContent.h"

#include <utils/SortedVector.h>

class HwcTestState;

namespace Hwcval {
class LayerList;

struct LLEntry {
  Hwcval::LayerList* mLL;
  bool mUnsignalled;
  bool mUnvalidated;
  uint32_t mHwcFrame;

  void Clean();
  LLEntry();
  LLEntry(const LLEntry& entry);
};

class LayerListQueue
    : protected EventQueue<LLEntry, HWCVAL_LAYERLISTQUEUE_DEPTH> {
 public:
  LayerListQueue();

  // Set queue id (probably display index).
  void SetId(uint32_t id);

  // Will pushing any more result in an eviction?
  bool IsFull();

  void Push(LayerList* layerList, uint32_t hwcFrame);

  // Number of entries remaining in the queue.
  uint32_t GetSize();

  // Log out the contents of the LLQ (if enabled)
  void LogQueue();

  // Is there something to validate at the back of the queue?
  bool BackNeedsValidating();

  // Get entry at back of queue
  LayerList* GetBack();
  uint32_t GetBackFN();

  // Get entry at front of queue
  uint32_t GetFrontFN();

  // Get entry with stated sequence number
  LayerList* GetFrame(uint32_t hwcFrame, bool expectPrevSignalled = true);

 private:
  // Test state
  HwcTestState* mState;

  // Queue id (probably display index)
  uint32_t mQid;

  // Skip the "previous fence is signalled" check
  bool mExpectPrevSignalled = false;
};
}

#endif  // __HwcvalLayerListQueue_h__
