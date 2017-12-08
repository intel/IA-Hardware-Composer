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

#include "nativeglresource.h"

#include "hwctrace.h"
#include "overlaylayer.h"
#include "shim.h"

namespace hwcomposer {

bool NativeGLResource::PrepareResources(
    const std::vector<OverlayBuffer*>& buffers) {
  Reset();
  std::vector<GLuint>().swap(layer_textures_);
  layer_textures_.reserve(buffers.size());
  EGLDisplay egl_display = eglGetCurrentDisplay();
  for (auto& buffer : buffers) {
    // Create EGLImage.
    EGLImageKHR egl_image = buffer->ImportImage(egl_display);

    if (egl_image == EGL_NO_IMAGE_KHR) {
      ETRACE("Failed to make import image.");
      return false;
    }
    GLuint texture = (GLuint)buffer->GetImageTexture();
    layer_textures_.emplace_back(texture);
  }

  return true;
}

NativeGLResource::~NativeGLResource() {
  Reset();
}

void NativeGLResource::ReleaseGPUResources() {
  Reset();
}

void NativeGLResource::Reset() {
}

GpuResourceHandle NativeGLResource::GetResourceHandle(
    uint32_t layer_index) const {
  if (layer_textures_.size() < layer_index)
    return 0;

  return layer_textures_.at(layer_index);
}

}  // namespace hwcomposer
