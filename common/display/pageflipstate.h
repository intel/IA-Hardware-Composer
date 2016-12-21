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

#ifndef PAGE_FLIP_STATE_H_
#define PAGE_FLIP_STATE_H_

#include <stdint.h>

namespace hwcomposer {

class NativeSync;
class PageFlipEventHandler;

class PageFlipState {
 public:
  PageFlipState(NativeSync* sync_object, PageFlipEventHandler* flip_handler,
                uint32_t pipe);
  ~PageFlipState();

  PageFlipEventHandler* GetFlipHandler() const {
    return flip_handler_;
  }

  NativeSync* GetSyncObject() const {
    return sync_object_;
  }

 private:
  NativeSync* sync_object_;
  PageFlipEventHandler* flip_handler_;
  int time_line_fd_;
  uint32_t pipe_;
};

}  // namespace
#endif  // PAGE_FLIP_STATE_H_
