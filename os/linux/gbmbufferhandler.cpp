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

#include "drmutils.h"

namespace hwcomposer {

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
  uint32_t gbm_format = format;
  if (gbm_format == 0)
    gbm_format = GBM_FORMAT_XRGB8888;

  gbm_bo_ = gbm_bo_create(device_, w, h, gbm_format,
                          GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!gbm_bo_) {
    gbm_bo_ = gbm_bo_create(device_, w, h, gbm_format, GBM_BO_USE_RENDERING);
    if (!gbm_bo_) {
      ETRACE("GbmBufferHandler: failed to create gbm_bo");
      return false;
    }
  }

  struct gbm_handle *temp = new struct gbm_handle();
  temp->import_data.width = gbm_bo_get_width(gbm_bo_);
  temp->import_data.height = gbm_bo_get_height(gbm_bo_);
  temp->import_data.format = gbm_bo_get_format(gbm_bo_);
#if USE_MINIGBM
  temp->import_data.fds[0] = gbm_bo_get_plane_fd(gbm_bo_, 0);
  size_t total_planes = gbm_bo_get_num_planes(gbm_bo_);
  for (size_t i = 0; i < total_planes; i++) {
    temp->import_data.offsets[i] = gbm_bo_get_plane_offset(gbm_bo_, i);
    temp->import_data.strides[i] = gbm_bo_get_plane_stride(gbm_bo_, i);
  }
  temp->total_planes = total_planes;
#else
  temp->import_data.fd = gbm_bo_get_fd(gbm_bo_);
  temp->import_data.stride = gbm_bo_get_stride(gbm_bo_);
  temp->total_planes = drm_bo_get_num_planes(temp->import_data.format);
#endif

  *handle = temp;

  return true;
}

bool GbmBufferHandler::DestroyBuffer(HWCNativeHandle handle) {
  if (gbm_bo_) {
    gbm_bo_destroy(gbm_bo_);
#ifdef USE_MINIGBM
    close(handle->import_data.fds[0]);
#else
    close(handle->import_data.fd);
#endif
    delete handle;
    handle = NULL;
  }

  return true;
}

bool GbmBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  memset(bo, 0, sizeof(struct HwcBuffer));
  uint32_t gem_handle = 0;
  bo->format = handle->import_data.format;
  if (!gbm_bo_) {
#if USE_MINIGBM
    gbm_bo_ = gbm_bo_import(device_, GBM_BO_IMPORT_FD_PLANAR,
                            &handle->import_data, GBM_BO_USE_SCANOUT);
    if (!gbm_bo_) {
      gbm_bo_ = gbm_bo_import(device_, GBM_BO_IMPORT_FD_PLANAR,
                              &handle->import_data, GBM_BO_USE_RENDERING);
      if (!gbm_bo_) {
        ETRACE("can't import bo");
      }
    }
#else
    gbm_bo_ = gbm_bo_import(device_, GBM_BO_IMPORT_FD, &handle->import_data,
                            GBM_BO_USE_SCANOUT);
    if (!gbm_bo_) {
      gbm_bo_ = gbm_bo_import(device_, GBM_BO_IMPORT_FD, &handle->import_data,
                              GBM_BO_USE_RENDERING);
      if (!gbm_bo_) {
        ETRACE("can't import bo");
      }
    }
#endif
  }

  gem_handle = gbm_bo_get_handle(gbm_bo_).u32;

  if (!gem_handle) {
    ETRACE("Invalid GEM handle. \n");
    return false;
  }

  bo->width = handle->import_data.width;
  bo->height = handle->import_data.height;

#if USE_MINIGBM
  bo->prime_fd = handle->import_data.fds[0];
  size_t total_planes = gbm_bo_get_num_planes(gbm_bo_);
  for (size_t i = 0; i < total_planes; i++) {
    bo->gem_handles[i] = gem_handle;
    bo->offsets[i] = gbm_bo_get_plane_offset(gbm_bo_, i);
    bo->pitches[i] = gbm_bo_get_plane_stride(gbm_bo_, i);
  }
#else
  bo->prime_fd = handle->import_data.fd;
  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;
  bo->pitches[0] = gbm_bo_get_stride(gbm_bo_);
#endif

  return true;
}

uint32_t GbmBufferHandler::GetTotalPlanes(HWCNativeHandle handle) {
  return handle->total_planes;
}

void *GbmBufferHandler::Map(HWCNativeHandle handle, uint32_t x, uint32_t y,
                            uint32_t width, uint32_t height, uint32_t *stride,
                            void **map_data, size_t plane) {
  if (!gbm_bo_)
    return NULL;

  return gbm_bo_map(gbm_bo_, x, y, width, height, GBM_BO_TRANSFER_WRITE, stride,
                    map_data, plane);
}

void GbmBufferHandler::UnMap(HWCNativeHandle handle, void *map_data) {
  if (!gbm_bo_)
    return;

  return gbm_bo_unmap(gbm_bo_, map_data);
}

}  // namespace hwcomposer
