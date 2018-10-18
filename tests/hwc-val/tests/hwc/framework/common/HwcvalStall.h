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

#ifndef __HwcvalStall_h__
#define __HwcvalStall_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include "HwcTestDefs.h"
#include <string>
#include "HwcvalDebug.h"

namespace Hwcval {
enum StallType {
  eStallSetDisplay = 0,
  eStallDPMS,
  eStallSetMode,
  eStallSuspend,
  eStallResume,
  eStallHotPlug,
  eStallHotUnplug,
  eStallGemWait,
  eStallMax
};

class Stall {
 public:
  Stall();
  Stall(const char* configStr, const char* name);
  Stall(uint32_t us, double pct);
  Stall(const Stall& rhs);

  void Do(Hwcval::Mutex* mtx = 0);

 private:
  std::string mName;
  uint32_t mUs;
  double mPct;
  int mRandThreshold;
};
}  // namespace Hwcval

#endif  // __HwcvalStall_h__
