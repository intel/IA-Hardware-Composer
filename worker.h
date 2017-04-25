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

#ifndef ANDROID_WORKER_H_
#define ANDROID_WORKER_H_

#include <stdint.h>
#include <stdlib.h>
#include <string>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace android {

class Worker {
 public:
  void Lock() {
    mutex_.lock();
  }
  void Unlock() {
    mutex_.unlock();
  }

  void Signal() {
    cond_.notify_all();
  }
  void Exit();

  bool initialized() const {
    return initialized_;
  }

 protected:
  Worker(const char *name, int priority);
  virtual ~Worker();

  int InitWorker();
  virtual void Routine() = 0;

  /*
   * Must be called with the lock acquired. max_nanoseconds may be negative to
   * indicate infinite timeout, otherwise it indicates the maximum time span to
   * wait for a signal before returning.
   * Returns -EINTR if interrupted by exit request, or -ETIMEDOUT if timed out
   */
  int WaitForSignalOrExitLocked(int64_t max_nanoseconds = -1);

  bool should_exit() const {
    return exit_;
  }

  std::mutex mutex_;
  std::condition_variable cond_;

 private:
  void InternalRoutine();

  std::string name_;
  int priority_;

  std::unique_ptr<std::thread> thread_;
  bool exit_;
  bool initialized_;
};
}
#endif
