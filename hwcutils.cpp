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

int DrmHwcNativeHandle::CopyBufferHandle(buffer_handle_t handle) {
  native_handle_t *handle_copy;
  GraphicBufferMapper &gm(GraphicBufferMapper::get());
  int ret =
      gm.importBuffer(handle, const_cast<buffer_handle_t *>(&handle_copy));
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

  ret = handle.CopyBufferHandle(sf_handle);
  if (ret)
    return ret;

  gralloc_buffer_usage = buffer.operator->()->usage;

  return 0;
}

void DrmHwcLayer::SetSourceCrop(hwc_frect_t const &crop) {
  source_crop = DrmHwcRect<float>(crop.left, crop.top, crop.right, crop.bottom);
}

void DrmHwcLayer::SetDisplayFrame(hwc_rect_t const &frame) {
  display_frame =
      DrmHwcRect<int>(frame.left, frame.top, frame.right, frame.bottom);
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
}
