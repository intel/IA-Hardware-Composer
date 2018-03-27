
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

#ifndef OS_UTILS_ANDROID_H_
#define OS_UTILS_ANDROID_H_

#include <xf86drmMode.h>
#include <xf86drm.h>

#include <stdint.h>
#include <drm_fourcc.h>
#include <system/graphics.h>
#include <hardware/gralloc1.h>

#include "platformdefines.h"

#include <cros_gralloc_handle.h>
#include <cros_gralloc_helpers.h>

#include <hwcdefs.h>
#include "hwctrace.h"
#include "hwcutils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_MAX_PLANES 4

// Conversion from HAL to fourcc-based DRM formats
static uint32_t GetDrmFormatFromHALFormat(int format) {
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
    case HAL_PIXEL_FORMAT_RGBA_FP16:
      return DRM_FORMAT_XBGR161616;
    case HAL_PIXEL_FORMAT_RGBA_1010102:
      return DRM_FORMAT_ABGR2101010;
    default:
      break;
  }

  return DRM_FORMAT_NONE;
}

static uint32_t DrmFormatToHALFormat(int format) {
  switch (format) {
    case DRM_FORMAT_BGRA8888:
      return HAL_PIXEL_FORMAT_RGBA_8888;
    case DRM_FORMAT_BGRX8888:
      return HAL_PIXEL_FORMAT_RGBX_8888;
    case DRM_FORMAT_BGR888:
      return HAL_PIXEL_FORMAT_RGB_888;
    case DRM_FORMAT_BGR565:
      return HAL_PIXEL_FORMAT_RGB_565;
    case DRM_FORMAT_ARGB8888:
      return HAL_PIXEL_FORMAT_BGRA_8888;
    case DRM_FORMAT_YVU420:
      return HAL_PIXEL_FORMAT_YV12;
    case DRM_FORMAT_R8:
      return HAL_PIXEL_FORMAT_BLOB;
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R16:
      return HAL_PIXEL_FORMAT_Y16;
    case DRM_FORMAT_ABGR8888:
      return HAL_PIXEL_FORMAT_RGBA_8888;
    case DRM_FORMAT_RGB332:  //('R', 'G', 'B', '8') /* [7:0] R:G:B 3:3:2 */
      return 0;
    case DRM_FORMAT_BGR233:  //('B', 'G', 'R', '8') /* [7:0] B:G:R 2:3:3 */
      return 0;

    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:
      return 0;
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:
      return 0;
    case DRM_FORMAT_RGB565:
      return HAL_PIXEL_FORMAT_RGB_565;
    case DRM_FORMAT_RGB888:
      return HAL_PIXEL_FORMAT_RGB_888;
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_RGBA8888:
      return 0;
    case DRM_FORMAT_ABGR2101010:
      return HAL_PIXEL_FORMAT_RGBA_1010102;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
      return 0;
    case DRM_FORMAT_YUYV:
      return HAL_PIXEL_FORMAT_YCbCr_422_I;
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_AYUV:
      ETRACE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_NV12:
      return HAL_PIXEL_FORMAT_NV12;
    case DRM_FORMAT_NV21:
      return HAL_PIXEL_FORMAT_YCrCb_420_SP;
    case DRM_FORMAT_NV16:
      return HAL_PIXEL_FORMAT_YCbCr_422_SP;
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_YUV410:
    case DRM_FORMAT_YVU410:
    case DRM_FORMAT_YUV411:
    case DRM_FORMAT_YVU411:
      ETRACE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_YUV420:
      return HAL_PIXEL_FORMAT_YCbCr_420_888;
    case DRM_FORMAT_YVU420_ANDROID:
      return HAL_PIXEL_FORMAT_YV12;
    case DRM_FORMAT_YUV422:
      return HAL_PIXEL_FORMAT_YCbCr_422_888;
    case DRM_FORMAT_YVU422:
      ETRACE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_YUV444:
      return HAL_PIXEL_FORMAT_YCbCr_444_888;
    case DRM_FORMAT_YVU444:
      ETRACE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_NV12_Y_TILED_INTEL:
      return HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL;
    case DRM_FORMAT_P010:
      return HAL_PIXEL_FORMAT_P010_INTEL;
    case DRM_FORMAT_XBGR161616:
      return HAL_PIXEL_FORMAT_RGBA_FP16;
    default:
      return 0;
      break;
  }

  return DRM_FORMAT_NONE;
}

