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

#include "videolayerrenderer.h"
#include <stdio.h>

VideoLayerRenderer::VideoLayerRenderer(struct gbm_device* gbm_dev)
    : LayerRenderer(gbm_dev) {
}

VideoLayerRenderer::~VideoLayerRenderer() {
  if (resource_fd_)
    fclose(resource_fd_);
  resource_fd_ = NULL;
}

bool VideoLayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                              glContext* gl, const char* resource_path) {
  if (!LayerRenderer::Init(width, height, format, gl, resource_path))
    return false;
  if (!resource_path) {
    printf("resource file no provided\n");
    return false;
  }

  resource_fd_ = fopen(resource_path, "r");
  if (!resource_fd_) {
    printf("Could not open the resource file\n");
    return false;
  }

  planes_ = gbm_bo_get_num_planes(gbm_bo_);
  for (size_t i = 0; i < planes_; i++) {
    native_handle_.import_data.offsets[i] = gbm_bo_get_plane_offset(gbm_bo_, i);
    native_handle_.import_data.strides[i] = gbm_bo_get_plane_stride(gbm_bo_, i);
  }
  isFileEnded_ = false;

  return true;
}

void VideoLayerRenderer::Draw(int64_t* pfence) {
  // *p_Draw();

  if (isFileEnded_)
    return;
  if (!resource_fd_)
    return;

  void* pOpaque = NULL;
  uint32_t width = native_handle_.import_data.width;
  uint32_t height = native_handle_.import_data.height;
  uint32_t stride = native_handle_.import_data.strides[0];
  uint32_t mapStride;
  void* pBo = gbm_bo_map(gbm_bo_, 0, 0, width, height, GBM_BO_TRANSFER_WRITE,
                         &mapStride, &pOpaque, 0);
  if (!pBo) {
    printf("gbm_bo_map is not successful!\n");
    return;
  }

  void* pReadLoc = pBo;
  for (int i = 0; i < planes_; i++) {
    uint32_t planeBlockSize = strides_[i] * height;
    uint32_t readCount = fread(pReadLoc, 1, planeBlockSize, resource_fd_);
    if (readCount != planeBlockSize) {
      isFileEnded_ = true;
      break;
    }
  }

  gbm_bo_unmap(gbm_bo_, pOpaque);
  *pfence = -1;
}
