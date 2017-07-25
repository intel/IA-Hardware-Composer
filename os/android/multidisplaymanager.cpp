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

#include "multidisplaymanager.h"

namespace hwcomposer {

MultiDisplayManager::~MultiDisplayManager() {
}

void MultiDisplayManager::SetPrimaryDisplay(NativeDisplay* primary_display) {
  primary_display_ = primary_display;
}

void MultiDisplayManager::UpdatedDisplay(NativeDisplay* display, bool primary) {
  lock_.lock();
  if (primary && state_.empty()) {
    lock_.unlock();
    return;
  }

  size_t size = state_.size();
  if (primary) {
    std::vector<ExtendedDisplayState> new_state;
    for (size_t i = 0; i < size; i++) {
      ExtendedDisplayState& state = state_.at(i);
      if (state.last_frame_updated_) {
        new_state.emplace_back();
        ExtendedDisplayState& temp = new_state.back();
        temp.display_ = state.display_;
        temp.last_frame_updated_ = false;
      } else {
        display->CloneDisplay(primary_display_);
      }
    }

    new_state.swap(state_);
  } else {
    bool found = false;
    for (size_t i = 0; i < size; i++) {
      ExtendedDisplayState& state = state_.at(i);
      if (state.display_ == display) {
        found = true;
      }
    }

    if (!found) {
      state_.emplace_back();
      ExtendedDisplayState& temp = state_.back();
      temp.display_ = display;
      temp.last_frame_updated_ = true;
      display->CloneDisplay(NULL);
    }
  }

  lock_.unlock();
}

}  // namespace hwcomposer
