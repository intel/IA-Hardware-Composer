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

#ifndef PLATFORM_DEFINES_
#define PLATFORM_DEFINES_

#include <stdio.h>
#include <stddef.h>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cstddef>
#include <gbm.h>

struct gbm_handle {
  struct gbm_import_fd_planar_data import_data;
  struct gbm_bo* bo = NULL;
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

// _cplusplus
#ifdef _cplusplus
}
#endif

#endif  // PLATFORM_DEFINES_
