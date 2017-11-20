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

#include "cclayerrenderer.h"
#include <nativebufferhandler.h>
#include <stdio.h>
#include <drm_fourcc.h>

CCLayerRenderer::CCLayerRenderer(
    hwcomposer::NativeBufferHandler* buffer_handler)
    : LayerRenderer(buffer_handler) {
}

CCLayerRenderer::~CCLayerRenderer() {
}

bool CCLayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                           uint32_t usage_format, uint32_t usage, glContext* gl,
                           const char* resource_path) {
  if (format != DRM_FORMAT_XRGB8888)
    return false;

  if (!LayerRenderer::Init(width, height, format, usage_format, usage, gl,
                           resource_path))
    return false;

  return true;
}

void CCLayerRenderer::Draw(int64_t* pfence) {
  void* pOpaque = NULL;
  uint32_t stride = handle_->meta_data_.pitches_[0];
  uint32_t mapStride;
  void* pBo = buffer_handler_->Map(handle_, 0, 0, width_, height_, &mapStride,
                                   &pOpaque, 0);
  if (!pBo) {
    ETRACE("gbm_bo_map is not successful!");
    return;
  }

  uint32_t c_offiset = 4;
  uint32_t c_value = 255;

  memset(pBo, 0, height_ * stride);
  int color_height = height_ / 3;

  if (color_height <= 0)
    return;

  for (int i = 0; i < height_; i++) {
    if (i < color_height) {
      c_offiset = 0;  // blue
    } else if (i >= color_height && i < color_height * 2) {
      c_offiset = 1;  // green
    } else {
      c_offiset = 2;  // red
    }
    c_value = 255 * ((float)(i % color_height) / color_height);

    if (c_value > 255) {
      c_value = 255;
    } else if (c_value <= 0) {
      c_value = 1;
    }

    for (int j = c_offiset; j < stride; j = j + 4) {
      ((char*)pBo)[i * stride + j] = c_value;
    }
  }

  buffer_handler_->UnMap(handle_, pOpaque);
  *pfence = -1;
}
