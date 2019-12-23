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

#ifndef COMMON_DRM_COMPOSITOR_VA_VAUTILS_H_
#define COMMON_DRM_COMPOSITOR_VA_VAUTILS_H_
#include <stdint.h>
#include <cutils/native_handle.h>

namespace android {
#define DRM_FORMAT_NONE fourcc_code('0', '0', '0', '0')
#define DRM_FORMAT_YVU420_ANDROID fourcc_code('9', '9', '9', '7')
#define DRM_FORMAT_NV12_Y_TILED_INTEL fourcc_code('9', '9', '9', '6')
#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0') /* 2x2 subsampled Cr:Cb plane 10 bits per channel */

struct gralloc_handle {
  buffer_handle_t handle_ = NULL;
  uint64_t gralloc1_buffer_descriptor_t_ = 0;
};

typedef struct gralloc_handle* DRMHwcNativeHandle;


int DrmFormatToVAFormat(int format);
int DrmFormatToRTFormat(int format);
int DrmFormatToHALFormat(int format) ;
bool IsSupportedMediaFormat(uint32_t format);

}  // namespace hwcomposer

#endif  // COMMON_COMPOSITOR_VA_VAUTILS_H_
