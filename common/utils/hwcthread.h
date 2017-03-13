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

#ifndef COMMON_UTILS_HWCTHREAD_H_
#define COMMON_UTILS_HWCTHREAD_H_

#include <thread>
#include <string>
#include <memory>

#include "hwcevent.h"
#include "spinlock.h"

namespace hwcomposer {

class HWCThread {
 protected:
  /**
   * Constructor that initializes priority and name
   * of the thread.
   *
   * @param priority is priority of thread.
   * @param name is name of thread.
   */
  HWCThread(int priority, const char *name);
  virtual ~HWCThread();

  /**
   * Initializes Thread.
   * @return true if succesful.
   */
  bool InitWorker();

  /**
   * Wakes up the thread and schedules any
   * pending jobs.
   */
  void Resume();

  /**
   * Exits the thread. After this call InitWorker
   * needs to be called before scheduling any
   * jobs to this thread.
   */
  void Exit();

  /**
   * Called during thread execution. Derived
   * class needs to implement this function
   * and handle any related jobs.
   */
  virtual void HandleRoutine() = 0;

  /**
   * Called before the thread is killed to
   * do any resource cleanup.
   */
  virtual void HandleExit();

  /**
   * Called during thread execution and will
   * put the thread to sleep in case there is
   * no pending work to be handled.
   */
  virtual void HandleWait();

 private:
  void ProcessThread();

  int priority_;
  std::string name_;
  HWCEvent event_;
  bool exit_ = false;
  bool initialized_;

  std::unique_ptr<std::thread> thread_;
};

}  // namespace hwcomposer
#endif  // COMMON_UTILS_HWCTHREAD_H_
