/*
// Copyright (c) 2016 Intel Corporation
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

#ifndef COMMON_DISPLAY_VBLANK_EVENT_HANDLER_H_
#define COMMON_DISPLAY_VBLANK_EVENT_HANDLER_H_

#include <stdint.h>
#include <xf86drm.h>

#include <nativedisplay.h>
#include <spinlock.h>

#include <memory>

#include "hwcthread.h"

namespace hwcomposer {

class DisplayQueue;

class VblankEventHandler : public HWCThread {
 public:
  VblankEventHandler(DisplayQueue* queue);
  ~VblankEventHandler() override;

  void Init(int fd, int pipe);

  bool SetPowerMode(uint32_t power_mode);

  void HandlePageFlipEvent(unsigned int sec, unsigned int usec);

  int RegisterCallback(std::shared_ptr<VsyncCallback> callback,
                       uint32_t display_id);

  int RegisterCallback(std::shared_ptr<VsyncPeriodCallback> callback,
                       uint32_t display_id);

  int VSyncControl(bool enabled);

 protected:
  void HandleRoutine() override;
  void HandleWait() override;

 private:
  // shared_ptr since we need to use this outside of the thread lock (to
  // actually call the hook) and we don't want the memory freed until we're
  // done
  std::shared_ptr<VsyncCallback> callback_ = NULL;
  std::shared_ptr<VsyncPeriodCallback> callback_2_4_ = NULL;
  SpinLock spin_lock_;
  uint32_t display_;
  int64_t vperiod_;
  bool enabled_ = false;

  int fd_;
  int64_t last_timestamp_;
  int64_t previous_timestamp_;
  drmVBlankSeqType type_;
  DisplayQueue* queue_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_VBLANK_EVENT_HANDLER_H_
