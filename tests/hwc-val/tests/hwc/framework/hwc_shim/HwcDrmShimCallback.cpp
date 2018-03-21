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

#include <utils/Trace.h>

#include "HwcDrmShimCallback.h"
#include "HwcTestUtil.h"
#include "HwcTestDefs.h"

// Constructor
HwcDrmShimCallback::HwcDrmShimCallback()
    : cHWCOnSets(0), cPageFlips(0), pfnPageFlipCallback(NULL) {
}

// Destructor
HwcDrmShimCallback::~HwcDrmShimCallback() {
}

// Callbacks that can be overriden in subclass
void HwcDrmShimCallback::VSync(uint32_t disp) {
  HWCVAL_UNUSED(disp);
}

void HwcDrmShimCallback::PageFlipComplete(uint32_t disp) {
  HWCVAL_UNUSED(disp);
  ATRACE_CALL();

  ++cPageFlips;
  HWCLOGV(
      "HwcDrmShimCallback::PageFlipComplete - OnSet/PageFlipComplete = %u/%u",
      cHWCOnSets, cPageFlips);
  if (pfnPageFlipCallback) {
    pfnPageFlipCallback(disp);
  }
}
