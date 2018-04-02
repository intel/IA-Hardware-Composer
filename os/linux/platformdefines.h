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

#ifndef OS_LINUX_PLATFORMDEFINES_H_
#define OS_LINUX_PLATFORMDEFINES_H_

#include <gbm.h>
#include <stddef.h>
#include <stdio.h>
#include <cmath>

#include <va/va_drm.h>

#include <cstring>
#include <algorithm>
#include <cstddef>

#include <libsync.h>

#include "platformcommondefines.h"

struct gbm_handle {
#ifdef USE_MINIGBM
  struct gbm_import_fd_planar_data import_data;
#else
  struct gbm_import_fd_data import_data;
#endif
  struct gbm_bo* bo = NULL;
  struct gbm_bo* imported_bo = NULL;
  uint32_t total_planes = 0;
  HwcBuffer meta_data_;
  bool hwc_buffer_ = false;
  // In case this is true, we expect meta_data_
  // to be filled with correct width, height and
  // format.
  void* pixel_memory_ = NULL;
  uint32_t gbm_flags = 0;
};

typedef struct gbm_handle* HWCNativeHandle;

#ifdef _cplusplus
extern "C" {
#endif

#define VTRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define DTRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define ITRACE(fmt, ...) fprintf(stderr, "\n" fmt, ##__VA_ARGS__)
#define WTRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define ETRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define STRACE() ((void)0)

#ifdef USE_MINIGBM
inline uint32_t GetNativeBuffer(uint32_t gpu_fd, HWCNativeHandle handle) {
  uint32_t id = 0;
  uint32_t prime_fd = handle->import_data.fds[0];
  if (drmPrimeFDToHandle(gpu_fd, prime_fd, &id)) {
    ETRACE("Error generate handle from prime fd %d", prime_fd);
  }
  return id;
}
#else
inline uint32_t GetNativeBuffer(uint32_t gpu_fd, HWCNativeHandle handle) {
  uint32_t id = 0;
  uint32_t prime_fd = handle->import_data.fd;
  if (drmPrimeFDToHandle(gpu_fd, prime_fd, &id)) {
    ETRACE("Error generate handle from prime fd %d", prime_fd);
  }
  return id;
}
#endif

// _cplusplus
#ifdef _cplusplus
}
#endif

#endif  // OS_LINUX_PLATFORMDEFINES_H_
