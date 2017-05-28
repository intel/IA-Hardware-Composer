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

#include "egloffscreencontext.h"

#include "hwctrace.h"

namespace hwcomposer {

EGLOffScreenContext::EGLOffScreenContext()
    : egl_display_(EGL_NO_DISPLAY),
      egl_ctx_(EGL_NO_CONTEXT),
      restore_context_(false) {
}

EGLOffScreenContext::~EGLOffScreenContext() {
  if (egl_display_ != EGL_NO_DISPLAY && egl_ctx_ != EGL_NO_CONTEXT)
    if (eglDestroyContext(egl_display_, egl_ctx_) == EGL_FALSE)
      ETRACE("Failed to destroy OpenGL ES Context.");
}

bool EGLOffScreenContext::Init() {
  EGLint num_configs;
  EGLConfig egl_config;
  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                           EGL_NONE};

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                          EGL_NONE};

  egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl_display_ == EGL_NO_DISPLAY) {
    ETRACE("Failed to get egl display");
    return false;
  }

  if (!eglInitialize(egl_display_, NULL, NULL)) {
    ETRACE("Egl Initialization failed.");
    return false;
  }

  if (!eglChooseConfig(egl_display_, config_attribs, &egl_config, 1,
                       &num_configs)) {
    ETRACE("Failed to choose a valid EGLConfig.");
    return false;
  }

  egl_ctx_ = eglCreateContext(egl_display_, egl_config, EGL_NO_CONTEXT,
                              context_attribs);

  if (egl_ctx_ == EGL_NO_CONTEXT) {
    ETRACE("Failed to create EGL Context.");
    return false;
  }

  return true;
}

bool EGLOffScreenContext::MakeCurrent() {
  saved_egl_display_ = eglGetCurrentDisplay();
  saved_egl_ctx_ = eglGetCurrentContext();
  saved_egl_read_ = eglGetCurrentSurface(EGL_READ);
  saved_egl_draw_ = eglGetCurrentSurface(EGL_DRAW);
  restore_context_ = false;

  if (saved_egl_display_ != egl_display_ || saved_egl_ctx_ != egl_ctx_ ||
      saved_egl_read_ != EGL_NO_SURFACE || saved_egl_draw_ != EGL_NO_SURFACE) {
    if (eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       egl_ctx_)) {
      restore_context_ = true;
    } else {
      ETRACE("failed to make context current");
      return false;
    }
  }

  return true;
}

void EGLOffScreenContext::RestoreState() {
  if (!restore_context_)
    return;

  eglMakeCurrent(saved_egl_display_, saved_egl_read_, saved_egl_draw_,
                 saved_egl_ctx_);
}

EGLint EGLOffScreenContext::GetSyncFD() {
  EGLint sync_fd = -1;
  EGLSyncKHR egl_sync =
      eglCreateSyncKHR(egl_display_, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
  if (egl_sync == EGL_NO_SYNC_KHR) {
    ETRACE("Failed to make sync object.");
    return -1;
  }

  sync_fd = eglDupNativeFenceFDANDROID(egl_display_, egl_sync);
  if (sync_fd == EGL_NO_NATIVE_FENCE_FD_ANDROID) {
    ETRACE("Failed to duplicate native fence object.");
    sync_fd = -1;
  }

  eglDestroySyncKHR(egl_display_, egl_sync);

  return sync_fd;
}

}  // namespace hwcomposer
