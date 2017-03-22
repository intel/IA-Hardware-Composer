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

#ifndef COMMON_CORE_NATIVESYNC_H_
#define COMMON_CORE_NATIVESYNC_H_

#include <stdint.h>

#include <scopedfd.h>

namespace hwcomposer {

class NativeSync {
 public:
  // kReady:  Fence can be signalled immediately. This is usually
  //          the case when we need to fallback to 3D Composition for
  //          a given buffer.
  // kIgnore: Dont signal the fence yet. This is the case when layer
  //          associated with the fence is recycled.
  // kSignalOnPageFlipEvent: Release the fence when we recieve PageFlipEvent
  //                         notification.
  enum class State : int32_t { kReady, kIgnore, kSignalOnPageFlipEvent };

  NativeSync();
  virtual ~NativeSync();

  bool Init();

  int CreateNextTimelineFence();

  void SetState(State fence_state);
  State GetState() const {
    return state_;
  }

 private:
  int IncreaseTimelineToPoint(int point);
#ifndef USE_ANDROID_SYNC
  int sw_sync_fence_create(int fd, const char *name, unsigned value);
  int sw_sync_timeline_inc(int fd, unsigned count);
#endif

  ScopedFd timeline_fd_;
  int64_t timeline_ = 0;
  int64_t timeline_current_ = 0;
  State state_ = State::kSignalOnPageFlipEvent;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_NATIVESYNC_H_
