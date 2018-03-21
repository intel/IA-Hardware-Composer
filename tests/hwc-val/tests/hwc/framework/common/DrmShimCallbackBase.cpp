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

#include "DrmShimCallbackBase.h"
#include "HwcTestUtil.h"

DrmShimCallbackBase::~DrmShimCallbackBase() {
}

uint32_t DrmShimCallbackBase::GetVersion() {
  return HWCVAL_DRMSHIMCALLBACKBASE_VERSION;
}

// Callbacks that can be overriden in subclass
void DrmShimCallbackBase::VSync(uint32_t disp) {
  HWCVAL_UNUSED(disp);
}

void DrmShimCallbackBase::PageFlipComplete(uint32_t disp) {
  HWCVAL_UNUSED(disp);
}
