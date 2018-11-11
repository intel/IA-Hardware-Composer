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

#ifndef OS_ANDROID_PLATFORMDEFINES_H_
#define OS_ANDROID_PLATFORMDEFINES_H_

#ifndef LOG_TAG
#define LOG_TAG "iahwcomposer"
#endif

#ifndef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#endif

#include <android/log.h>
#include <cros_gralloc_handle.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <ui/GraphicBuffer.h>
#include <utils/Trace.h>
#include "platformcommondefines.h"

#define DRV_I915 1
#include <i915_private_android_types.h>

#ifdef _cplusplus
extern "C" {
#endif

struct gralloc_handle {
  buffer_handle_t handle_ = NULL;
  native_handle_t* imported_handle_ = NULL;
  HwcMeta meta_data_;
  uint64_t gralloc1_buffer_descriptor_t_ = 0;
  bool hwc_buffer_ = false;
  void* pixel_memory_ = NULL;
};

typedef struct gralloc_handle* HWCNativeHandle;

#define VTRACE(fmt, ...) ALOGV("%s: " fmt, __func__, ##__VA_ARGS__)
#define DTRACE(fmt, ...) ALOGD("%s: " fmt, __func__, ##__VA_ARGS__)
#define ITRACE(fmt, ...) ALOGI(fmt, ##__VA_ARGS__)
#define WTRACE(fmt, ...) ALOGW("%s: " fmt, __func__, ##__VA_ARGS__)
#define ETRACE(fmt, ...) ALOGE("%s: " fmt, __func__, ##__VA_ARGS__)
#define STRACE() ATRACE_CALL()

inline uint32_t GetNativeBuffer(uint32_t gpu_fd, HWCNativeHandle handle) {
  uint32_t id = 0;
  uint32_t prime_fd = handle->handle_->data[0];
  if (drmPrimeFDToHandle(gpu_fd, prime_fd, &id)) {
    ETRACE("Error generate handle from prime fd %d", prime_fd);
  }
  return id;
}

// _cplusplus
#ifdef _cplusplus
}
#endif

#endif  // OS_ANDROID_PLATFORMDEFINES_H_
