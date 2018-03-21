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

#include "HwcvalLayerListQueue.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"
#include "HwcvalContent.h"
#include "DrmShimBuffer.h"

Hwcval::LLEntry::LLEntry() {
}

Hwcval::LLEntry::LLEntry(const Hwcval::LLEntry& entry)
    : mLL(entry.mLL),
      mUnsignalled(entry.mUnsignalled),
      mUnvalidated(entry.mUnvalidated),
      mHwcFrame(entry.mHwcFrame) {
}

Hwcval::LayerListQueue::LayerListQueue() {
  mState = HwcTestState::getInstance();

  SetQueueFullError(eCheckLLQOverflow);
}

void Hwcval::LayerListQueue::SetId(uint32_t qid) {
  mQid = qid;
  std::string name((std::string("LLQ-D") + std::to_string(qid)).c_str());
  SetName(name);
}

bool Hwcval::LayerListQueue::IsFull() {
  return EventQueue::IsFull();
}

void Hwcval::LLEntry::Clean() {
  if (mUnsignalled) {
    int fence = mLL->GetRetireFence();
    HWCLOGD_COND(eLogLLQ, "frame:%d: Closing retire fence %d", mHwcFrame,
                 fence);
    CloseFence(fence);
    mUnsignalled = false;
  }

  if (mUnvalidated) {
    HWCLOGD_COND(eLogLLQ, "frame:%d: LLQ entry closed without validation",
                 mHwcFrame);
  }
}

void Hwcval::LayerListQueue::Push(LayerList* layerList, uint32_t hwcFrame) {
  ATRACE_CALL();

  Hwcval::LLEntry entry;
  entry.mLL = layerList;
  entry.mUnsignalled = true;
  entry.mUnvalidated = true;
  entry.mHwcFrame = hwcFrame;

  LLEntry deletedEntry;

  if (EventQueue::Push(entry, &deletedEntry)) {
    if (deletedEntry.mUnsignalled) {
      int fence = deletedEntry.mLL->GetRetireFence();

      if (fence > 0) {
        HWCERROR(eCheckRetireFenceSignalledPromptly,
                 "Expired old unsignalled fence %d from display %d frame:%d",
                 fence, mQid, deletedEntry.mHwcFrame);
      } else {
        HWCLOGD("  -- Flushed entry was SF%d frame:%d. No fence.", mQid,
                deletedEntry.mHwcFrame);
      }

    } else {
      HWCLOGD("  -- Flushed entry was SF%d frame:%d", mQid,
              deletedEntry.mHwcFrame);
    }

    deletedEntry.Clean();
    delete deletedEntry.mLL;
  }
}

uint32_t Hwcval::LayerListQueue::GetSize() {
  return EventQueue::Size();
}

void Hwcval::LayerListQueue::LogQueue() {
  if (mState->IsOptionEnabled(eLogLLQContents)) {
    // TODO, if required
  }
}

Hwcval::LayerList* Hwcval::LayerListQueue::GetBack() {
  LLEntry* back = this->Back();

  if (back == 0) {
    return 0;
  } else {
    return back->mLL;
  }
}

uint32_t Hwcval::LayerListQueue::GetBackFN() {
  LLEntry* back = this->Back();

  if (back == 0) {
    return 0;
  } else {
    return back->mHwcFrame;
  }
}

uint32_t Hwcval::LayerListQueue::GetFrontFN() {
  LLEntry* front = this->Front();

  if (front == 0) {
    return 0;
  } else {
    return front->mHwcFrame;
  }
}

bool Hwcval::LayerListQueue::BackNeedsValidating() {
  LLEntry* back = this->Back();

  if (back == 0) {
    return false;
  } else {
    return back->mUnvalidated;
  }
}

// Get specified frame number for specified display
Hwcval::LayerList* Hwcval::LayerListQueue::GetFrame(uint32_t hwcFrame,
                                                    bool expectPrevSignalled) {
  ATRACE_CALL();

  Hwcval::LLEntry* front = Front();

  while (front) {
    if (front->mHwcFrame == hwcFrame) {
      HWCLOGD_COND(eLogLLQ, "LLQ:GetFrame: SF%d frame:%d found", mQid,
                   hwcFrame);
      front->mUnvalidated = false;
      return front->mLL;
    } else if (front->mHwcFrame < hwcFrame) {
      if (front->mUnvalidated) {
        HWCLOGD_COND(eLogLLQ, "LLQ: SF%d frame:%d dropped without notification",
                     mQid, front->mHwcFrame);
      }

      if (front->mUnsignalled) {
        int fence = front->mLL->GetRetireFence();

        // When frame(s) are dropped, we can't expect the previous frame's fence
        // to be signalled
        // We have to remember this on the following regular frame
        bool doSignalCheck = mExpectPrevSignalled && expectPrevSignalled;

        if (mState->IsFenceUnsignalled(fence, mQid, front->mHwcFrame)) {
          if (!doSignalCheck) {
            HWCLOGD(
                "SF%d frame:%d dropped so not expecting frame:%d to be "
                "signalled",
                mQid, hwcFrame, front->mHwcFrame);
          } else {
            HWCERROR(eCheckFlipFences,
                     "SF%d frame:%d requested for validation when frame:%d not "
                     "yet signalled",
                     mQid, hwcFrame, front->mHwcFrame);
          }
        }

        CloseFence(fence);
        mExpectPrevSignalled = expectPrevSignalled;
      }

      LLEntry entry;
      Pop(entry);

      if (entry.mLL) {
        delete entry.mLL;
      }

      front = Front();
    } else {
      break;
    }
  }

  HWCLOGW("SF%d frame:%d not found", mQid, hwcFrame);

  return 0;
}
