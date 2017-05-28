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

#ifndef PUBLIC_SCOPEDFD_H_
#define PUBLIC_SCOPEDFD_H_

#include <unistd.h>
#include "spinlock.h"

namespace hwcomposer {

class ScopedFd {
 public:
  ScopedFd() = default;
  explicit ScopedFd(int fd) : fd_(fd) {
  }
  ScopedFd(ScopedFd &&rhs) {
    fd_ = rhs.fd_;
    rhs.fd_ = -1;
  }

  ScopedFd &operator=(ScopedFd &&rhs) {
    Reset(rhs.Release());
    return *this;
  }

  ~ScopedFd() {
    spin_lock_.lock();
    if (fd_ > 0)
      close(fd_);
    spin_lock_.unlock();
  }

  int Release() {
    spin_lock_.lock();
    int old_fd = fd_;
    fd_ = -1;
    spin_lock_.unlock();
    return old_fd;
  }

  int Reset(int fd) {
    spin_lock_.lock();
    if (fd_ > 0)
      close(fd_);
    fd_ = fd;
    spin_lock_.unlock();
    return fd_;
  }

  void Close() {
    spin_lock_.lock();
    if (fd_ > 0)
      close(fd_);
    fd_ = -1;
    spin_lock_.unlock();
  }

  int get() const {
    return fd_;
  }

 private:
  SpinLock spin_lock_;
  int fd_ = -1;
};

}  // namespace hwcomposer
#endif  // PUBLIC_SCOPEDFD_H_
