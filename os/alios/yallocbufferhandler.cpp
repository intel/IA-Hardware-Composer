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

#include "yallocbufferhandler.h"

#include <drm.h>
#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>

#include <hwcdefs.h>
#include <hwctrace.h>
#include <platformdefines.h>

#include "commondrmutils.h"

#include <memory>

#include <utils_alios.h>

namespace hwcomposer {

// static
NativeBufferHandler *NativeBufferHandler::CreateInstance(uint32_t fd) {
  YallocBufferHandler *handler = new YallocBufferHandler(fd);
  if (!handler)
    return NULL;

  if (!handler->Init()) {
    ETRACE("Failed to initialize YallocBufferHandler.");
    delete handler;
    return NULL;
  }
  return handler;
}

YallocBufferHandler::YallocBufferHandler(uint32_t fd) : fd_(fd), device_(0) {
}

YallocBufferHandler::~YallocBufferHandler() {
  if (device_)
    yalloc_close(device_);
}

bool YallocBufferHandler::Init() {
  VendorModule *module =
      (VendorModule *)LOAD_VENDOR_MODULE(YALLOC_VENDOR_MODULE_ID);
  if (module == NULL) {
    ETRACE("LOAD_VENDOR_MODULE Failed\n");
    return false;
  }

  yalloc_open(module, &device_);
  return true;
}

bool YallocBufferHandler::CreateBuffer(uint32_t w, uint32_t h, int format,
                                       HWCNativeHandle *handle,
                                       uint32_t layer_type, bool *modifier_used,
                                       int64_t /*preferred_modifier*/,
                                       bool /*raw_pixel_buffer*/) const {
  // LOG_I("YallocBufferHandler::CreateBuffer --> enter.\n");
  // LOG_I("\tw: %d, h: %d, format: %x.\n", w, h, format);
  // FIXME:: Take Modifiers into use.
  if (modifier_used)
    *modifier_used = false;

  struct yalloc_handle *temp = new struct yalloc_handle();

  uint32_t pixel_format = 0;
  if (format != 0) {
    pixel_format = DrmFormatToHALFormat(format);
  }

  if (pixel_format == 0) {
    pixel_format = YUN_HAL_FORMAT_RGBA_8888;
  }

  bool force_normal_usage = false;
  if ((layer_type == hwcomposer::kLayerVideo) &&
      !IsSupportedMediaFormat(format)) {
    ETRACE("Forcing normal usage for Video Layer. \n");
    force_normal_usage = true;
  }

  uint32_t usage = 0;
  if ((layer_type == hwcomposer::kLayerNormal) || force_normal_usage) {
    usage |= YALLOC_FLAG_HW_COMPOSER | YALLOC_FLAG_HW_RENDER |
             YALLOC_FLAG_HW_TEXTURE;
  } else if (layer_type == hwcomposer::kLayerVideo) {
    switch (pixel_format) {
      case YUN_HAL_FORMAT_YCbCr_422_I:
      case YUN_HAL_FORMAT_Y8:
        usage |= YALLOC_FLAG_HW_TEXTURE | YALLOC_FLAG_HW_VIDEO_DECODER;
        break;
      default:
        usage |= YALLOC_FLAG_HW_CAMERA_WRITE | YALLOC_FLAG_HW_CAMERA_READ |
                 YALLOC_FLAG_HW_VIDEO_ENCODER | YALLOC_FLAG_HW_TEXTURE;
    }
  } else if (layer_type == hwcomposer::kLayerCursor) {
    usage |= YALLOC_FLAG_CURSOR;
  }

  int stride;
  device_->alloc(device_, w, h, pixel_format, usage, &(temp->target_), &stride);

  temp->hwc_buffer_ = true;

  *handle = temp;

  return true;
}

bool YallocBufferHandler::ReleaseBuffer(HWCNativeHandle handle) const {
  if (handle->hwc_buffer_) {
    device_->free(device_, handle->target_);
  } else if (handle->imported_target_) {
    device_->unAuthorizeBuffer(device_, handle->imported_target_);
  }

  return true;
}

void YallocBufferHandler::DestroyHandle(HWCNativeHandle handle) const {
  DestroyBufferHandle(handle);
}

void YallocBufferHandler::CopyHandle(HWCNativeHandle source,
                                     HWCNativeHandle *target) const {
  CopyBufferHandle(source, target);
}

bool YallocBufferHandler::ImportBuffer(HWCNativeHandle handle) const {
  if (!handle->imported_target_) {
    ETRACE("could not find yalloc drm target");
    return false;
  }

  device_->authorizeBuffer(device_, handle->imported_target_);

  return ImportGraphicsBuffer(handle, fd_);
}

uint32_t YallocBufferHandler::GetTotalPlanes(HWCNativeHandle handle) const {
  return handle->meta_data_.num_planes_;
}

void *YallocBufferHandler::Map(HWCNativeHandle handle, uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               uint32_t *stride, void **map_data,
                               size_t plane) const {
  if (!handle->imported_target_) {
    ETRACE("could not find yalloc drm handle");
    return NULL;
  }

  int error =
      device_->map(device_, handle->imported_target_,
                   YALLOC_FLAG_SW_READ_OFTEN | YALLOC_FLAG_SW_WRITE_OFTEN, x, y,
                   width, height, map_data);

  return error ? NULL : *map_data;
}

int32_t YallocBufferHandler::UnMap(HWCNativeHandle handle,
                                   void *map_data) const {
  if (!handle->imported_target_) {
    ETRACE("could not find gralloc drm handle");
    return -1;
  }

  return device_->unmap(device_, handle->imported_target_);
}

uint32_t YallocBufferHandler::GetFd() const {
  return fd_;
}

}  // namespace hwcomposer
