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

#include "shim.h"
#include "vkshim.h"

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

struct vk_import {
  VkImage image;
  VkDeviceMemory memory;
  VkResult res;
};

struct vk_resource {
  VkImage image;
  VkImageView image_view;
  VkDeviceMemory image_memory;
};

union GpuResourceHandle {
  unsigned gl;
  struct vk_resource vk;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_COMPOSITORDEFS_H_
