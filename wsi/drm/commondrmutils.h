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

#ifndef WSI_COMMONDRMUTILS_H_
#define WSI_COMMONDRMUTILS_H_

#include <drm_fourcc.h>

static size_t drm_bo_get_num_planes(uint32_t format) {
  switch (format) {
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_AYUV:
    case DRM_FORMAT_BGR233:
    case DRM_FORMAT_BGR565:
    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_BGRA1010102:
    case DRM_FORMAT_BGRA4444:
    case DRM_FORMAT_BGRA5551:
    case DRM_FORMAT_BGRA8888:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_C8:
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R8:
    case DRM_FORMAT_RG88:
    case DRM_FORMAT_RGB332:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_R16:
      return 1;
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV12_Y_TILED_INTEL:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_P010:
      return 2;
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YVU420_ANDROID:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_YUV422:
      return 3;
  }

  fprintf(stderr, "DRM: UNKNOWN FORMAT %d\n", format);
  return 0;
}

static uint64_t choose_drm_modifier(uint32_t format) {
#ifdef ENABLE_RBC
  switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
      // FIXME: When to choose I915_FORMAT_MOD_Yf_TILED_CCS?
      return I915_FORMAT_MOD_Y_TILED_CCS;
    default:
      return DRM_FORMAT_MOD_NONE;
  }
#else
  return DRM_FORMAT_MOD_NONE;
#endif
}

#endif  // WSI_COMMONDRMUTILS_H_
