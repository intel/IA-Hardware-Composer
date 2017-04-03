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

#include "glrenderer.h"
#include "glsurface.h"
#include "nativeglresource.h"

#include "nativevkresource.h"
#include "vkrenderer.h"
#include "vksurface.h"

namespace hwcomposer {

static NativeSurface* create_gl_surface(uint32_t width, uint32_t height) {
  return new GLSurface(width, height);
}

static Renderer* create_gl_renderer() {
  return new GLRenderer();
}

static NativeGpuResource* create_gl_resource() {
  return new NativeGLResource();
}

static NativeSurface* create_vk_surface(uint32_t width, uint32_t height) {
  return new VKSurface(width, height);
}

static Renderer* create_vk_renderer() {
  return new VKRenderer();
}

static NativeGpuResource* create_vk_resource() {
  return new NativeVKResource();
}

struct backend {
  char const *id;
  NativeSurface* (*create_surface)(uint32_t width, uint32_t height);
  Renderer* (*create_renderer)(void);
  NativeGpuResource* (*create_resource)(void);
};

static struct backend backends[] = {
#ifdef USE_VK
  {"VK", create_vk_surface, create_vk_renderer, create_vk_resource},
  {"GL", create_gl_surface, create_gl_renderer, create_gl_resource},
#else
  {"GL", create_gl_surface, create_gl_renderer, create_gl_resource},
  {"VK", create_vk_surface, create_vk_renderer, create_vk_resource},
#endif
  {}
};

static struct backend *active_backend = NULL;

NativeSurface* CreateBackBuffer(uint32_t width, uint32_t height) {
  if (active_backend) {
    return active_backend->create_surface(width, height);
  }

  for (struct backend *be = backends; be->id; be++) {
    NativeSurface *surf = be->create_surface(width, height);
    if (surf) {
      ETRACE("Selecting %s backend\n", be->id);
      active_backend = be;
      return surf;
    }
    ETRACE("Failed to create surface for %s backend\n", be->id);
  }

  ETRACE("Failed to create a NativeSuface\n");
  return NULL;
}

Renderer* CreateRenderer() {
  if (active_backend) {
    return active_backend->create_renderer();
  }

  for (struct backend *be = backends; be->id; be++) {
    Renderer *renderer = be->create_renderer();
    if (renderer) {
      ETRACE("Selecting %s backend\n", be->id);
      active_backend = be;
      return renderer;
    }
    ETRACE("Failed to create Renderer for %s backend\n", be->id);
  }

  ETRACE("Failed to create a Renderer\n");
  return NULL;
}

NativeGpuResource* CreateNativeGpuResourceHandler() {
  if (active_backend) {
    return active_backend->create_resource();
  }

  for (struct backend *be = backends; be->id; be++) {
    NativeGpuResource *resource = be->create_resource();
    if (resource) {
      ETRACE("Selecting %s backend\n", be->id);
      active_backend = be;
      return resource;
    }
    ETRACE("Failed to create NativeGpuResource for %s backend\n", be->id);
  }

  ETRACE("Failed to create a NativeGpuResource\n");
  return NULL;
}

}  // namespace hwcomposer
