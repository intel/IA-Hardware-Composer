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

#include "imagelayerrenderer.h"

ImageLayerRenderer::ImageLayerRenderer(struct gbm_device* gdb_dev)
    : LayerRenderer(gdb_dev) {
}

ImageLayerRenderer::~ImageLayerRenderer() {
}

bool ImageLayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                              glContext* gl, const char* resource_path) {
  return true;
}

void ImageLayerRenderer::Draw(int64_t* pfence) {
  *pfence = -1;
}
