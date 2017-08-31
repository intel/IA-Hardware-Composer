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
#include "utils_android.h"

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
                                        HWCNativeHandle *handle,
                                        uint32_t layer_type) {
  return CreateGraphicsBuffer(w, h, format, handle, layer_type);
}

bool GrallocBufferHandler::ReleaseBuffer(HWCNativeHandle handle) {
  return ReleaseGraphicsBuffer(handle, fd_);
}

void GrallocBufferHandler::DestroyHandle(HWCNativeHandle handle) {
  DestroyBufferHandle(handle);
}

#ifdef USE_MINIGBM
bool GrallocBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  if (!handle->imported_handle_) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  return ImportGraphicsBuffer(handle, bo, fd_);
}

uint32_t GrallocBufferHandler::GetTotalPlanes(HWCNativeHandle handle) {
  auto gr_handle = (struct cros_gralloc_handle *)handle->imported_handle_;
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
  gralloc_->registerBuffer(gralloc_, handle->handle_);
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

void GrallocBufferHandler::CopyHandle(HWCNativeHandle source,
                                      HWCNativeHandle *target) {
  CopyBufferHandle(source, target);
}

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
