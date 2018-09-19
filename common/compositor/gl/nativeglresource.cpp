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
  std::vector<GLuint>().swap(layer_textures_);
  layer_textures_.reserve(buffers.size());
  EGLDisplay egl_display = eglGetCurrentDisplay();
  for (auto& buffer : buffers) {
    // Create EGLImage.
    if (buffer) {
      const ResourceHandle& import_image =
          buffer->GetGpuResource(egl_display, true);

      if (import_image.image_ == EGL_NO_IMAGE_KHR) {
        ETRACE("Failed to make import image.");
        return false;
      }

      layer_textures_.emplace_back(import_image.texture_);
    } else
      layer_textures_.emplace_back(0);
  }

  return true;
}

NativeGLResource::~NativeGLResource() {
}

void NativeGLResource::ReleaseGPUResources(
    const std::vector<ResourceHandle>& handles) {
  size_t purged_size = handles.size();
  EGLDisplay egl_display = eglGetCurrentDisplay();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  std::vector<GLuint> textures;
  std::vector<GLuint> fbs;

  for (size_t i = 0; i < purged_size; i++) {
    const ResourceHandle& handle = handles.at(i);
    if (handle.image_) {
      eglDestroyImageKHR(egl_display, handle.image_);
    }

    if (handle.texture_) {
      textures.emplace_back(handle.texture_);
    }

    if (handle.fb_) {
      fbs.emplace_back(handle.fb_);
    }
  }

  uint32_t textures_size = textures.size();
  if (textures_size > 0) {
    glDeleteTextures(textures_size, textures.data());
  }

  uint32_t fb_size = fbs.size();

  if (fb_size > 0) {
    glDeleteFramebuffers(fb_size, fbs.data());
  }
}

GpuResourceHandle NativeGLResource::GetResourceHandle(
    uint32_t layer_index) const {
  if (layer_textures_.size() < layer_index)
    return 0;

  return layer_textures_.at(layer_index);
}

}  // namespace hwcomposer
