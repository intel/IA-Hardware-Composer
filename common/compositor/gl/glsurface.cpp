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

#include "glsurface.h"

#include "hwctrace.h"
#include "nativebufferhandler.h"
#include "overlaybuffer.h"
#include "shim.h"

namespace hwcomposer {

GLSurface::GLSurface(uint32_t width, uint32_t height)
    : NativeSurface(width, height) {
}

GLSurface::~GLSurface() {
  if (fb_)
    glDeleteFramebuffers(1, &fb_);
  if (tex_)
    glDeleteTextures(1, &tex_);
}

bool GLSurface::InitializeGPUResources() {
  EGLDisplay egl_display = eglGetCurrentDisplay();
  // Create EGLImage.
  EGLImageKHR image = overlay_buffer_->ImportImage(egl_display);

  if (image == EGL_NO_IMAGE_KHR) {
    ETRACE("Failed to make EGL image.");
    return false;
  }

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
  glBindTexture(GL_TEXTURE_2D, 0);

  tex_ = texture;
  eglDestroyImageKHR(egl_display, image);

  // Create Fb.
  GLuint gl_fb;
  glGenFramebuffers(1, &gl_fb);
  glBindFramebuffer(GL_FRAMEBUFFER, gl_fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);

  fb_ = gl_fb;
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    ETRACE("GL Framebuffer is not complete.");
    return false;
  }

  return true;
}

void GLSurface::MakeCurrent() {
  glBindFramebuffer(GL_FRAMEBUFFER, fb_);
}

}  // namespace hwcomposer
