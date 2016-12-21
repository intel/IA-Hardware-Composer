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

#ifndef SCOPED_FD_H_
#define SCOPED_FD_H_

#include <unistd.h>

namespace hwcomposer {

class ScopedFd {
 public:
  ScopedFd() = default;
  ScopedFd(int fd) : fd_(fd) {
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
    if (fd_ >= 0)
      close(fd_);
  }

  int Release() {
    int old_fd = fd_;
    fd_ = -1;
    return old_fd;
  }

  int Reset(int fd) {
    if (fd_ >= 0)
      close(fd_);
    fd_ = fd;
    return fd_;
  }

  void Close() {
    if (fd_ >= 0)
      close(fd_);
    fd_ = -1;
  }

  int get() const {
    return fd_;
  }

 private:
  int fd_ = -1;
};

}  // namespace hwcomposer
#endif  // SCOPED_FD_H_
