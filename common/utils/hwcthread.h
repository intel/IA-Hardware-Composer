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

#include "fdhandler.h"
#include "hwcevent.h"
#include "spinlock.h"

namespace hwcomposer {

class HWCThread {
 protected:
  HWCThread(int priority, const char *name);
  virtual ~HWCThread();

  bool InitWorker();

  void Resume();
  void Exit();

  virtual void HandleRoutine() = 0;
  virtual void HandleExit();
  virtual void HandleWait();

  FDHandler fd_handler_;
  bool initialized_;

 private:
  void ProcessThread();

  int priority_;
  std::string name_;
  HWCEvent event_;
  bool exit_ = false;
  bool suspended_ = false;

  std::unique_ptr<std::thread> thread_;
};

}  // namespace hwcomposer
#endif  // COMMON_UTILS_HWCTHREAD_H_
