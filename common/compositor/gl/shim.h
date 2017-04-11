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

#ifndef COMMON_COMPOSITOR_GL_SHIM_H_
#define COMMON_COMPOSITOR_GL_SHIM_H_

#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace hwcomposer {
bool gl_is_supported();

bool InitializeShims();

extern PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
extern PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
extern PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
extern PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES;
extern PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES;
extern PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES;
#ifndef USE_ANDROID_SHIM
extern PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
#endif
}  // namespace hwcomposer

#endif  //  COMMON_COMPOSITOR_GL_SHIM_H_
