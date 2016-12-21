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

#ifndef PAGE_FLIP_EVENT_HANDLER_H_
#define PAGE_FLIP_EVENT_HANDLER_H_

#include <stdint.h>

#include <nativedisplay.h>

#include "spinlock.h"

namespace hwcomposer {

class PageFlipEventHandler {
 public:
  PageFlipEventHandler();
  ~PageFlipEventHandler();

  void Init(float refresh);

  void HandlePageFlipEvent(unsigned int sec, unsigned int usec);

  int RegisterCallback(std::shared_ptr<VsyncCallback> callback,
                       uint32_t display_id);

  int VSyncControl(bool enabled);

 private:
  // shared_ptr since we need to use this outside of the thread lock (to
  // actually call the hook) and we don't want the memory freed until we're
  // done
  std::shared_ptr<VsyncCallback> callback_ = NULL;
  SpinLock spin_lock_;
  uint32_t display_;
  bool enabled_;

  float refresh_;
  int64_t last_timestamp_;
};

}  // namespace
#endif  // PAGE_FLIP_EVENT_HANDLER_H_
