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

#ifndef __HwcDrmShimCallback_h__
#define __HwcDrmShimCallback_h__

#include "DrmShimCallbackBase.h"

class HwcFrameControl;

typedef void(PFN_PFC)(uint32_t disp);

class HwcDrmShimCallback : public DrmShimCallbackBase {
 public:
  HwcDrmShimCallback();
  virtual ~HwcDrmShimCallback();

  // VSync callback
  virtual void VSync(uint32_t disp);
  virtual void PageFlipComplete(uint32_t disp);

  void IncOnSetCounter();
  void SetPageFlipCompleteCallback(PFN_PFC *pfn);

 private:
  uint32_t cHWCOnSets;
  uint32_t cPageFlips;
  PFN_PFC *pfnPageFlipCallback;
};

inline void HwcDrmShimCallback::IncOnSetCounter() {
  ++cHWCOnSets;
}

inline void HwcDrmShimCallback::SetPageFlipCompleteCallback(PFN_PFC *pfn) {
  pfnPageFlipCallback = pfn;
}

#endif  // __HwcDrmShimCallback_h__
