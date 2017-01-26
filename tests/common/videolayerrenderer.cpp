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

static uint32_t getPixelBits(uint32_t format) {
  switch (format) {
    case GBM_FORMAT_C8:
    case GBM_FORMAT_R8:
    case GBM_FORMAT_RGB332:
    case GBM_FORMAT_BGR233:
      return 8;
    case GBM_FORMAT_GR88:
    case GBM_FORMAT_XRGB4444:
    case GBM_FORMAT_XBGR4444:
    case GBM_FORMAT_RGBX4444:
    case GBM_FORMAT_BGRX4444:
    case GBM_FORMAT_ARGB4444:
    case GBM_FORMAT_ABGR4444:
    case GBM_FORMAT_RGBA4444:
    case GBM_FORMAT_BGRA4444:
    case GBM_FORMAT_XRGB1555:
    case GBM_FORMAT_XBGR1555:
    case GBM_FORMAT_RGBX5551:
    case GBM_FORMAT_BGRX5551:
    case GBM_FORMAT_ARGB1555:
    case GBM_FORMAT_ABGR1555:
    case GBM_FORMAT_RGBA5551:
    case GBM_FORMAT_BGRA5551:
    case GBM_FORMAT_BGR565:
    case GBM_FORMAT_YUYV:
    case GBM_FORMAT_YVYU:
    case GBM_FORMAT_UYVY:
    case GBM_FORMAT_VYUY:
    case GBM_FORMAT_AYUV:
    case GBM_FORMAT_YUV422:
    case GBM_FORMAT_YVU422:
      return 16;

    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
    case GBM_FORMAT_YUV444:
    case GBM_FORMAT_YVU444:
      return 24;

    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_RGBX8888:
    case GBM_FORMAT_BGRX8888:
    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGBA8888:
    case GBM_FORMAT_BGRA8888:
    case GBM_FORMAT_XRGB2101010:
    case GBM_FORMAT_XBGR2101010:
    case GBM_FORMAT_RGBX1010102:
    case GBM_FORMAT_BGRX1010102:
    case GBM_FORMAT_ARGB2101010:
    case GBM_FORMAT_ABGR2101010:
    case GBM_FORMAT_RGBA1010102:
    case GBM_FORMAT_BGRA1010102:
      return 32;

    case GBM_FORMAT_NV12:
    case GBM_FORMAT_NV21:
    case GBM_FORMAT_NV16:
    case GBM_FORMAT_NV61:
    case GBM_FORMAT_YUV410:
    case GBM_FORMAT_YVU410:
    case GBM_FORMAT_YUV411:
    case GBM_FORMAT_YVU411:
    case GBM_FORMAT_YUV420:
    case GBM_FORMAT_YVU420:
      return 12;
    defalut:
      return 0;
  }
}

static void getPlanesStrides(uint32_t format, uint32_t stride,
                             uint32_t* plane_size, uint32_t* strides) {
  switch (format) {
    case GBM_FORMAT_C8:
    case GBM_FORMAT_R8:
    case GBM_FORMAT_RGB332:
    case GBM_FORMAT_BGR233:
    case GBM_FORMAT_GR88:
    case GBM_FORMAT_XRGB4444:
    case GBM_FORMAT_XBGR4444:
    case GBM_FORMAT_RGBX4444:
    case GBM_FORMAT_BGRX4444:
    case GBM_FORMAT_ARGB4444:
    case GBM_FORMAT_ABGR4444:
    case GBM_FORMAT_RGBA4444:
    case GBM_FORMAT_BGRA4444:
    case GBM_FORMAT_XRGB1555:
    case GBM_FORMAT_XBGR1555:
    case GBM_FORMAT_RGBX5551:
    case GBM_FORMAT_BGRX5551:
    case GBM_FORMAT_ARGB1555:
    case GBM_FORMAT_ABGR1555:
    case GBM_FORMAT_RGBA5551:
    case GBM_FORMAT_BGRA5551:
    case GBM_FORMAT_BGR565:
    case GBM_FORMAT_YUYV:
    case GBM_FORMAT_YVYU:
    case GBM_FORMAT_UYVY:
    case GBM_FORMAT_VYUY:
    case GBM_FORMAT_AYUV:
    case GBM_FORMAT_YUV422:
    case GBM_FORMAT_YVU422:
    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
    case GBM_FORMAT_YUV444:
    case GBM_FORMAT_YVU444:
    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_RGBX8888:
    case GBM_FORMAT_BGRX8888:
    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGBA8888:
    case GBM_FORMAT_BGRA8888:
    case GBM_FORMAT_XRGB2101010:
    case GBM_FORMAT_XBGR2101010:
    case GBM_FORMAT_RGBX1010102:
    case GBM_FORMAT_BGRX1010102:
    case GBM_FORMAT_ARGB2101010:
    case GBM_FORMAT_ABGR2101010:
    case GBM_FORMAT_RGBA1010102:
    case GBM_FORMAT_BGRA1010102:
      *plane_size = 1;
      strides[0] = stride;
      break;

    case GBM_FORMAT_NV12:
    case GBM_FORMAT_NV21:
    case GBM_FORMAT_NV16:
    case GBM_FORMAT_NV61:
      *plane_size = 2;
      strides[0] = stride;
      strides[1] = stride / 2;
      break;

    case GBM_FORMAT_YUV410:
    case GBM_FORMAT_YVU410:
    case GBM_FORMAT_YUV411:
    case GBM_FORMAT_YVU411:
    case GBM_FORMAT_YUV420:
    case GBM_FORMAT_YVU420:
      *plane_size = 3;
      strides[0] = stride;
      strides[1] = strides[2] = stride / 4;
      break;
    defalut:
      break;
  }
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

  pixelbytes_ = getPixelBits(format);
  getPlanesStrides(format_, native_handle_.import_data.stride, &planes_,
                   strides_);
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
  uint32_t stride = native_handle_.import_data.stride;
  uint32_t mapStride;
  void* pBo = gbm_bo_map(gbm_bo_, 0, 0, width, height, GBM_BO_TRANSFER_WRITE,
                         &mapStride, &pOpaque);
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
