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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-utils"

#include "drmhwcomposer.h"
#include "platform.h"

#include <log/log.h>
#include <ui/GraphicBufferMapper.h>

#define UNUSED(x) (void)(x)

namespace android {

const hwc_drm_bo *DrmHwcBuffer::operator->() const {
  if (importer_ == NULL) {
    ALOGE("Access of non-existent BO");
    exit(1);
    return NULL;
  }
  return &bo_;
}

void DrmHwcBuffer::Clear() {
  if (importer_ != NULL) {
    importer_->ReleaseBuffer(&bo_);
    importer_ = NULL;
  }
}

int DrmHwcBuffer::ImportBuffer(buffer_handle_t handle, Importer *importer) {
  hwc_drm_bo tmp_bo;

  int ret = importer->ImportBuffer(handle, &tmp_bo);
  if (ret)
    return ret;

  if (importer_ != NULL) {
    importer_->ReleaseBuffer(&bo_);
  }

  importer_ = importer;

  bo_ = tmp_bo;

  return 0;
}

int DrmHwcNativeHandle::CopyBufferHandle(buffer_handle_t handle, int width,
                                         int height, int layerCount, int format,
                                         int usage, int stride) {
  native_handle_t *handle_copy;
  GraphicBufferMapper &gm(GraphicBufferMapper::get());
  int ret;

#ifdef HWC2_USE_OLD_GB_IMPORT
  UNUSED(width);
  UNUSED(height);
  UNUSED(layerCount);
  UNUSED(format);
  UNUSED(usage);
  UNUSED(stride);
  ret = gm.importBuffer(handle, const_cast<buffer_handle_t *>(&handle_copy));
#else
  ret = gm.importBuffer(handle, width, height, layerCount, format, usage,
                        stride, const_cast<buffer_handle_t *>(&handle_copy));
#endif
  if (ret) {
    ALOGE("Failed to import buffer handle %d", ret);
    return ret;
  }

  Clear();

  handle_ = handle_copy;

  return 0;
}

DrmHwcNativeHandle::~DrmHwcNativeHandle() {
  Clear();
}

void DrmHwcNativeHandle::Clear() {
  if (handle_ != NULL) {
    GraphicBufferMapper &gm(GraphicBufferMapper::get());
    int ret = gm.freeBuffer(handle_);
    if (ret) {
      ALOGE("Failed to free buffer handle %d", ret);
    }
    handle_ = NULL;
  }
}

int DrmHwcLayer::ImportBuffer(Importer *importer) {
  int ret = buffer.ImportBuffer(sf_handle, importer);
  if (ret)
    return ret;

  const hwc_drm_bo *bo = buffer.operator->();

  unsigned int layer_count;
  for (layer_count = 0; layer_count < HWC_DRM_BO_MAX_PLANES; ++layer_count)
    if (bo->gem_handles[layer_count] == 0)
      break;

  ret = handle.CopyBufferHandle(sf_handle, bo->width, bo->height, layer_count,
                                bo->hal_format, bo->usage, bo->pixel_stride);
  if (ret)
    return ret;

  gralloc_buffer_usage = bo->usage;

  return 0;
}

int DrmHwcLayer::InitFromDrmHwcLayer(DrmHwcLayer *src_layer,
                                     Importer *importer) {
  blending = src_layer->blending;
  sf_handle = src_layer->sf_handle;
  acquire_fence = -1;
  display_frame = src_layer->display_frame;
  alpha = src_layer->alpha;
  source_crop = src_layer->source_crop;
  transform = src_layer->transform;
  return ImportBuffer(importer);
}

void DrmHwcLayer::SetSourceCrop(hwc_frect_t const &crop) {
  source_crop = crop;
}

void DrmHwcLayer::SetDisplayFrame(hwc_rect_t const &frame) {
  display_frame = frame;
}

void DrmHwcLayer::SetTransform(int32_t sf_transform) {
  transform = 0;
  // 270* and 180* cannot be combined with flips. More specifically, they
  // already contain both horizontal and vertical flips, so those fields are
  // redundant in this case. 90* rotation can be combined with either horizontal
  // flip or vertical flip, so treat it differently
  if (sf_transform == HWC_TRANSFORM_ROT_270) {
    transform = DrmHwcTransform::kRotate270;
  } else if (sf_transform == HWC_TRANSFORM_ROT_180) {
    transform = DrmHwcTransform::kRotate180;
  } else {
    if (sf_transform & HWC_TRANSFORM_FLIP_H)
      transform |= DrmHwcTransform::kFlipH;
    if (sf_transform & HWC_TRANSFORM_FLIP_V)
      transform |= DrmHwcTransform::kFlipV;
    if (sf_transform & HWC_TRANSFORM_ROT_90)
      transform |= DrmHwcTransform::kRotate90;
  }
}
}  // namespace android
