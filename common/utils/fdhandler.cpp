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

#include "fdhandler.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/types.h>

#include "hwctrace.h"

namespace hwcomposer {

FDHandler::FDHandler() {
}

FDHandler::~FDHandler() {
}

bool FDHandler::AddFd(int fd) {
  if (fd < 0) {
    ETRACE("Cannot add negative fd: %d", fd);
    return false;
  }

  auto it = fds_.find(fd);
  if (it != fds_.end()) {
    ETRACE("FD already being watched: %d\n", it->first);
    return false;
  }

  fds_.emplace(fd, FDWatch());

  return true;
}

bool FDHandler::RemoveFd(int fd) {
  const auto &fd_iter = fds_.find(fd);
  if (fd_iter == fds_.end()) {
    ETRACE("FD %d is not being watched.\n", fd);
    return false;
  }

  fds_.erase(fd_iter);
  return true;
}

int FDHandler::Poll(int timeout) {
  nfds_t nfds = fds_.size();
  struct pollfd fds[nfds];

  int i = 0;
  for (auto &it : fds_) {
    fds[i].fd = it.first;
    fds[i].events = POLLIN;
    it.second.idx = i;
    i++;
  }

  int ret = poll(fds, nfds, timeout);

  for (auto &it : fds_) {
    it.second.revents = fds[it.second.idx].revents;
  }

  return ret;
}

int FDHandler::IsReady(int fd) const {
  const auto &it = fds_.find(fd);
  if (it == fds_.end()) {
    ETRACE("FD %d is being watched but we can't find it.\n", fd);
    return false;
  }

  int revents = it->second.revents;
  if (revents & POLLIN)
    return 1;
  else if (revents & POLLERR)
    return -1;
  else if (revents & POLLNVAL)
    return -2;
  else
    return 0;
}

FDHandler::FDWatch::FDWatch() : idx(0), revents(0) {
}

}  // namespace hwcomposer
