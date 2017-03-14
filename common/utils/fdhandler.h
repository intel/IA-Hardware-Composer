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

#ifndef COMMON_UTILS_FDHANDLER_H_
#define COMMON_UTILS_FDHANDLER_H_

#include <map>

namespace hwcomposer {

// Class wrapper around poll() and struct pollfd.
// It keeps the fds added on it so one doesn't have to keep
// recreating/populating the pollfd manually.
class FDHandler {
 public:
  FDHandler();
  virtual ~FDHandler();

  // Add fd to the list of fds that we care about. This makes ::Poll watch for
  // this fd when called.
  bool AddFd(int fd);

  // Remove the fd from the list of fds that we are watching.
  bool RemoveFd(int fd);

  // Call poll() on the list of fds that we are watching. Will block if
  // timemout > 0. Store the result from the poll request, so it can be queried
  // with ::IsReady().
  //  - timeout: time in miliseconds to stay blocked before returning if no fd
  //  is ready.
  //  - return: number of fds ready. If return is 0, it means we timed out. If
  //  return is < 0, an error has ocurred.
  int Poll(int timeout);

  // Check whether this fd is ready.
  // - return: 1 if fd is ready to read
  //           0 if fd is not ready
  //           -1 if there's an error on the fd
  //           -2 if the fd is closed and can't be polled
  int IsReady(int fd) const;

 private:
  struct FDWatch {
    FDWatch();
    int idx;
    short revents;
  };

  std::map<int, FDWatch> fds_;
};

}  // namespace hwcomposer

#endif  // COMMON_UTILS_FDHANDLER_H_
