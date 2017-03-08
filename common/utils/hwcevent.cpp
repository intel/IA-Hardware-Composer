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

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>

#include "hwctrace.h"

namespace hwcomposer {

HWCEvent::HWCEvent() {
  fd_ = eventfd(0, EFD_SEMAPHORE);
  if (fd_ < 0) {
    ETRACE("Failed to initialize eventfd: %s", strerror(errno));
  }
}

HWCEvent::~HWCEvent() {
  if (fd_ >= 0)
    close(fd_);

  fd_ = -1;
}

bool HWCEvent::Signal() {
  if (fd_ < 0) {
    ETRACE("invalid eventfd: %d", fd_);
    return false;
  }

  uint64_t inc = 1;
  ssize_t ret = write(fd_, &inc, sizeof(inc));
  if (ret < 0) {
    ETRACE("couldn't write to eventfd: %zd (%s)", ret, strerror(errno));
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
    ETRACE("couldn't read from eventfd: %zd (%s)", ret, strerror(errno));
    return false;
  }

  if (result != 1) {
    ETRACE("read from eventfd has wrong value: %lu (should be 1)", result);
    return false;
  }

  return true;
}

bool HWCEvent::ShouldWait() {
  return true;
}

}  // namespace hwcomposer