static native_handle_t *dup_buffer_handle(buffer_handle_t handle) {
  native_handle_t *new_handle =
      native_handle_create(handle->numFds, handle->numInts);
  if (new_handle == NULL)
    return NULL;

  const int *old_data = handle->data;
  int *new_data = new_handle->data;
  for (int i = 0; i < handle->numFds; i++) {
    *new_data = dup(*old_data);
    old_data++;
    new_data++;
  }
  memcpy(new_data, old_data, sizeof(int) * handle->numInts);

  return new_handle;
}

static void free_buffer_handle(native_handle_t *handle) {
  int ret = native_handle_close(handle);
  if (ret)
    ETRACE("Failed to close native handle %d", ret);
  ret = native_handle_delete(handle);
  if (ret)
    ETRACE("Failed to delete native handle %d", ret);
}

static void CopyBufferHandle(HWCNativeHandle source, HWCNativeHandle *target) {
  struct gralloc_handle *temp = new struct gralloc_handle();
  temp->handle_ = source->handle_;
  temp->gralloc1_buffer_descriptor_t_ = 0;
  temp->imported_handle_ = dup_buffer_handle(source->handle_);
  temp->hwc_buffer_ = false;
  *target = temp;
}

static void DestroyBufferHandle(HWCNativeHandle handle) {
  if (handle->imported_handle_)
    free_buffer_handle(handle->imported_handle_);

  delete handle;
  handle = NULL;
}

static bool ImportGraphicsBuffer(HWCNativeHandle handle, int fd) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->imported_handle_;
  memset(&(handle->meta_data_), 0, sizeof(struct HwcBuffer));
  handle->meta_data_.format_ = gr_handle->format;
  handle->meta_data_.tiling_mode_ = gr_handle->tiling_mode;
  handle->meta_data_.width_ = gr_handle->width;
  handle->meta_data_.height_ = gr_handle->height;
  handle->meta_data_.native_format_ = gr_handle->droid_format;

  int32_t numplanes = gr_handle->base.numFds;
  handle->meta_data_.num_planes_ = numplanes;

  for (int32_t p = 0; p < numplanes; p++) {
    handle->meta_data_.offsets_[p] = gr_handle->offsets[p];
    handle->meta_data_.pitches_[p] = gr_handle->strides[p];
    handle->meta_data_.prime_fds_[p] = gr_handle->fds[p];
    if (drmPrimeFDToHandle(fd, gr_handle->fds[p],
                           &handle->meta_data_.gem_handles_[p])) {
      ETRACE("drmPrimeFDToHandle failed. %s", PRINTERROR());
      return false;
    }
  }

  if (gr_handle->consumer_usage & GRALLOC1_PRODUCER_USAGE_PROTECTED) {
    handle->meta_data_.usage_ = hwcomposer::kLayerProtected;
  } else if (gr_handle->consumer_usage & GRALLOC1_CONSUMER_USAGE_CURSOR) {
    handle->meta_data_.usage_ = hwcomposer::kLayerCursor;
    // We support DRM_FORMAT_ARGB8888 for cursor.
    handle->meta_data_.format_ = DRM_FORMAT_ARGB8888;
  } else if (hwcomposer::IsSupportedMediaFormat(handle->meta_data_.format_)) {
    handle->meta_data_.usage_ = hwcomposer::kLayerVideo;
  } else {
    handle->meta_data_.usage_ = hwcomposer::kLayerNormal;
  }

  // switch minigbm specific enum to a standard one
  if (handle->meta_data_.format_ == DRM_FORMAT_YVU420_ANDROID)
    handle->meta_data_.format_ = DRM_FORMAT_YVU420;

  return true;
}
#ifdef __cplusplus
}
#endif
#endif
