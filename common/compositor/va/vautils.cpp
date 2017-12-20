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

#include "vautils.h"

#include <platformdefines.h>

#include <drm_fourcc.h>

namespace hwcomposer {

int DrmFormatToVAFormat(int format) {
  switch (format) {
    case DRM_FORMAT_NV12:
      return VA_FOURCC_NV12;
    case DRM_FORMAT_YVU420:
      return VA_FOURCC_YV12;
    case DRM_FORMAT_YUV420:
      return VA_FOURCC('I', '4', '2', '0');
    case DRM_FORMAT_YUV422:
      return VA_FOURCC_YUY2;
    case DRM_FORMAT_UYVY:
      return VA_FOURCC_UYVY;
    case DRM_FORMAT_YUYV:
      return VA_FOURCC_YUY2;
    case DRM_FORMAT_P010:
      return VA_FOURCC_P010;
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_AYUV:
    default:
      break;
  }
  return 0;
}

int DrmFormatToRTFormat(int format) {
  switch (format) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
      return VA_RT_FORMAT_YUV420;
    case DRM_FORMAT_YUV422:
      return VA_RT_FORMAT_YUV422;
    case DRM_FORMAT_YUV444:
      return VA_RT_FORMAT_YUV444;
    case DRM_FORMAT_P010:
      return VA_RT_FORMAT_YUV420_10BPP;
    default:
      break;
  }
  return 0;
}

}  // namespace hwcomposer
