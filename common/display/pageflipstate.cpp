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

#include "pageflipstate.h"

#include <hwctrace.h>
#include <nativesync.h>

namespace hwcomposer {

PageFlipState::PageFlipState(NativeSync* sync_object,
                             PageFlipEventHandler* flip_handler, uint32_t pipe)
    : sync_object_(sync_object), flip_handler_(flip_handler), pipe_(pipe) {
}

PageFlipState::~PageFlipState() {
  DUMPTRACE("PageFlipState releasing sync fd: %d", sync_object_->GetFd());
  delete sync_object_;
}
}
