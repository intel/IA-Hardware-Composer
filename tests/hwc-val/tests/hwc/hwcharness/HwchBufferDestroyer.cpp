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

#include "HwchBufferDestroyer.h"
#include "HwchSystem.h"
#include "HwcTestState.h"

Hwch::BufferDestroyer::BufferDestroyer() : EventThread("BufferDestroyer") {
  HWCLOGD("Starting BufferDestroyer thread");
  EnsureRunning();
}

Hwch::BufferDestroyer::~BufferDestroyer() {
}

bool Hwch::BufferDestroyer::threadLoop() {
  // Just pull each buffer from the event queue and allow it to be destroyed.

  while (true) {
    buffer_handle_t handle = 0;
    Hwch::System& system = Hwch::System::getInstance();
    HWCLOGD("Size=%d", Size());

    HWCLOGD(
        "Waiting for onSet and 10 buffers in queue before destroying "
        "buffers...");
    while (Size() < 10) {
      HwcTestState::getInstance()->WaitOnSetCondition();
    }

    HWCLOGD("Start destroying buffers, now %d in queue", Size());

    while (Size() > 0) {
       HWCNativeHandle bufHandle;
      if (ReadWait(bufHandle)) {
       system.bufferHandler->ReleaseBuffer(bufHandle);
        HWCLOGD("Destroying buffer handle %p", handle);
      }
    }
  }

  return true;
}
