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

#include "shim.h"
#include "egloffscreencontext.h"

#include <assert.h>

namespace hwcomposer {

bool gl_is_supported() {
  EGLOffScreenContext context;

  if (!context.Init())
    return false;

  return true;
}

static bool initialized = false;

bool InitializeShims() {
  if (initialized)
    return true;

  #define get_proc(name, proc)                  \
  do {                                        \
    name = (proc)eglGetProcAddress(#name); \
    assert(name);                          \
  } while (0)

  get_proc(eglCreateImageKHR, PFNEGLCREATEIMAGEKHRPROC);
  get_proc(eglCreateSyncKHR, PFNEGLCREATESYNCKHRPROC);
  get_proc(eglDestroySyncKHR, PFNEGLDESTROYSYNCKHRPROC);
  get_proc(eglWaitSyncKHR, PFNEGLWAITSYNCKHRPROC);
  get_proc(eglDestroyImageKHR, PFNEGLDESTROYIMAGEKHRPROC);
  get_proc(glEGLImageTargetTexture2DOES, PFNGLEGLIMAGETARGETTEXTURE2DOESPROC);
  get_proc(glDeleteVertexArraysOES, PFNGLDELETEVERTEXARRAYSOESPROC);
  get_proc(glGenVertexArraysOES, PFNGLGENVERTEXARRAYSOESPROC);
  get_proc(glBindVertexArrayOES, PFNGLBINDVERTEXARRAYOESPROC);
#ifndef USE_ANDROID_SHIM
  get_proc(eglDupNativeFenceFDANDROID, PFNEGLDUPNATIVEFENCEFDANDROIDPROC);
#endif

  initialized = true;

  return true;
}

PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES;
PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES;
PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES;
#ifndef USE_ANDROID_SHIM
PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
#endif
}
