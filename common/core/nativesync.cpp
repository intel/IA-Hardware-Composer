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

#include "nativesync.h"

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <platformdefines.h>

#include "hwctrace.h"

namespace hwcomposer {

#ifndef USE_ANDROID_SYNC
struct sw_sync_create_fence_data {
  __u32 value;
  char name[32];
  __s32 fence; /* fd of new fence */
};

#define SW_SYNC_IOC_MAGIC 'W'

#define SW_SYNC_IOC_CREATE_FENCE \
  _IOWR(SW_SYNC_IOC_MAGIC, 0, struct sw_sync_create_fence_data)

#define SW_SYNC_IOC_INC _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

#endif

NativeSync::NativeSync() {
}

NativeSync::~NativeSync() {
  if (timeline_fd_.get() >= 0)
    IncreaseTimelineToPoint(timeline_);
}

bool NativeSync::Init() {
#ifdef USE_ANDROID_SYNC
  timeline_fd_.Reset(open("/dev/sw_sync", O_RDWR));
#else
  timeline_fd_.Reset(open("/sys/kernel/debug/sync/sw_sync", O_RDWR));
#endif
  if (timeline_fd_.get() < 0) {
    ETRACE("Failed to create sw sync timeline %s", PRINTERROR());
    return false;
  }

  return true;
}

int NativeSync::CreateNextTimelineFence() {
  ++timeline_;
  return sw_sync_fence_create(timeline_fd_.get(), "NativeSync", timeline_);
}

bool NativeSync::Wait(int fence) {
  int ret = sync_wait(fence, 1000);
  if (ret) {
    ETRACE("Failed to wait for fence ret=%s\n", PRINTERROR());
    return false;
  }

  return true;
}

int NativeSync::IncreaseTimelineToPoint(int point) {
  int timeline_increase = point - timeline_current_;
  if (timeline_increase <= 0)
    return 0;

  int ret = sw_sync_timeline_inc(timeline_fd_.get(), timeline_increase);
  if (ret)
    ETRACE("Failed to increment sync timeline %s", PRINTERROR());
  else
    timeline_current_ = point;

  return ret;
}

#ifndef USE_ANDROID_SYNC
int NativeSync::sw_sync_fence_create(int fd, const char *name, unsigned value) {
  struct sw_sync_create_fence_data data;
  memset(&data, 0, sizeof(data));
  int err;

  data.value = value;
  size_t srclen = strlen(name);
  strncpy(data.name, name, srclen);
  data.name[srclen] = '\0';
  data.fence = 0;

  err = ioctl(fd, SW_SYNC_IOC_CREATE_FENCE, &data);
  if (err < 0)
    return err;

  return data.fence;
}

int NativeSync::sw_sync_timeline_inc(int fd, unsigned count) {
  uint32_t arg = count;
  return ioctl(fd, SW_SYNC_IOC_INC, &arg);
}
#endif

}  // namespace hwcomposer
