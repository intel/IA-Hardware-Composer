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

#ifndef __DrmShimCallbackBase_h__
#define __DrmShimCallbackBase_h__

#include "HwcTestState.h"

#define HWCVAL_DRMSHIMCALLBACKBASE_VERSION 2

class DrmShimCallbackBase {
 public:
  virtual ~DrmShimCallbackBase();

  bool CheckVersion();
  uint32_t GetVersion();

  // Callbacks that can be overriden in subclass
  virtual void VSync(uint32_t disp);
  virtual void PageFlipComplete(uint32_t disp);
};

inline bool DrmShimCallbackBase::CheckVersion() {
  bool result = (GetVersion() == HWCVAL_DRMSHIMCALLBACKBASE_VERSION);

  if (!result) {
    HWCERROR(eCheckDrmShimFail, "Incompatible shims");
  }

  return result;
}

#endif  // __DrmShimCallbackBase_h__
