/*
 * Copyright (C) 2015-2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "worker.h"

#include <sys/prctl.h>
#include <sys/resource.h>

namespace android {

Worker::Worker(const char *name, int priority)
    : name_(name), priority_(priority), exit_(false), initialized_(false) {
}

Worker::~Worker() {
  Exit();
}

int Worker::InitWorker() {
  if (initialized())
    return -EALREADY;

  thread_ = std::unique_ptr<std::thread>(
      new std::thread(&Worker::InternalRoutine, this));
  initialized_ = true;

  return 0;
}

void Worker::Exit() {
  if (initialized()) {
    Lock();
    exit_ = true;
    Unlock();
    cond_.notify_all();
    thread_->join();
    initialized_ = false;
  }
}

int Worker::WaitForSignalOrExitLocked(int64_t max_nanoseconds) {
  int ret = 0;
  if (should_exit())
    return -EINTR;

  std::unique_lock<std::mutex> lk(mutex_, std::adopt_lock);
  if (max_nanoseconds < 0) {
    cond_.wait(lk);
  } else if (std::cv_status::timeout ==
             cond_.wait_for(lk, std::chrono::nanoseconds(max_nanoseconds))) {
    ret = -ETIMEDOUT;
  }

  // exit takes precedence on timeout
  if (should_exit())
    ret = -EINTR;

  // release leaves lock unlocked when returning
  lk.release();

  return ret;
}

void Worker::InternalRoutine() {
  setpriority(PRIO_PROCESS, 0, priority_);
  prctl(PR_SET_NAME, name_.c_str());

  std::unique_lock<std::mutex> lk(mutex_, std::defer_lock);

  while (true) {
    lk.lock();
    if (should_exit())
      return;
    lk.unlock();

    Routine();
  }
}
}
