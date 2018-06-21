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

#ifdef USE_DC
#include "nativesurface.h"
#elif USE_GL
#include "glrenderer.h"
#include "glsurface.h"
#include "nativeglresource.h"
#elif USE_VK
#include "nativevkresource.h"
#include "vkrenderer.h"
#include "vksurface.h"
#endif

#ifndef DISABLE_VA
#include "va/varenderer.h"
#endif

namespace hwcomposer {

NativeSurface* Create3DBuffer(uint32_t width, uint32_t height) {
#ifdef USE_GL
  return new GLSurface(width, height);
#elif USE_VK
  return new VKSurface(width, height);
#else
  return NULL;
#endif
}

NativeSurface* CreateVideoBuffer(uint32_t width, uint32_t height) {
  return new NativeSurface(width, height);
}

Renderer* Create3DRenderer() {
#ifdef USE_GL
  return new GLRenderer();
#elif USE_VK
  return new VKRenderer();
#else
  return NULL;
#endif
}

Renderer* CreateMediaRenderer() {
#ifdef USE_DC
  return NULL;
#else
#ifndef DISABLE_VA
  return new VARenderer();
#else
  return NULL;
#endif
#endif
}

NativeGpuResource* CreateNativeGpuResourceHandler() {
#ifdef USE_GL
  return new NativeGLResource();
#elif USE_VK
  return new NativeVKResource();
#else
  return NULL;
#endif
}

}  // namespace hwcomposer
