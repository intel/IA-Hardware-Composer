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

#include <gbm.h>
#include "platformdefines.h"
#include "esUtil.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>

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
} glContext;

class LayerRenderer {
 public:
  LayerRenderer(struct gbm_device* gbm_dev);
  virtual ~LayerRenderer();

  virtual bool Init(uint32_t width, uint32_t height, uint32_t format,
                    glContext* gl = NULL, const char* resourePath = NULL) = 0;
  virtual void Draw(int64_t* pfence) = 0;
  struct gbm_handle* GetNativeBoHandle() {
    return &native_handle_;
  }

 protected:
  uint32_t planes_;
  struct gbm_bo* gbm_bo_;
  struct gbm_handle native_handle_;
  struct gbm_device* gbm_dev_;
  uint32_t format_;
  glContext* gl_;
};

#endif
