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

#ifndef HWC_THREAD_H_
#define HWC_THREAD_H_

#include <pthread.h>
#include <stdint.h>
#include <string>

namespace hwcomposer {

class HWCThread {
 protected:
  HWCThread(int priority);
  virtual ~HWCThread();

  bool InitWorker(const char *name);

  virtual void Routine() = 0;

  bool initialized_;

 private:
  static void *InternalRoutine(void *HWCThread);

  int priority_;

  pthread_t thread_;
  pthread_cond_t cond_;
};

}  // namespace hwcomposer
#endif  // HWC_THREAD_H_
