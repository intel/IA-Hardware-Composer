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

#ifndef COMMON_DISPLAY_KMSFENCEHANDLER_H_
#define COMMON_DISPLAY_KMSFENCEHANDLER_H_

#include <stdint.h>

#include <spinlock.h>

#include <vector>

#include "nativesync.h"
#include "overlaylayer.h"

#include "hwcthread.h"

namespace hwcomposer {

class KMSFenceEventHandler : public HWCThread {
 public:
  KMSFenceEventHandler(OverlayBufferManager* buffer_manager);
  ~KMSFenceEventHandler() override;

  bool Initialize();

  void WaitFence(uint64_t kms_fence, std::vector<OverlayLayer>& layers);

  bool EnsureReadyForNextFrame();

  void HandleRoutine() override;
  void ExitThread();

 private:
  SpinLock spin_lock_;
  std::vector<const OverlayBuffer*> buffers_;
  uint64_t kms_fence_;
  OverlayBufferManager* buffer_manager_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_PAGEFLIPEVENTHANDLER_H_
