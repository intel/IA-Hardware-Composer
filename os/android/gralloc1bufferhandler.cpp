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

#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <ui/GraphicBuffer.h>

#include <hwcdefs.h>
#include <hwctrace.h>

#include "commondrmutils.h"
#include "hwcutils.h"
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

Gralloc1BufferHandler::Gralloc1BufferHandler(uint32_t fd)
    : fd_(fd),
      gralloc_(nullptr),
      device_(nullptr),
      register_(nullptr),
      release_(nullptr),
      dimensions_(nullptr),
      lock_(nullptr),
      unlock_(nullptr),
      create_descriptor_(nullptr),
      destroy_descriptor_(nullptr),
      set_consumer_usage_(nullptr),
      set_dimensions_(nullptr),
      set_format_(nullptr),
      set_producer_usage_(nullptr),
      allocate_(nullptr),
      set_modifier_(nullptr) {
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

  create_descriptor_ = reinterpret_cast<GRALLOC1_PFN_CREATE_DESCRIPTOR>(
      gralloc1_dvc->getFunction(gralloc1_dvc,
                                GRALLOC1_FUNCTION_CREATE_DESCRIPTOR));
  destroy_descriptor_ = reinterpret_cast<GRALLOC1_PFN_DESTROY_DESCRIPTOR>(
      gralloc1_dvc->getFunction(gralloc1_dvc,
                                GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR));

  set_consumer_usage_ = reinterpret_cast<GRALLOC1_PFN_SET_CONSUMER_USAGE>(
      gralloc1_dvc->getFunction(gralloc1_dvc,
                                GRALLOC1_FUNCTION_SET_CONSUMER_USAGE));
  set_dimensions_ =
      reinterpret_cast<GRALLOC1_PFN_SET_DIMENSIONS>(gralloc1_dvc->getFunction(
          gralloc1_dvc, GRALLOC1_FUNCTION_SET_DIMENSIONS));
  set_format_ = reinterpret_cast<GRALLOC1_PFN_SET_FORMAT>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_SET_FORMAT));
  set_producer_usage_ = reinterpret_cast<GRALLOC1_PFN_SET_PRODUCER_USAGE>(
      gralloc1_dvc->getFunction(gralloc1_dvc,
                                GRALLOC1_FUNCTION_SET_PRODUCER_USAGE));
  allocate_ = reinterpret_cast<GRALLOC1_PFN_ALLOCATE>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_ALLOCATE));
#ifdef USE_GRALLOC1
  set_modifier_ = reinterpret_cast<GRALLOC1_PFN_SET_MODIFIER>(
      gralloc1_dvc->getFunction(gralloc1_dvc, GRALLOC1_FUNCTION_SET_MODIFIER));
#endif
  return true;
}

bool Gralloc1BufferHandler::CreateBuffer(uint32_t w, uint32_t h, int format,
                                         HWCNativeHandle *handle,
                                         uint32_t layer_type,
                                         bool *modifier_used,
                                         int64_t preferred_modifier,
                                         bool /*raw_pixel_buffer*/) const {
  struct gralloc_handle *temp = new struct gralloc_handle();
  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);

  create_descriptor_(gralloc1_dvc, &temp->gralloc1_buffer_descriptor_t_);
  uint32_t usage = 0;
  uint32_t pixel_format = 0;
  bool force_normal_usage = false;
  if (format != 0) {
    pixel_format = DrmFormatToHALFormat(format);
  }

  if (pixel_format == 0) {
    pixel_format = HAL_PIXEL_FORMAT_RGBA_8888;
  }

  set_format_(gralloc1_dvc, temp->gralloc1_buffer_descriptor_t_, pixel_format);
#ifdef ENABLE_RBC
  if (set_modifier_) {
    uint64_t modifier = 0;
    if (preferred_modifier != -1) {
      modifier = preferred_modifier;
    } else {
      modifier = choose_drm_modifier(format);
    }

    set_modifier_(gralloc1_dvc, temp->gralloc1_buffer_descriptor_t_, modifier);
  }

  if (modifier_used) {
    *modifier_used = true;
  }
