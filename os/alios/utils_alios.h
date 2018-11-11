
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

#ifndef OS_UTILS_ALIOS_H_
#define OS_UTILS_ALIOS_H_

#include <stdio.h>
#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <drm_fourcc.h>
#include <stdint.h>

#include "platformdefines.h"

#include <yalloc.h>
#include <yalloc_drm.h>
#include <yalloc_drm_handle.h>

#include <hwcdefs.h>
#include "hwctrace.h"
#include "hwcutils.h"

#ifdef __cplusplus
extern "C" {
#endif

// Conversion from HAL to fourcc-based DRM formats
static uint32_t GetDrmFormatFromHALFormat(int format) {
  int ret = 0;

  switch (format) {
    case YUN_HAL_FORMAT_RGBA_8888:
    case YUN_HAL_FORMAT_sRGB_A_8888:
      ret = DRM_FORMAT_ABGR8888;
      break;

    case YUN_HAL_FORMAT_RGBX_8888:
    case YUN_HAL_FORMAT_sRGB_X_8888:
      ret = DRM_FORMAT_XBGR8888;
      break;

    case YUN_HAL_FORMAT_RGB_888:
      ret = DRM_FORMAT_BGR888;
      break;

    case YUN_HAL_FORMAT_RGB_565:
      ret = DRM_FORMAT_RGB565;
      break;

    case YUN_HAL_FORMAT_BGRA_8888:
    case YUN_HAL_FORMAT_sBGR_A_8888:
      ret = DRM_FORMAT_ARGB8888;
      break;

    case YUN_HAL_FORMAT_BGRX_8888:
    case YUN_HAL_FORMAT_sBGR_X_8888:
      ret = DRM_FORMAT_XRGB8888;
      break;

    case YUN_HAL_FORMAT_I420:
      ret = DRM_FORMAT_YUV420;
      break;

    case YUN_HAL_FORMAT_YV12:
      ret = DRM_FORMAT_YVU420;
      break;

    case YUN_HAL_FORMAT_NV12:
    case YUN_HAL_FORMAT_DRM_NV12:
      ret = DRM_FORMAT_NV12;
      break;

    case YUN_HAL_FORMAT_NV21:
    case YUN_HAL_FORMAT_YCrCb_420_SP:
      ret = DRM_FORMAT_NV21;
      break;

    case YUN_HAL_FORMAT_NV16:
    case YUN_HAL_FORMAT_YCbCr_422_SP:
      ret = DRM_FORMAT_NV16;
      break;

    case YUN_HAL_FORMAT_NV61:
      ret = DRM_FORMAT_NV61;
      break;

    case YUN_HAL_FORMAT_UYVY:
      ret = DRM_FORMAT_UYVY;
      break;

    case YUN_HAL_FORMAT_VYUY:
      ret = DRM_FORMAT_VYUY;
      break;

    case YUN_HAL_FORMAT_YUYV:
    case YUN_HAL_FORMAT_YCbCr_422_I:
      ret = DRM_FORMAT_YUYV;
      break;

    case YUN_HAL_FORMAT_YVYU:
      ret = DRM_FORMAT_YVYU;
      break;

    default:
      ETRACE("GetDrmFormatFromHALFormat --> can't get format. \n");
      break;
  }

  ITRACE("GetDrmFormatFromHALFormat --> Format: %c%c%c%c.\n", ret, ret >> 8,
         ret >> 16, ret >> 24);

  return ret;
}

static uint32_t DrmFormatToHALFormat(int format) {
  ITRACE("DrmFormatToHALFormat --> Format: %c%c%c%c.\n", format, format >> 8,
         format >> 16, format >> 24);
  int ret = 0;

  switch (format) {
    /* Use below code validated from Yalloc. */
    case DRM_FORMAT_ABGR8888:
      ret = YUN_HAL_FORMAT_RGBA_8888;
      break;

    case DRM_FORMAT_XBGR8888:
      ret = YUN_HAL_FORMAT_RGBX_8888;
      break;

    case DRM_FORMAT_BGR888:
      ret = YUN_HAL_FORMAT_RGB_888;
      break;

    case DRM_FORMAT_RGB565:
      ret = YUN_HAL_FORMAT_RGB_565;
      break;

    case DRM_FORMAT_ARGB8888:
      ret = YUN_HAL_FORMAT_BGRA_8888;
      break;

    case DRM_FORMAT_XRGB8888:
      ret = YUN_HAL_FORMAT_BGRX_8888;
      break;

    case YUN_HAL_FORMAT_I420:
      ret = DRM_FORMAT_YUV420;
      break;

    case DRM_FORMAT_YUV420:
      ret = YUN_HAL_FORMAT_YV12;
      break;

    case DRM_FORMAT_NV12:
      ret = YUN_HAL_FORMAT_NV12;
      break;

    case DRM_FORMAT_NV21:
      ret = YUN_HAL_FORMAT_NV21;
      break;

    case DRM_FORMAT_NV16:
      ret = YUN_HAL_FORMAT_NV16;
      break;

    case DRM_FORMAT_NV61:
      ret = YUN_HAL_FORMAT_NV61;
      break;

    case DRM_FORMAT_UYVY:
      ret = YUN_HAL_FORMAT_UYVY;
      break;

    case DRM_FORMAT_VYUY:
      ret = YUN_HAL_FORMAT_VYUY;
      break;

    case DRM_FORMAT_YUYV:
      ret = YUN_HAL_FORMAT_YUYV;
      break;

    case DRM_FORMAT_YVYU:
      ret = YUN_HAL_FORMAT_YVYU;
      break;

    default:
      ETRACE("DrmFormatToHALFormat --> Error Format @ line: %d.\n", __LINE__);
      return 0;
      break;
  }

  return ret;
}

static native_target_t *dup_buffer_handle(gb_target_t handle) {
  native_target_t *new_handle =
      native_target_create(handle->fds.num, handle->attributes.num);
  if (new_handle == NULL)
    return NULL;

  int *old_data = handle->fds.data;
  int *new_data = new_handle->fds.data;

  for (int i = 0; i < handle->fds.num; i++) {
    *new_data = dup(*old_data);
    ITRACE("old_fd(%d), new_fd(%d)", *old_data, *new_data);
    old_data++;
    new_data++;
  }

  old_data = handle->attributes.data;
  new_data = new_handle->attributes.data;

  memcpy(new_data, old_data, sizeof(int) * handle->attributes.num);

  return new_handle;
}

static void free_buffer_handle(native_target_t *handle) {
  int ret = native_target_close(handle);
  if (ret)
    ETRACE("Failed to close native target %d", ret);
  ret = native_target_delete(handle);
  if (ret)
    ETRACE("Failed to delete native target %d", ret);
}

static void CopyBufferHandle(HWCNativeHandle source, HWCNativeHandle *target) {
  struct yalloc_handle *temp = new struct yalloc_handle();
  temp->target_ = source->target_;
  temp->imported_target_ = dup_buffer_handle(source->target_);
  temp->hwc_buffer_ = false;
  *target = temp;
}

static void DestroyBufferHandle(HWCNativeHandle handle) {
  if (handle->imported_target_)
    free_buffer_handle(handle->imported_target_);

  delete handle;
}

static struct yalloc_drm_handle_t AttrData2YallocHandle(
    HWCNativeHandle native_handle) {
  native_array_t *attrib_array = &native_handle->imported_target_->attributes;
  struct yalloc_drm_handle_t handle;

  handle.magic = attrib_array->data[0];
  handle.width = attrib_array->data[1];
  handle.height = attrib_array->data[2];
  handle.format = attrib_array->data[3];
  handle.usage = attrib_array->data[4];
  handle.plane_mask = (unsigned int)attrib_array->data[5];
  handle.name = attrib_array->data[6];
  handle.stride = attrib_array->data[7];

  handle.plane_num = attrib_array->data[8];
  handle.bpp[0] = attrib_array->data[9];
  handle.bpp[1] = attrib_array->data[10];
  handle.bpp[2] = attrib_array->data[11];
  handle.aligned_width[0] = attrib_array->data[12];
  handle.aligned_width[1] = attrib_array->data[13];
  handle.aligned_width[2] = attrib_array->data[14];
  handle.aligned_height[0] = attrib_array->data[15];
  handle.aligned_height[1] = attrib_array->data[16];
  handle.aligned_height[2] = attrib_array->data[17];
  handle.tiling_mode = attrib_array->data[18];

  handle.data_owner = attrib_array->data[19];
  memcpy(&handle.data, &attrib_array->data[20], sizeof(handle.data));

  return handle;
}

static bool ImportGraphicsBuffer(HWCNativeHandle handle, int fd) {
  struct yalloc_drm_handle_t handle_data = AttrData2YallocHandle(handle);
  auto gr_handle = &handle_data;
  int32_t total_planes;

  memset(&(handle->meta_data_), 0, sizeof(struct HwcMeta));
  handle->meta_data_.format_ = GetDrmFormatFromHALFormat(gr_handle->format);
  handle->meta_data_.width_ = gr_handle->width;
  handle->meta_data_.height_ = gr_handle->height;
  handle->meta_data_.native_format_ = gr_handle->format;

  total_planes = gr_handle->plane_num;
  for (uint32_t p = 0; p < total_planes; p++) {
    // handle->meta_data_.offsets_[p] = gr_handle->offsets[p];
    handle->meta_data_.pitches_[p] = gr_handle->stride;

    /* yalloc only return one fd */
    handle->meta_data_.prime_fds_[p] = handle->imported_target_->fds.data[0];

    if (drmPrimeFDToHandle(fd, handle->meta_data_.prime_fds_[p],
                           &handle->meta_data_.gem_handles_[p])) {
      ETRACE("drmPrimeFDToHandle failed. %s prime_fd (%d)", PRINTERROR(),
             handle->meta_data_.prime_fds_[p]);
      return false;
    }
    ITRACE("prime_fd (%d), handle (%d)", handle->meta_data_.prime_fds_[p],
           handle->meta_data_.gem_handles_[p]);
  }

  handle->meta_data_.num_planes_ = total_planes;

  if (gr_handle->usage & YALLOC_FLAG_PROTECTED) {
    handle->meta_data_.usage_ = hwcomposer::kLayerProtected;
  } else if (gr_handle->usage & YALLOC_FLAG_CURSOR) {
    handle->meta_data_.usage_ = hwcomposer::kLayerCursor;
    // We support DRM_FORMAT_ARGB8888 for cursor.
    handle->meta_data_.format_ = DRM_FORMAT_ARGB8888;
  } else if (hwcomposer::IsSupportedMediaFormat(handle->meta_data_.format_)) {
    handle->meta_data_.usage_ = hwcomposer::kLayerVideo;
  } else {
    handle->meta_data_.usage_ = hwcomposer::kLayerNormal;
  }

  if (handle->meta_data_.format_ == DRM_FORMAT_YVU420_ANDROID)
    handle->meta_data_.format_ = DRM_FORMAT_YVU420;

  return true;
}
#ifdef __cplusplus
}
#endif
#endif
