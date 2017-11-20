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

#ifndef OS_LINUX_PLATFORMCOMMONDEFINES_H_
#define OS_LINUX_PLATFORMCOMMONDEFINES_H_

#ifdef USE_VK
#include <vulkan/vulkan.h>

VkFormat NativeToVkFormat(int native_format);
#endif

#include <hwcbuffer.h>

#define DRM_FORMAT_NONE fourcc_code('0', '0', '0', '0')

#define DRM_FORMAT_NV12_Y_TILED_INTEL fourcc_code('9', '9', '9', '6')
// minigbm specific DRM_FORMAT_YVU420_ANDROID enum
#define DRM_FORMAT_YVU420_ANDROID fourcc_code('9', '9', '9', '7')

#endif  // OS_LINUX_PLATFORMCOMMONDEFINES_H_
