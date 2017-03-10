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

bool FDHandler::AddFd(int id, int fd) {
  if (fd < 0) {
    ETRACE("Cannot add negative fd: %d", fd);
    return false;
  }

  auto it = fds_.find(fd);
  if (it != fds_.end()) {
    ETRACE("FD already being watched: %d, id: %d\n", it->first, it->second.id);
    return false;
  }

  const auto &it_id = ids_to_fds_.find(id);
  if (it_id != ids_to_fds_.end()) {
    ETRACE("ID already being watched: %d, fd: %d\n", it_id->first, it_id->second);
    return false;
  }

  fds_.emplace(fd, FDWatch(fd, id));
  ids_to_fds_.emplace(id, fd);

  return true;
}

bool FDHandler::RemoveFd(int id) {
  const auto &id_iter = ids_to_fds_.find(id);
  if (id_iter == ids_to_fds_.end()) {
    ETRACE("ID %d is not being watched.\n", id);
    return false;
  }

  const auto &fd_iter = fds_.find(id_iter->second);
  if (fd_iter == fds_.end()) {
    ETRACE("FD %d is not being watched.\n", id_iter->second);
    ids_to_fds_.erase(id_iter);
    return false;
  }

  ids_to_fds_.erase(id_iter);
  fds_.erase(fd_iter);
  return true;
}

bool FDHandler::UpdateFd(int id, int fd) {
  const auto &id_iter = ids_to_fds_.find(id);
  if (id_iter == ids_to_fds_.end()) {
    ETRACE("ID %d is not being watched.\n", id);
    return false;
  }

  const auto &fd_iter = fds_.find(id_iter->second);
  if (fd_iter == fds_.end()) {
    ETRACE("FD %d is not being watched.\n", id_iter->second);
    return false;
  }

  ids_to_fds_.erase(id_iter);
  ids_to_fds_.emplace(id, fd);

  fds_.erase(fd_iter);
  fds_.emplace(fd, FDWatch(fd, id));

  return true;
}

int FDHandler::Poll(int timeout) {
  nfds_t nfds = fds_.size();
  struct pollfd fds[nfds];

  int i = 0;
  for (const auto &it : fds_) {
    fds[i].fd = it.second.fd;
    fds[i].events = POLLIN;
    i++;
  }

  int ret = poll(fds, nfds, timeout);

  for (i = 0; i < nfds; i++) {
    auto it = fds_.find(fds[i].fd);
    it->second.revents = fds[i].revents;
  }

  return ret;
}

int FDHandler::IsReady(int id) const {
  const auto &it_id = ids_to_fds_.find(id);
  if (it_id == ids_to_fds_.end()) {
    ETRACE("ID %d is not being watched.\n", id);
    return false;
  }

  const auto &it = fds_.find(it_id->second);
  if (it == fds_.end()) {
    ETRACE("FD %d is being watched but we can't find it.\n", it_id->second);
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

FDHandler::FDWatch::FDWatch(int fd, int id)
    : fd(fd),
      id(id),
      revents(0) {}

} // namespace hwcomposer
