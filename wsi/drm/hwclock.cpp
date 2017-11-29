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

#include "hwclock.h"

#include <sys/file.h>

#include "drmdisplaymanager.h"
#include "hwctrace.h"

namespace hwcomposer {

HWCLock::HWCLock() : HWCThread(-8, "HWCLock") {
}

HWCLock::~HWCLock() {
}

bool HWCLock::RegisterCallBack(DrmDisplayManager* display_manager) {
  display_manager_ = display_manager;
  lock_fd_ = open("/vendor/hwc.lock", O_RDONLY);
  if (lock_fd_ == -1)
    return false;

  if (!InitWorker()) {
    ETRACE("Failed to initalize thread for VblankEventHandler. %s",
           PRINTERROR());
    return false;
  }

  return true;
}

void HWCLock::DisableWatch() {
  Exit();
}

void HWCLock::HandleWait() {
  if (lock_fd_ == -1) {
    HWCThread::HandleWait();
  }
}

void HWCLock::HandleRoutine() {
  if (lock_fd_ == -1)
    return;

  if (flock(lock_fd_, LOCK_EX) != 0)
    ETRACE("Failed to wait on hwc lock.");

  close(lock_fd_);
  lock_fd_ = -1;

  display_manager_->ForceRefresh();
}

}  // namespace hwcomposer
