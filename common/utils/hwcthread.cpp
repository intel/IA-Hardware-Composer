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

#include "hwcthread.h"

#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "hwctrace.h"

namespace hwcomposer {

HWCThread::HWCThread(int priority, const char *name)
    : initialized_(false), priority_(priority), name_(name) {
}

HWCThread::~HWCThread() {
  Exit();
}

bool HWCThread::InitWorker() {
  if (initialized_)
    return true;

  initialized_ = true;
  exit_ = false;

  if (!event_.Initialize())
    return false;

  fd_handler_.AddFd(event_.get_fd());
  thread_ = std::unique_ptr<std::thread>(
      new std::thread(&HWCThread::ProcessThread, this));

  return true;
}

void HWCThread::Resume() {
  if (exit_ || !initialized_)
    return;

  event_.Signal();
}

void HWCThread::Exit() {
  if (!initialized_)
    return;

  initialized_ = false;
  exit_ = true;
  IHOTPLUGEVENTTRACE("HWCThread::Exit recieved.");
  event_.Signal();
  thread_->join();
}

void HWCThread::HandleExit() {
}

void HWCThread::HandleWait() {
  if (fd_handler_.Poll(-1) <= 0) {
    ETRACE("Poll Failed in DisplayManager %s", PRINTERROR());
    return;
  }

  if (fd_handler_.IsReady(event_.get_fd())) {
    // If eventfd_ is ready, we need to wait on it (using read()) to clean
    // the flag that says it is ready.
    event_.Wait();
  }
}

void HWCThread::ProcessThread() {
  setpriority(PRIO_PROCESS, 0, priority_);
  prctl(PR_SET_NAME, name_.c_str());

  while (1) {
    HandleWait();
    if (exit_) {
      HandleExit();
      fd_handler_.RemoveFd(event_.get_fd());
      return;
    }

    HandleRoutine();
  }
}

}  // namespace hwcomposer
