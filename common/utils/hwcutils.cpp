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

#include "hwcutils.h"

#include <poll.h>

#include "hwctrace.h"

namespace hwcomposer {

int HWCPoll(int fd, int timeout) {
  CTRACE();
  int ret;
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;

  if ((ret = poll(fds, 1, timeout)) <= 0) {
    ETRACE("Poll Failed in HWCPoll %s", PRINTERROR());
  }
  return ret;
}

void ResetRectToRegion(const HwcRegion& hwc_region, HwcRect<int>& rect) {
  size_t total_rects = hwc_region.size();
  if (total_rects == 0) {
    rect.left = 0;
    rect.top = 0;
    rect.right = 0;
    rect.bottom = 0;
    return;
  }

  const HwcRect<int>& new_rect = hwc_region.at(0);
  rect.left = new_rect.left;
  rect.top = new_rect.top;
  rect.right = new_rect.right;
  rect.bottom = new_rect.bottom;

  for (uint32_t r = 1; r < total_rects; r++) {
    const HwcRect<int>& temp = hwc_region.at(r);
    rect.left = std::min(rect.left, temp.left);
    rect.top = std::min(rect.top, temp.top);
    rect.right = std::max(rect.right, temp.right);
    rect.bottom = std::max(rect.bottom, temp.bottom);
  }
}

}  // namespace hwcomposer
