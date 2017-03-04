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
  mutex_.lock();
  initialized_ = true;
  exit_ = false;
  mutex_.unlock();
  thread_ = std::unique_ptr<std::thread>(
      new std::thread(&HWCThread::ProcessThread, this));

  return true;
}

void HWCThread::Resume() {
  if (!suspended_ || exit_)
    return;

  mutex_.lock();
  suspended_ = false;
  mutex_.unlock();

  cond_.notify_one();
}

void HWCThread::Exit() {
  IHOTPLUGEVENTTRACE("HWCThread::Exit recieved.");
  if (!initialized_)
    return;

  mutex_.lock();
  initialized_ = false;
  suspended_ = false;
  exit_ = true;
  mutex_.unlock();

  cond_.notify_one();
  thread_->join();
}

void HWCThread::HandleExit() {
}

void HWCThread::ProcessThread() {
  setpriority(PRIO_PROCESS, 0, priority_);
  prctl(PR_SET_NAME, name_.c_str());

  std::unique_lock<std::mutex> lk(mutex_, std::defer_lock);
  while (true) {
    lk.lock();
    if (exit_) {
      HandleExit();
      return;
    }

    if (suspended_) {
      cond_.wait(lk);
    }
    lk.unlock();
    HandleRoutine();
  }
}

void HWCThread::ConditionalSuspend() {
  mutex_.lock();
  suspended_ = true;
  mutex_.unlock();
}

}  // namespace hwcomposer
