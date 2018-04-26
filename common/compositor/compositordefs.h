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

#ifndef COMMON_COMPOSITOR_COMPOSITORDEFS_H_
#define COMMON_COMPOSITOR_COMPOSITORDEFS_H_

#include <stdint.h>

#ifdef USE_GL
#include "shim.h"
#elif USE_VK
#include "vkshim.h"
#endif

#include <platformdefines.h>

#include <va/va.h>

namespace hwcomposer {

// clang-format off
// Column-major order:
// float mat[4] = { 1, 2, 3, 4 } ===
// [ 1 3 ]
// [ 2 4 ]
static float TransformMatrices[] = {
    1.0f, 0.0f, 0.0f, 1.0f,  // identity matrix
    0.0f, 1.0f, 1.0f, 0.0f,  // swap x and y
};
// clang-format on

#if USE_DC
typedef unsigned GpuResourceHandle;
typedef struct dc_import {
  HWCNativeHandle handle_ = 0;
  uint32_t drm_fd_ = 0;
} ResourceHandle;
typedef void* GpuDisplay;
#elif USE_GL
typedef unsigned GpuResourceHandle;
typedef struct gl_import {
  EGLImageKHR image_ = 0;
  GLuint texture_ = 0;
  GLuint fb_ = 0;
  HWCNativeHandle handle_ = 0;
  uint32_t drm_fd_ = 0;
} ResourceHandle;
typedef EGLDisplay GpuDisplay;
#elif USE_VK
typedef struct vk_resource {
  VkImage image;
  VkImageView image_view;
} GpuResourceHandle;
typedef struct vk_import {
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkImageView texture_ = VK_NULL_HANDLE;
  HWCNativeHandle handle_ = 0;
  uint32_t drm_fd_ = 0;
} ResourceHandle;

typedef VkDevice GpuDisplay;
#else
typedef unsigned GpuResourceHandle;
typedef void* ResourceHandle;
typedef void* GpuDisplay;
#endif

typedef struct media_import {
  VASurfaceID surface_ = VA_INVALID_ID;
  HWCNativeHandle handle_ = 0;
  uint32_t drm_fd_ = 0;
} MediaResourceHandle;

typedef void* MediaDisplay;

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_COMPOSITORDEFS_H_
