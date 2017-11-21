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

#include "hwcevent.h"

#include <sys/eventfd.h>

#include "hwctrace.h"

#ifndef EFD_SEMAPHORE
#define EFD_SEMAPHORE           (1 << 0)
#endif

namespace hwcomposer {

HWCEvent::HWCEvent() : fd_(0) {
}

HWCEvent::~HWCEvent() {
  if (fd_ >= 0)
    close(fd_);

  fd_ = -1;
}

bool HWCEvent::Initialize() {
  if (fd_ > 0) {
    return true;
  }

  fd_ = eventfd(0, EFD_SEMAPHORE);
  if (fd_ < 0) {
    ETRACE("Failed to initialize eventfd: %s", PRINTERROR());
    return false;
  }

  return true;
}

bool HWCEvent::Signal() {
  uint64_t inc = 1;
  ssize_t ret = write(fd_, &inc, sizeof(inc));
  if (ret < 0) {
    ETRACE("couldn't write to eventfd: %zd (%s)", ret, PRINTERROR());
    return false;
  }

  return true;
}

bool HWCEvent::Wait() {
  if (fd_ < 0) {
    ETRACE("invalid eventfd: %d", fd_);
    return false;
  }

  uint64_t result;
  ssize_t ret = read(fd_, &result, sizeof(result));
  if (ret < 0) {
    ETRACE("couldn't read from eventfd: %zd (%s)", ret, PRINTERROR());
    return false;
  }

  if (result != 1) {
    ETRACE("read from eventfd has wrong value: %" PRIu64 " (should be 1)", result);
    return false;
  }

  return true;
}

}  // namespace hwcomposer
