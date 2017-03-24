
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

#ifndef OS_ANDROID_DRMUTILS_H_
#define OS_ANDROID_DRMUTILS_H_

#include <stdint.h>
#include <drm_fourcc.h>
#include <system/graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRM_FORMAT_NONE  fourcc_code('0', '0', '0', '0')

//Conversion from HAL to fourcc-based DRM formats
uint32_t GetDrmFormat(int format) {

        switch (format) {
          case HAL_PIXEL_FORMAT_RGBA_8888:
            return DRM_FORMAT_BGRA8888;
          case HAL_PIXEL_FORMAT_RGBX_8888:
            return DRM_FORMAT_BGRX8888;
          case HAL_PIXEL_FORMAT_RGB_888:
            return DRM_FORMAT_BGR888;
          case HAL_PIXEL_FORMAT_RGB_565:
            return DRM_FORMAT_BGR565;
          case HAL_PIXEL_FORMAT_BGRA_8888:
            return DRM_FORMAT_ARGB8888;
          case HAL_PIXEL_FORMAT_YV12:
            return DRM_FORMAT_YVU420;
          default:
            break;
        }

        return DRM_FORMAT_NONE;
}

#ifdef __cplusplus
}
#endif
#endif

