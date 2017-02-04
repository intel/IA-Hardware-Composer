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

#include "layerrenderer.h"

LayerRenderer::LayerRenderer(struct gbm_device* dev) {
  gbm_dev_ = dev;
  gl_ = NULL;
}

LayerRenderer::~LayerRenderer() {
  if (gbm_bo_)
    gbm_bo_destroy(gbm_bo_);

  gbm_bo_ = NULL;
}

bool LayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                         glContext* gl, const char* resource_path) {
  gbm_bo_ = gbm_bo_create(gbm_dev_, width, height, format,
                          GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!gbm_bo_) {
    printf("LayerRenderer: failed to create gbm_bo\n");
    return false;
  }

  int gbm_bo_fd = gbm_bo_get_plane_fd(gbm_bo_, 0);
  if (gbm_bo_fd == -1) {
    printf("LayerRenderer: gbm_bo_get_fd() failed\n");
    return false;
  }

  size_t total_planes = gbm_bo_get_num_planes(gbm_bo_);
  for (size_t i = 0; i < total_planes; i++) {
    native_handle_.import_data.offsets[i] = gbm_bo_get_plane_offset(gbm_bo_, i);
    native_handle_.import_data.strides[i] = gbm_bo_get_plane_stride(gbm_bo_, i);
    native_handle_.import_data.fds[i] = gbm_bo_fd;
  }

  native_handle_.import_data.width = width;
  native_handle_.import_data.height = height;
  native_handle_.import_data.format = gbm_bo_get_format(gbm_bo_);
  format_ = format;

  if (gl)
    gl_ = gl;
  return true;
}
