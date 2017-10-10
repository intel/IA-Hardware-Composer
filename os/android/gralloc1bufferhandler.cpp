/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gralloc1bufferhandler.h"

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <ui/GraphicBuffer.h>
#include <cutils/native_handle.h>

#include <hwcdefs.h>
#include <hwctrace.h>

#include "commondrmutils.h"
#include "utils_android.h"

namespace hwcomposer {

// static
NativeBufferHandler *NativeBufferHandler::CreateInstance(uint32_t fd) {
  Gralloc1BufferHandler *handler = new Gralloc1BufferHandler(fd);
  if (!handler)
    return NULL;

  if (!handler->Init()) {
    ETRACE("Failed to initialize GralocBufferHandlers.");
    delete handler;
    return NULL;
  }
  return handler;
}

Gralloc1BufferHandler::Gralloc1BufferHandler(uint32_t fd) : fd_(fd) {
}

Gralloc1BufferHandler::~Gralloc1BufferHandler() {
  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  gralloc1_dvc->common.close(device_);
}

bool Gralloc1BufferHandler::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ETRACE("Failed to get gralloc module");
    return false;
  }

  ret = gralloc_->methods->open(gralloc_, GRALLOC_HARDWARE_MODULE_ID, &device_);
  if (ret) {
    ETRACE("Failed to open gralloc module");
    return false;
  }

  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  register_ = reinterpret_cast<GRALLOC1_PFN_RETAIN>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_RETAIN));
  release_ = reinterpret_cast<GRALLOC1_PFN_RELEASE>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_RELEASE));
  lock_ = reinterpret_cast<GRALLOC1_PFN_LOCK>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_LOCK));
  unlock_ = reinterpret_cast<GRALLOC1_PFN_UNLOCK>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_UNLOCK));

  dimensions_ =
      reinterpret_cast<GRALLOC1_PFN_GET_DIMENSIONS>(gralloc1_dvc->getFunction(
          gralloc1_dvc, GRALLOC1_FUNCTION_GET_DIMENSIONS));

  return true;
}

bool Gralloc1BufferHandler::CreateBuffer(uint32_t w, uint32_t h, int format,
                                         HWCNativeHandle *handle,
                                         uint32_t layer_type) {
  return CreateGraphicsBuffer(w, h, format, handle, layer_type);
}

bool Gralloc1BufferHandler::ReleaseBuffer(HWCNativeHandle handle) {
  if (!ReleaseGraphicsBuffer(handle, fd_))
    return false;

  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  release_(gralloc1_dvc, handle->imported_handle_);

  return true;
}

void Gralloc1BufferHandler::DestroyHandle(HWCNativeHandle handle) {
  DestroyBufferHandle(handle);
}

bool Gralloc1BufferHandler::ImportBuffer(HWCNativeHandle handle,
                                         HwcBuffer *bo) {
  if (!handle->imported_handle_) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  register_(gralloc1_dvc, handle->imported_handle_);
  return ImportGraphicsBuffer(handle, bo, fd_);
}

uint32_t Gralloc1BufferHandler::GetTotalPlanes(HWCNativeHandle handle) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->imported_handle_;
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  return drm_bo_get_num_planes(gr_handle->format);
}

void Gralloc1BufferHandler::CopyHandle(HWCNativeHandle source,
                                       HWCNativeHandle *target) {
  CopyBufferHandle(source, target);
}

void *Gralloc1BufferHandler::Map(HWCNativeHandle handle, uint32_t x,
                                 uint32_t y, uint32_t width,
                                 uint32_t height, uint32_t * /*stride*/,
                                 void ** map_data, size_t /*plane*/) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->imported_handle_;
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return NULL;
  }

  int acquireFence = -1;
  gralloc1_rect_t rect{};
  rect.left = x;
  rect.top = y;
  rect.width = width;
  rect.height = height;

  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  uint32_t status = lock_(gralloc1_dvc, handle->imported_handle_, GRALLOC_USAGE_SW_READ_RARELY,
          GRALLOC_USAGE_SW_READ_RARELY, &rect, map_data, acquireFence);
  return (GRALLOC1_ERROR_NONE == status) ? *map_data : NULL;
}

int32_t Gralloc1BufferHandler::UnMap(HWCNativeHandle handle,
                                  void * /*map_data*/) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->imported_handle_;
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return GRALLOC1_ERROR_BAD_HANDLE;
  }

  int releaseFence = 0;
  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  return unlock_(gralloc1_dvc, handle->imported_handle_, &releaseFence);
}

}  // namespace hwcomposer
