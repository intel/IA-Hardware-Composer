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

#ifndef __HwchInputGenerator_h__
#define __HwchInputGenerator_h__

#include "utils/Thread.h"
#include "HwchDefs.h"
#include "hwcthread.h"
namespace Hwch {
class InputGenerator : public hwcomposer::HWCThread {
 public:
  InputGenerator();
  virtual ~InputGenerator();

  // Open and configure the ui device
  void Open();

  // Start or stop keypress generation
  void SetActive(bool active);

  // Wait until previous active/inactive request is complete
  void Stabilize();

  // Start or stop keypress generation.
  // If stopping, wait for input to time out.
  void SetActiveAndWait(bool active);

 private:
  void HandleRoutine();

  void Keypress();
  int WriteEvent(int type, int code, int value);

  int mFd;
  bool mRunning;
  bool mActive;
  bool mKeypressFailed;

  // Time at which input will have timed out
  int64_t mInactiveTime;

  static const uint32_t mKeypressIntervalUs;
  static const uint32_t mTimeoutPeriodUs;
};
}

#endif  // __HwchInputGenerator_h__
