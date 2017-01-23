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

#include <hwcdefs.h>
#include <hwctrace.h>

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

bool GrallocBufferHandler::ImportBuffer(HWCNativeHandle handle, HwcBuffer *bo) {
  int ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_DRM_IMPORT, fd_,
                              handle->handle_, bo);
  if (ret) {
    ETRACE("GRALLOC_MODULE_PERFORM_DRM_IMPORT failed %d", ret);
    return false;
  }

  uint32_t usage = 0;
  if (bo->usage & GRALLOC_USAGE_CURSOR)
    usage |= hwcomposer::kLayerCursor;

  if (bo->usage & GRALLOC_USAGE_PROTECTED)
    usage |= hwcomposer::kLayerProtected;

  bo->usage = usage;

  return true;
}
}
