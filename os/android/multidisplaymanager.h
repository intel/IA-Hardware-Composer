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

#ifndef OS_ANDROID_MULTIDISPLAYMANAGER_H_
#define OS_ANDROID_MULTIDISPLAYMANAGER_H_

#include <stdint.h>
#include <vector>

#include <nativedisplay.h>
#include <spinlock.h>

namespace hwcomposer {

class DisplayQueue;

// An utility class to track if we need to support
// Extended or clone display mode when we have more
// than one monitor connected.
class MultiDisplayManager {
 public:
  MultiDisplayManager() = default;

  MultiDisplayManager(const MultiDisplayManager& rhs) = delete;
  MultiDisplayManager& operator=(const MultiDisplayManager& rhs) = delete;

  ~MultiDisplayManager();

  void SetPrimaryDisplay(NativeDisplay* primary_display);
  void UpdatedDisplay(NativeDisplay* display, bool primary);

 private:
  struct ExtendedDisplayState {
    NativeDisplay* display_;
    bool last_frame_updated_ = false;
  };

  std::vector<ExtendedDisplayState> state_;
  NativeDisplay* primary_display_;
  SpinLock lock_;
};

}  // namespace hwcomposer
#endif  // OS_ANDROID_MULTIDISPLAYMANAGER_H_
