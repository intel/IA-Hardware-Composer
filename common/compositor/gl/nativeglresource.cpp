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
  std::vector<GpuResourceHandle>().swap(layer_textures_);
  EGLDisplay egl_display = eglGetCurrentDisplay();
  for (auto& buffer : buffers) {
    // Create EGLImage.
    EGLImageKHR egl_image = buffer->ImportImage(egl_display);

    if (egl_image == EGL_NO_IMAGE_KHR) {
      ETRACE("Failed to make import image.");
      return false;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES,
                                 (GLeglImageOES)egl_image);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    GpuResourceHandle resource;
    resource.gl = texture;
    layer_textures_.emplace_back(resource);
    eglDestroyImageKHR(egl_display, egl_image);
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
  GLuint texture_id = 0;
  size_t size = layer_textures_.size();
  for (size_t i = 0; i < size; i++) {
    texture_id = layer_textures_.at(i).gl;
    glDeleteTextures(1, &texture_id);
  }
}

GpuResourceHandle NativeGLResource::GetResourceHandle(
    uint32_t layer_index) const {
  if (layer_textures_.size() < layer_index) {
    GpuResourceHandle res;
    res.gl = 0;
    return res;
  }

  return layer_textures_.at(layer_index);
}

}  // namespace hwcomposer
