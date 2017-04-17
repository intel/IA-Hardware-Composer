/*
// Copyright (c) 2017 Intel Corporation
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

#include "kmsfencehandler.h"

#include "hwcutils.h"
#include "hwctrace.h"

namespace hwcomposer {

KMSFenceEventHandler::KMSFenceEventHandler(OverlayBufferManager* buffer_manager)
    : HWCThread(-8, "KMSFenceEventHandler"),
      kms_fence_(0),
      kms_ready_fence_(0),
      buffer_manager_(buffer_manager) {
}

KMSFenceEventHandler::~KMSFenceEventHandler() {
}

bool KMSFenceEventHandler::Initialize() {
  if (!InitWorker()) {
    ETRACE("Failed to initalize thread for KMSFenceEventHandler. %s",
           PRINTERROR());
    return false;
  }

  return true;
}

bool KMSFenceEventHandler::EnsureReadyForNextFrame() {
  // Lets ensure the job associated with previous frame
  // has been done, else commit will fail with -EBUSY.
  if (kms_ready_fence_ > 0) {
    HWCPoll(kms_ready_fence_, -1);
    close(kms_ready_fence_);
    kms_ready_fence_ = 0;
  }

  return true;
}

void KMSFenceEventHandler::WaitFence(uint64_t kms_fence,
                                     std::vector<OverlayLayer>& layers) {
  spin_lock_.lock();
  for (OverlayLayer& layer : layers) {
    OverlayBuffer* const buffer = layer.GetBuffer();
    buffers_.emplace_back(buffer);
    // Instead of registering again, we mark the buffer
    // released in layer so that it's not deleted till we
    // explicitly unregister the buffer.
    layer.ReleaseBuffer();
  }

  kms_fence_ = kms_fence;
  kms_ready_fence_ = dup(kms_fence);
  spin_lock_.unlock();
  Resume();
}

void KMSFenceEventHandler::ExitThread() {
  HWCThread::Exit();
}

void KMSFenceEventHandler::HandleRoutine() {
  spin_lock_.lock();
  if (kms_fence_ > 0) {
    HWCPoll(kms_fence_, -1);
    close(kms_fence_);
    kms_fence_ = 0;
  }

  buffer_manager_->UnRegisterBuffers(buffers_);
  std::vector<const OverlayBuffer*>().swap(buffers_);
  spin_lock_.unlock();
}

}  // namespace hwcomposer
