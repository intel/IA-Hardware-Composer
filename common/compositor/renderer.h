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

#ifndef COMMON_COMPOSITOR_RENDERER_H_
#define COMMON_COMPOSITOR_RENDERER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace hwcomposer {

class NativeSurface;
struct RenderState;
struct MediaState;
struct media_import;

class Renderer {
 public:
  Renderer() = default;
  virtual ~Renderer() {
  }
  Renderer(const Renderer& rhs) = delete;
  Renderer& operator=(const Renderer& rhs) = delete;

  // Needs to be implemented for 3D Renderer's only.
  virtual bool Init() {
    return false;
  }

  virtual bool Draw(const std::vector<RenderState>& /*commands*/,
                    NativeSurface* /*surface*/) {
    return false;
  }

  // Needs to be implemented for Media Renderer's only.
  virtual bool Init(int /*gpu_fd*/) {
    return false;
  }

  virtual bool DestroyMediaResources(
      std::vector<struct media_import>& /*resources*/) {
    return true;
  }

  virtual bool Draw(const MediaState& /*state*/, NativeSurface* /*surface*/) {
    return false;
  }

  virtual void InsertFence(int32_t kms_fence) = 0;

  virtual void SetExplicitSyncSupport(bool disable_explicit_sync) = 0;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_RENDERER_H_
