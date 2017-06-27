
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

#include "platformdefines.h"

#ifdef USE_MINIGBM
#include <cros_gralloc_handle.h>
#include <cros_gralloc_helpers.h>
#endif

#include <hwcdefs.h>
#include "hwcbuffer.h"

#define HWC_UNUSED(x) ((void)&(x))

#ifdef __cplusplus
extern "C" {
#endif

#define DRM_FORMAT_NONE fourcc_code('0', '0', '0', '0')

// minigbm specific DRM_FORMAT_YVU420_ANDROID enum
#define DRM_FORMAT_YVU420_ANDROID fourcc_code('9', '9', '9', '7')

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
    default:
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
  temp->buffer_ = source->buffer_;
  temp->imported_handle_ = dup_buffer_handle(source->handle_);
  *target = temp;
}

static void DestroyBufferHandle(HWCNativeHandle handle) {
  if (handle->imported_handle_)
    free_buffer_handle(handle->imported_handle_);

  delete handle;
  handle = NULL;
}
#ifdef USE_MINIGBM
static bool CreateGraphicsBuffer(uint32_t w, uint32_t h, int /*format*/,
                                 HWCNativeHandle *handle, bool cursor_usage) {
  struct gralloc_handle *temp = new struct gralloc_handle();
  uint32_t usage =
      GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;
  if (cursor_usage) {
    usage |= GRALLOC_USAGE_CURSOR;
  }

  temp->buffer_ =
      new android::GraphicBuffer(w, h, android::PIXEL_FORMAT_RGBA_8888, usage);
  temp->handle_ = temp->buffer_->handle;
  temp->hwc_buffer_ = true;
  *handle = temp;

  return true;
}

static bool ReleaseGraphicsBuffer(HWCNativeHandle handle, int fd) {
  if (!handle)
    return false;

  if (handle->buffer_.get() && handle->hwc_buffer_) {
    handle->buffer_.clear();
  }

  if (handle->gem_handle_ > 0) {
    struct drm_gem_close gem_close;
    memset(&gem_close, 0, sizeof(gem_close));
    gem_close.handle = handle->gem_handle_;
    int ret = drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret)
      ETRACE("Failed to close gem handle %d", ret);
  }

  handle->gem_handle_ = 0;

  return true;
}

static bool ImportGraphicsBuffer(HWCNativeHandle handle, HwcBuffer *bo,
                                 int fd) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->imported_handle_;
  memset(bo, 0, sizeof(struct HwcBuffer));
  bo->format = gr_handle->format;
  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->prime_fd = gr_handle->fds[0];

  uint32_t id = 0;
  if (drmPrimeFDToHandle(fd, bo->prime_fd, &id)) {
    ETRACE("drmPrimeFDToHandle failed.");
    return false;
  }

  for (size_t p = 0; p < DRV_MAX_PLANES; p++) {
    bo->offsets[p] = gr_handle->offsets[p];
    bo->pitches[p] = gr_handle->strides[p];
    bo->gem_handles[p] = id;
  }

  if (gr_handle->usage & GRALLOC_USAGE_PROTECTED) {
    bo->usage |= hwcomposer::kLayerProtected;
  } else if (gr_handle->usage & GRALLOC_USAGE_CURSOR) {
    bo->usage |= hwcomposer::kLayerCursor;
    // We support DRM_FORMAT_ARGB8888 for cursor.
    bo->format = DRM_FORMAT_ARGB8888;
  } else {
    bo->usage |= hwcomposer::kLayerNormal;
  }

  handle->gem_handle_ = id;

  // switch minigbm specific enum to a standard one
  if (bo->format == DRM_FORMAT_YVU420_ANDROID)
    bo->format = DRM_FORMAT_YVU420;

  return true;
}
#endif
#ifdef __cplusplus
}
#endif
#endif
