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

#include "gbmbufferhandler.h"

#include <unistd.h>
#include <drm.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

#include <hwcbuffer.h>
#include <hwcdefs.h>
#include <hwctrace.h>
#include <platformdefines.h>

namespace hwcomposer {

#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

// static
NativeBufferHandler *NativeBufferHandler::CreateInstance(uint32_t fd) {
  GbmBufferHandler *handler = new GbmBufferHandler(fd);
  if (!handler)
    return NULL;

  if (!handler->Init()) {
    ETRACE("Failed to initialize GbmBufferHandler.");
    delete handler;
    return NULL;
  }
  return handler;
}

static void calculate_offsets(const struct gbm_import_fd_data &import_data,
                              uint32_t gem_handle, uint32_t *pitches,
                              uint32_t *offsets, uint32_t *handles) {
  memset(pitches, 0, 4 * sizeof(uint32_t));
  memset(offsets, 0, 4 * sizeof(uint32_t));
  memset(handles, 0, 4 * sizeof(uint32_t));

  pitches[0] = import_data.stride;
  handles[0] = gem_handle;

  switch (import_data.format) {
    case DRM_FORMAT_YUV420:
      // U and V stride are half of Y plane
      pitches[2] = ALIGN(pitches[0] / 2, 16);
      pitches[1] = ALIGN(pitches[0] / 2, 16);

      // like I420 but U and V are in reverse order
      offsets[2] = offsets[0] + pitches[0] * import_data.height;
      offsets[1] = offsets[2] + pitches[2] * import_data.height / 2;

      handles[1] = handles[2] = handles[0];
      break;
  }
}

static void calculate_aligned_geometry(uint32_t fourcc_format, uint32_t *width,
                                       uint32_t *height) {
  uint32_t width_alignment = 1, height_alignment = 1, extra_height_div = 0;
  switch (fourcc_format) {
    case DRM_FORMAT_YUV420:
      width_alignment = 32;
      height_alignment = 2;
      extra_height_div = 2;
      break;
    case DRM_FORMAT_NV16:
      width_alignment = 2;
      extra_height_div = 1;
      break;
    case DRM_FORMAT_YUYV:
      width_alignment = 2;
      break;
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV12:
      width_alignment = 2;
      height_alignment = 2;
      extra_height_div = 2;
      break;
  }

  *width = ALIGN(*width, width_alignment);
  *height = ALIGN(*height, height_alignment);

  if (extra_height_div)
    *height += *height / extra_height_div;

  *width = ALIGN(*width, 64);

  if (fourcc_format == DRM_FORMAT_YUV420)
    *width = ALIGN(*width, 128);
}

GbmBufferHandler::GbmBufferHandler(uint32_t fd) : fd_(fd), device_(0) {
}

GbmBufferHandler::~GbmBufferHandler() {
  if (device_)
    gbm_device_destroy(device_);
}

bool GbmBufferHandler::Init() {
  device_ = gbm_create_device(fd_);
  if (!device_) {
    ETRACE("failed to create gbm device \n");
    return false;
  }

  return true;
}

bool GbmBufferHandler::CreateBuffer(uint32_t w, uint32_t h, int format,
                                    HWCNativeHandle *handle) {
  struct gbm_bo *bo = gbm_bo_create(device_, w, h, GBM_FORMAT_XRGB8888,
                                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  struct gbm_handle *temp = new struct gbm_handle();
  temp->import_data.fd = gbm_bo_get_fd(bo);
  temp->import_data.width = gbm_bo_get_width(bo);
  temp->import_data.height = gbm_bo_get_height(bo);
  temp->import_data.stride = gbm_bo_get_stride(bo);
  temp->import_data.format = gbm_bo_get_format(bo);
  temp->bo = bo;
  *handle = temp;

  return true;
}

bool GbmBufferHandler::DestroyBuffer(HWCNativeHandle handle) {
  if (handle->bo) {
    gbm_bo_destroy(handle->bo);
    close(handle->import_data.fd);
    delete handle;
    handle = NULL;
  }

  return true;
}

bool GbmBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  memset(bo, 0, sizeof(struct HwcBuffer));
  uint32_t aligned_width = handle->import_data.width;
  uint32_t aligned_height = handle->import_data.height;
  bo->format = handle->import_data.format;
  uint32_t gem_handle = handle->gem_handle;
  if (!gem_handle && handle->bo) {
    handle->gem_handle = gbm_bo_get_handle(handle->bo).u32;
  }

  gem_handle = handle->gem_handle;

  if (!gem_handle) {
    ETRACE("Invalid GEM handle. \n");
    return false;
  }

  calculate_offsets(handle->import_data, gem_handle, bo->pitches, bo->offsets,
                    bo->gem_handles);
  calculate_aligned_geometry(bo->format, &aligned_width, &aligned_height);
  bo->width = aligned_width;
  bo->height = aligned_height;
  bo->prime_fd = handle->import_data.fd;

  return true;
}
}
