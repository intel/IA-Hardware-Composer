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

#include <stdio.h>
#include <stddef.h>
#include <gbm.h>

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
  uint32_t gbm_flags = 0;
  bool use_dumb_buffer_ = false;
};

typedef struct gbm_handle* HWCNativeHandle;
#define GETNATIVEBUFFER(handle) (handle->import_data)

#ifdef USE_MINIGBM
typedef gbm_import_fd_planar_data HWCNativeBuffer;
struct BufferHash {
  size_t operator()(gbm_import_fd_planar_data const& p) const {
    std::size_t seed = 0;
    for (int i = 0; i < GBM_MAX_PLANES; i++) {
      hash_combine_hwc(seed, (const size_t)p.fds[i]);
    }
    // avoid to consider next information?
    hash_combine_hwc(seed, p.width);
    hash_combine_hwc(seed, p.height);
    hash_combine_hwc(seed, p.format);
    return seed;
  }
};

struct BufferEqual {
  bool operator()(const gbm_import_fd_planar_data& p1,
                  const gbm_import_fd_planar_data& p2) const {
    bool equal = true;
    for (int i = 0; i < GBM_MAX_PLANES; i++) {
      equal = equal && (p1.fds[i] == p2.fds[i]);
      if (!equal)
        break;
    }
    if (equal)
      equal = equal && (p1.width == p2.width) && (p1.height == p2.height) &&
              (p1.format == p2.format);
    return equal;
  }
};
#else

typedef gbm_import_fd_data HWCNativeBuffer;
struct BufferHash {
  size_t operator()(gbm_import_fd_data const& p) const {
    std::size_t seed = 0;
    hash_combine_hwc(seed, p.fd);
    hash_combine_hwc(seed, p.width);
    hash_combine_hwc(seed, p.height);
    hash_combine_hwc(seed, p.stride);
    hash_combine_hwc(seed, p.format);
    return seed;
  }
};
struct BufferEqual {
  bool operator()(const gbm_import_fd_data& p1,
                  const gbm_import_fd_data& p2) const {
    bool equal = (p1.fd == p2.fd) && (p1.width == p2.width) &&
                 (p1.height == p2.height) && (p1.stride == p2.stride) &&
                 (p1.format == p2.format);
    return equal;
  }
};

#endif

#ifdef _cplusplus
extern "C" {
#endif

#define VTRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define DTRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define ITRACE(fmt, ...) fprintf(stderr, "\n" fmt, ##__VA_ARGS__)
#define WTRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define ETRACE(fmt, ...) fprintf(stderr, "%s: \n" fmt, __func__, ##__VA_ARGS__)
#define STRACE() ((void)0)
// _cplusplus
#ifdef _cplusplus
}
#endif

#endif  // OS_LINUX_PLATFORMDEFINES_H_
