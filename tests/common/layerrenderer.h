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

#ifndef LAYER_RENDERER_H_
#define LAYER_RENDERER_H_

#include <hwcbuffer.h>
#include <platformdefines.h>
#include "esUtil.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>

typedef struct {
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC
  glEGLImageTargetRenderbufferStorageOES;
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
  PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
  PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
  PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
  PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
  PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
} glContext;

namespace hwcomposer {
class NativeBufferHandler;
}

class LayerRenderer {
 public:
  LayerRenderer(hwcomposer::NativeBufferHandler* buffer_handler);
  virtual ~LayerRenderer();

  virtual bool Init(uint32_t width, uint32_t height, uint32_t format,
                    uint32_t usage_format = -1, uint32_t usage = 0,
                    glContext* gl = NULL, const char* resourePath = NULL) = 0;
  virtual void Draw(int64_t* pfence) = 0;
  HWCNativeHandle GetNativeBoHandle() {
    return handle_;
  }

 protected:
  HWCNativeHandle handle_;
  HwcBuffer bo_;
  hwcomposer::NativeBufferHandler* buffer_handler_ = NULL;
  uint32_t format_ = DRM_FORMAT_XRGB8888;
  uint32_t planes_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t stride_ = 0;
  uint32_t fd_ = 0;

#define MAX_MODIFICATORS 4
  uint32_t bufferusage_ = 0;

// This format fines the format befind format_
// format_ is for buffer allocation purpose
// this usage_format is here for
#define INVALID_USAGE_FORMAT -1
  uint32_t usage_format_ = INVALID_USAGE_FORMAT;
};

#endif
