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

#include "factory.h"
#include "platformdefines.h"

#ifdef USE_GL
#include "glsurface.h"
#include "glrenderer.h"
#include "nativeglresource.h"
#endif

namespace hwcomposer {

NativeSurface* CreateBackBuffer(uint32_t width, uint32_t height) {
#ifdef USE_GL
  return new GLSurface(width, height);
#else
  return NULL;
#endif
}

Renderer* CreateRenderer() {
#ifdef USE_GL
  return new GLRenderer();
#else
  return NULL;
#endif
}

NativeGpuResource* CreateNativeGpuResourceHandler() {
#ifdef USE_GL
  return new NativeGLResource();
#else
  return NULL;
#endif
}

}  // namespace hwcomposer