#else
  if (modifier_used) {
    *modifier_used = false;
  }
#endif

  if ((layer_type == hwcomposer::kLayerVideo) &&
      !IsSupportedMediaFormat(format)) {
    ETRACE("Forcing normal usage for Video Layer. \n");
    force_normal_usage = true;
  }

  if ((layer_type == hwcomposer::kLayerNormal) || force_normal_usage) {
    usage |= GRALLOC1_CONSUMER_USAGE_HWCOMPOSER |
             GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET |
             GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE;
  } else if (layer_type == hwcomposer::kLayerVideo) {
    switch (pixel_format) {
      case HAL_PIXEL_FORMAT_YCbCr_422_I:
      case HAL_PIXEL_FORMAT_Y8:
        usage |= GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE |
                 GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER;
        break;
      default:
        usage |= GRALLOC1_PRODUCER_USAGE_CAMERA |
                 GRALLOC1_CONSUMER_USAGE_CAMERA |
                 GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER |
                 GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE;
    }
  } else if (layer_type == hwcomposer::kLayerCursor) {
    usage |= GRALLOC1_CONSUMER_USAGE_CURSOR;
  }

  set_consumer_usage_(gralloc1_dvc, temp->gralloc1_buffer_descriptor_t_, usage);
  set_producer_usage_(gralloc1_dvc, temp->gralloc1_buffer_descriptor_t_, usage);
  set_dimensions_(gralloc1_dvc, temp->gralloc1_buffer_descriptor_t_, w, h);
  allocate_(gralloc1_dvc, 1, &temp->gralloc1_buffer_descriptor_t_,
            &temp->handle_);

  if (!temp->handle_) {
    ETRACE("Failed to allocate buffer \n");
  }

  temp->hwc_buffer_ = true;
  *handle = temp;

  return true;
}

bool Gralloc1BufferHandler::ReleaseBuffer(HWCNativeHandle handle) const {
  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);

  if (handle->hwc_buffer_) {
    release_(gralloc1_dvc, handle->handle_);
  } else if (handle->imported_handle_) {
    release_(gralloc1_dvc, handle->imported_handle_);
  }

  if (handle->gralloc1_buffer_descriptor_t_ > 0)
    destroy_descriptor_(gralloc1_dvc, handle->gralloc1_buffer_descriptor_t_);

  return true;
}

void Gralloc1BufferHandler::DestroyHandle(HWCNativeHandle handle) const {
  DestroyBufferHandle(handle);
}

bool Gralloc1BufferHandler::ImportBuffer(HWCNativeHandle handle) const {
  if (!handle->imported_handle_) {
    ETRACE("could not find gralloc drm handle");
    return false;
  }

  gralloc1_device_t *gralloc1_dvc =
      reinterpret_cast<gralloc1_device_t *>(device_);
  register_(gralloc1_dvc, handle->imported_handle_);
  if (!ImportGraphicsBuffer(handle, fd_)) {
    return false;
  }

  return true;
}

uint32_t Gralloc1BufferHandler::GetTotalPlanes(HWCNativeHandle handle) const {
  return handle->meta_data_.num_planes_;
}

void Gralloc1BufferHandler::CopyHandle(HWCNativeHandle source,
                                       HWCNativeHandle *target) const {
  CopyBufferHandle(source, target);
}

void *Gralloc1BufferHandler::Map(HWCNativeHandle handle, uint32_t x, uint32_t y,
                                 uint32_t width, uint32_t height,
                                 uint32_t * /*stride*/, void **map_data,
                                 size_t /*plane*/) const {
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
  uint32_t status = lock_(gralloc1_dvc, handle->imported_handle_,
                          GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN,
                          GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN, &rect,
                          map_data, acquireFence);
  return (GRALLOC1_ERROR_NONE == status) ? *map_data : NULL;
}

int32_t Gralloc1BufferHandler::UnMap(HWCNativeHandle handle,
                                     void * /*map_data*/) const {
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
