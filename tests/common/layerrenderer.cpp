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

#include <nativebufferhandler.h>

LayerRenderer::LayerRenderer(hwcomposer::NativeBufferHandler* buffer_handler) {
  buffer_handler_ = buffer_handler;
}

LayerRenderer::~LayerRenderer() {
  if (buffer_handler_ && handle_) {
    buffer_handler_->ReleaseBuffer(handle_);
    buffer_handler_->DestroyHandle(handle_);
  }
}

bool LayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                         uint32_t usage_format, uint32_t usage, glContext* gl,
                         const char* resource_path) {
  if (!buffer_handler_->CreateBuffer(width, height, format, &handle_, usage)) {
    ETRACE("LayerRenderer: CreateBuffer failed");
    return false;
  }

  buffer_handler_->CopyHandle(handle_, &handle_);

  if (!buffer_handler_->ImportBuffer(handle_, &bo_)) {
    ETRACE("LayerRenderer: ImportBuffer failed");
    return false;
  }

  width_ = bo_.width;
  height_ = bo_.height;
  stride_ = bo_.pitches[0];
  fd_ = bo_.prime_fd;
  planes_ = buffer_handler_->GetTotalPlanes(handle_);
  format_ = format;

  return true;
}
