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
  struct gbm_bo *bo = gbm_bo_create(device_, w, h, GBM_FORMAT_XRGB8888,
                                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  struct gbm_handle *temp = new struct gbm_handle();
  temp->import_data.width = gbm_bo_get_width(bo);
  temp->import_data.height = gbm_bo_get_height(bo);
  temp->import_data.format = gbm_bo_get_format(bo);
  temp->import_data.fds[0] = gbm_bo_get_plane_fd(bo, 0);
  size_t total_planes = gbm_bo_get_num_planes(bo);
  for (size_t i = 0; i < total_planes; i++) {
    temp->import_data.offsets[i] = gbm_bo_get_plane_offset(bo, i);
    temp->import_data.strides[i] = gbm_bo_get_plane_stride(bo, i);
  }
  temp->bo = bo;
  *handle = temp;

  return true;
}

bool GbmBufferHandler::DestroyBuffer(HWCNativeHandle handle) {
  if (handle->bo) {
    gbm_bo_destroy(handle->bo);
    close(handle->import_data.fds[0]);
    delete handle;
    handle = NULL;
  }

  return true;
}

bool GbmBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  memset(bo, 0, sizeof(struct HwcBuffer));
  uint32_t aligned_width = handle->import_data.width;
  uint32_t aligned_height = handle->import_data.height;
  uint32_t gem_handle = 0;
  bo->format = handle->import_data.format;
  if (!handle->bo) {
    handle->bo = gbm_bo_import(device_, GBM_BO_IMPORT_FD_PLANAR,
                               &handle->import_data, GBM_BO_USE_SCANOUT);
  }

  gem_handle = gbm_bo_get_handle(handle->bo).u32;

  if (!gem_handle) {
    ETRACE("Invalid GEM handle. \n");
    return false;
  }
  uint32_t pitches[4];
  uint32_t offsets[4];
  uint32_t gem_handles[4];
  bo->width = handle->import_data.width;
  bo->height = handle->import_data.height;
  bo->prime_fd = dup(handle->import_data.fds[0]);
  size_t total_planes = gbm_bo_get_num_planes(handle->bo);
  for (size_t i = 0; i < total_planes; i++) {
    bo->gem_handles[i] = gem_handle;
    bo->offsets[i] = gbm_bo_get_plane_offset(handle->bo, i);
    bo->pitches[i] = gbm_bo_get_plane_stride(handle->bo, i);
  }

  return true;
}
}
