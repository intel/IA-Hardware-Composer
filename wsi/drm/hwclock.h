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

#ifndef COMMON_CORE_HWCLOCK_H_
#define COMMON_CORE_HWCLOCK_H_

#include <stdint.h>

#include "hwcthread.h"

namespace hwcomposer {

class DrmDisplayManager;

// An utility class to block HWC updates if any other
// application what to control display during system
// boot up.
class HWCLock : public HWCThread {
 public:
  HWCLock();
  ~HWCLock() override;

  // The function will return true if DisplayQueue
  // needs to ignore updates till ForceRefresh is
  // called.
  bool RegisterCallBack(DrmDisplayManager* display_manager);

  void DisableWatch();

 protected:
  void HandleRoutine() override;
  void HandleWait() override;

 private:
  DrmDisplayManager* display_manager_ = NULL;
  int lock_fd_ = -1;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_HWCLOCK_H_
