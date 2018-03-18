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

#include "gllayerrenderer.h"
#include <assert.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

GLLayerRenderer::GLLayerRenderer(
    hwcomposer::NativeBufferHandler* buffer_handler)
    : LayerRenderer(buffer_handler) {
}

GLLayerRenderer::~GLLayerRenderer() {
  if (gl_)
    delete gl_;
  gl_ = NULL;
}

bool GLLayerRenderer::Init_GL(glContext* gl) {
  EGLint n;
  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                           EGL_NONE};

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                          EGL_NONE};

  gl_ = new glContext;
  gl_->display = gl->display;
  gl_->glEGLImageTargetRenderbufferStorageOES =
      gl->glEGLImageTargetRenderbufferStorageOES;
  gl_->eglCreateImageKHR = gl->eglCreateImageKHR;
  gl_->eglCreateSyncKHR = gl->eglCreateSyncKHR;
  gl_->eglDestroySyncKHR = gl->eglDestroySyncKHR;
  gl_->eglWaitSyncKHR = gl->eglWaitSyncKHR;
  gl_->eglClientWaitSyncKHR = gl->eglClientWaitSyncKHR;
  gl_->eglDupNativeFenceFDANDROID = gl->eglDupNativeFenceFDANDROID;
  gl_->glEGLImageTargetTexture2DOES = gl->glEGLImageTargetTexture2DOES;
  gl_->eglDestroyImageKHR = gl->eglDestroyImageKHR;

  if (!eglChooseConfig(gl_->display, config_attribs, &gl_->config, 1, &n) ||
      n != 1) {
    printf("failed to choose config: %d\n", n);
    return false;
  }
  gl_->context = eglCreateContext(gl_->display, gl_->config, EGL_NO_CONTEXT,
                                  context_attribs);

  if (gl_->context == NULL) {
    printf("failed to create context\n");
    return false;
  }
  return true;
}

bool GLLayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                           uint32_t usage_format, uint32_t usage, glContext* gl,
                           const char* resource_path) {
  if (format != DRM_FORMAT_XRGB8888)
    return false;
  if (!LayerRenderer::Init(width, height, format, usage_format, usage, gl))
    return false;

  if (!Init_GL(gl)) {
    ETRACE("Failed to create gl context for layer renderer");
  }

  eglMakeCurrent(gl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, gl_->context);

  if (!handle_->meta_data_.modifier) {
    const EGLint image_attrs[] = {
        EGL_WIDTH,                     (EGLint)width,
        EGL_HEIGHT,                    (EGLint)height,
        EGL_LINUX_DRM_FOURCC_EXT,      DRM_FORMAT_XRGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT,     (EGLint)fd_,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  (EGLint)stride_,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE,
    };
    egl_image_ = gl_->eglCreateImageKHR(gl_->display, EGL_NO_CONTEXT,
                                        EGL_LINUX_DMA_BUF_EXT,
                                        (EGLClientBuffer)NULL, image_attrs);
  } else {
    uint64_t modifier = handle_->meta_data_.modifier;
    EGLint modifier_low = (EGLint)modifier;
    EGLint modifier_high = (EGLint)(modifier >> 32);
    const EGLint image_attrs[] = {
        EGL_WIDTH,
        (EGLint)width,
        EGL_HEIGHT,
        (EGLint)height,
        EGL_LINUX_DRM_FOURCC_EXT,
        DRM_FORMAT_XRGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT,
        (EGLint)fd_,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        (EGLint)handle_->meta_data_.pitches_[0],
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        (EGLint)handle_->meta_data_.offsets_[0],
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
        modifier_low,
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
        modifier_high,
        EGL_DMA_BUF_PLANE1_FD_EXT,
        (EGLint)fd_,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,
        (EGLint)handle_->meta_data_.pitches_[1],
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        (EGLint)handle_->meta_data_.offsets_[1],
        EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        modifier_low,
        EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        modifier_high,
        EGL_NONE,
    };
    egl_image_ = gl_->eglCreateImageKHR(gl_->display, EGL_NO_CONTEXT,
                                        EGL_LINUX_DMA_BUF_EXT,
                                        (EGLClientBuffer)NULL, image_attrs);
  }

  if (!egl_image_) {
    printf("failed to create EGLImage from gbm_bo\n");
    return false;
  }

  glGenRenderbuffers(1, &gl_renderbuffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, gl_renderbuffer_);
  gl_->glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, egl_image_);
  if (glGetError() != GL_NO_ERROR) {
    printf("failed to create GL renderbuffer from EGLImage\n");
    return false;
  }

  glGenFramebuffers(1, &gl_framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, gl_renderbuffer_);

  if (glGetError() != GL_NO_ERROR) {
    printf("failed to create GL framebuffer\n");
    return false;
  }

  return true;
}

void GLLayerRenderer::Draw(int64_t* pfence) {
  eglMakeCurrent(gl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, gl_->context);

  glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer_);

  glDrawFrame();

  int64_t gpu_fence_fd = -1;
#ifndef DISABLE_EXPLICIT_SYNC
  EGLint attrib_list[] = {
      EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID,
      EGL_NONE,
  };
  EGLSyncKHR gpu_fence = gl_->eglCreateSyncKHR(
      gl_->display, EGL_SYNC_NATIVE_FENCE_ANDROID, attrib_list);
  assert(gpu_fence);

  gpu_fence_fd =
      gl_->eglDupNativeFenceFDANDROID(gl_->display, gpu_fence);
  gl_->eglDestroySyncKHR(gl_->display, gpu_fence);
  assert(gpu_fence_fd != -1);
#endif
  *pfence = gpu_fence_fd;
}
