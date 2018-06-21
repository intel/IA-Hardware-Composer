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

#include "platformdefines.h"

#ifndef DISABLE_VA
#include <va/va_android.h>
#endif

#define ANDROID_DISPLAY_HANDLE 0x18C34078

#ifdef USE_VK
#include <drm/drm_fourcc.h>

VkFormat NativeToVkFormat(int native_format) {
  switch (native_format) {
    case DRM_FORMAT_R8:
      return VK_FORMAT_R8_UNORM;
    case DRM_FORMAT_GR88:
      return VK_FORMAT_R8G8_UNORM;
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_RGBX4444:
      return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_BGRA4444:
      return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_ARGB1555:
      return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_RGBA5551:
      return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_BGRA5551:
      return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
    case DRM_FORMAT_RGB565:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case DRM_FORMAT_BGR565:
      return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case DRM_FORMAT_RGB888:
      return VK_FORMAT_B8G8R8_UNORM;
    case DRM_FORMAT_BGR888:
      return VK_FORMAT_R8G8B8_UNORM;
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_ARGB2101010:
      return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_ABGR2101010:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    default:
      ETRACE("drm_format %d unhandled\n", native_format);
      return VK_FORMAT_UNDEFINED;
  }
}
#endif

#ifndef DISABLE_VA
void* GetVADisplay(uint32_t gpu_fd) {
  HWC_UNUSED(gpu_fd);
  unsigned int native_display = ANDROID_DISPLAY_HANDLE;
  return vaGetDisplay(&native_display);
}
#endif
