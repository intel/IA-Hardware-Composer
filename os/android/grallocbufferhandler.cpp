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

#include "grallocbufferhandler.h"

#include <xf86drmMode.h>
#include <xf86drm.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <ui/GraphicBuffer.h>
#include <cutils/native_handle.h>
#include "commondrmutils.h"

#ifdef USE_MINIGBM
#include <cros_gralloc_handle.h>
#include <cros_gralloc_helpers.h>
#else
#include <gralloc_drm_handle.h>
#endif

#include <hwcdefs.h>
#include <hwctrace.h>
#include "drmhwcgralloc.h"
#include "commondrmutils.h"

namespace hwcomposer {

#define DRV_MAX_PLANES 4

// static
NativeBufferHandler *NativeBufferHandler::CreateInstance(uint32_t fd) {
  GrallocBufferHandler *handler = new GrallocBufferHandler(fd);
  if (!handler)
    return NULL;

  if (!handler->Init()) {
    ETRACE("Failed to initialize GralocBufferHandlers.");
    delete handler;
    return NULL;
  }
  return handler;
}

GrallocBufferHandler::GrallocBufferHandler(uint32_t fd) : fd_(fd) {
}

GrallocBufferHandler::~GrallocBufferHandler() {
  device->close(device);
}

bool GrallocBufferHandler::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ETRACE("Failed to open gralloc module");
    return false;
  }
  ret = gralloc_->methods->open(gralloc_, GRALLOC_HARDWARE_MODULE_ID, &device);
  if (ret) {
    ETRACE("Failed to open hw_device device");
    return false;
  }

  gralloc1_dvc = reinterpret_cast<gralloc1_device_t *>(device);
  pfn = reinterpret_cast<GRALLOC1_PFN_RETAIN> \
           (gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_RETAIN));

  return true;
}

bool GrallocBufferHandler::CreateBuffer(uint32_t w, uint32_t h, int /*format*/,
                                        HWCNativeHandle *handle) {
  struct gralloc_handle *temp = new struct gralloc_handle();
  temp->buffer_ =
      new android::GraphicBuffer(w, h, android::PIXEL_FORMAT_RGBA_8888,
                                 GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER |
                                     GRALLOC_USAGE_HW_COMPOSER);
  temp->handle_ = temp->buffer_->handle;
  (*pfn)(gralloc1_dvc, temp->handle_);
  *handle = temp;

  return true;
}

bool GrallocBufferHandler::DestroyBuffer(HWCNativeHandle handle) {
  if (!handle)
    return false;

  if (handle->handle_) {
    (*pfn)(gralloc1_dvc, handle->handle_);
    handle->buffer_.clear();
  }

  delete handle;
  handle = NULL;

  return true;
}
#ifdef USE_MINIGBM
bool GrallocBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->handle_;
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  memset(bo, 0, sizeof(struct HwcBuffer));
  bo->format = gr_handle->format;
  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->prime_fd = gr_handle->fds[0];

  uint32_t id;
  if (drmPrimeFDToHandle(fd_, bo->prime_fd, &id)) {
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
  }

  return true;
}

uint32_t GrallocBufferHandler::GetTotalPlanes(HWCNativeHandle handle) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->handle_;
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  return drm_bo_get_num_planes(gr_handle->format);
}
#else
bool GrallocBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  hwc_drm_bo_t hwc_bo;
  int ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_DRM_IMPORT, fd_,
                              handle->handle_, &hwc_bo);
  if (ret) {
    ETRACE("GRALLOC_MODULE_PERFORM_DRM_IMPORT failed %d", ret);
    return false;
  }

  memset(bo, 0, sizeof(struct HwcBuffer));
  gralloc_drm_handle_t *gr_handle = gralloc_drm_handle(handle->handle_);
  bo->width = hwc_bo.width;
  bo->height = hwc_bo.height;
  bo->format = hwc_bo.format;
  for (uint32_t i = 0; i < 4; i++) {
    bo->pitches[i] = hwc_bo.pitches[i];
    bo->offsets[i] = hwc_bo.offsets[i];
    bo->gem_handles[i] = hwc_bo.gem_handles[i];
  }
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  if (gr_handle->usage & GRALLOC_USAGE_PROTECTED) {
    bo->usage |= hwcomposer::kLayerProtected;
  } else if (gr_handle->usage & GRALLOC_USAGE_CURSOR) {
    bo->usage |= hwcomposer::kLayerCursor;
  }

  bo->prime_fd = gr_handle->prime_fd;

  return true;
}

// stubs
uint32_t GrallocBufferHandler::GetTotalPlanes(HWCNativeHandle /*handle*/) {
  return 0;
}
#endif

void *GrallocBufferHandler::Map(HWCNativeHandle /*handle*/, uint32_t /*x*/,
                                uint32_t /*y*/, uint32_t /*width*/,
                                uint32_t /*height*/, uint32_t * /*stride*/,
                                void ** /*map_data*/, size_t /*plane*/) {
  return NULL;
}

void GrallocBufferHandler::UnMap(HWCNativeHandle /*handle*/,
                                 void * /*map_data*/) {
}

}  // namespace hwcomposer
