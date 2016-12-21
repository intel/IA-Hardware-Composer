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

#ifndef NATIVE_FENCE_H_
#define NATIVE_FENCE_H_

#include <unistd.h>

#include <scopedfd.h>

namespace hwcomposer {

struct NativeFence {
  NativeFence() = default;
  NativeFence(int fd) {
    fd_.Reset(fd);
  }
  NativeFence(NativeFence &&rhs) {
    fd_ = rhs.fd_.Release();
  }

  NativeFence &operator=(NativeFence &&rhs) {
    fd_ = rhs.fd_.Release();
    return *this;
  }

  int Reset(int fd) {
    fd_.Reset(fd);
    return fd_.get();
  }

  int Release() {
    return fd_.Release();
  }

  int get() const {
    return fd_.get();
  }

  operator bool() const {
    return fd_.get() > 0;
  }

 private:
  ScopedFd fd_;
};

}  // namespace hwcomposer
#endif  // NATIVE_FENCE_H_
