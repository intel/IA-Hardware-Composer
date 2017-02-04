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

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <ui/GraphicBuffer.h>
#include <cutils/native_handle.h>

#ifdef USE_MINIGBM
#include <cros_gralloc_handle.h>
#else
#include <gralloc_drm_handle.h>
#endif

#include <hwcdefs.h>
#include <hwctrace.h>
#include "drmhwcgralloc.h"

namespace hwcomposer {

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
}

bool GrallocBufferHandler::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ETRACE("Failed to open gralloc module");
    return false;
  }
  return true;
}

bool GrallocBufferHandler::CreateBuffer(uint32_t w, uint32_t h, int format,
                                        HWCNativeHandle *handle) {
  struct gralloc_handle *temp = new struct gralloc_handle();
  temp->buffer_ =
      new android::GraphicBuffer(w, h, android::PIXEL_FORMAT_RGBA_8888,
                                 GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER |
                                     GRALLOC_USAGE_HW_COMPOSER);
  temp->handle_ = temp->buffer_->handle;
  gralloc_->registerBuffer(gralloc_, temp->handle_);
  *handle = temp;

  return true;
}

bool GrallocBufferHandler::DestroyBuffer(HWCNativeHandle handle) {
  if (!handle)
    return false;

  if (handle->handle_) {
    gralloc_->unregisterBuffer(gralloc_, handle->handle_);
    handle->buffer_.clear();
  }

  delete handle;
  handle = NULL;

  return true;
}
#ifdef USE_MINIGBM
bool GrallocBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  memset(bo, 0, sizeof(struct HwcBuffer));
  int ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_DRM_IMPORT,
                              handle->handle_, fd_, bo);
  if (ret) {
    ETRACE("GRALLOC_MODULE_PERFORM_DRM_IMPORT failed %d", ret);
    return false;
  }

  auto gr_handle = (struct cros_gralloc_handle *)handle;
  if (!gr_handle) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  if (gr_handle->usage & GRALLOC_USAGE_PROTECTED) {
    bo->usage |= hwcomposer::kLayerProtected;
  } else if (gr_handle->usage & GRALLOC_USAGE_CURSOR) {
    bo->usage |= hwcomposer::kLayerCursor;
  }

  return true;
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
#endif
}
