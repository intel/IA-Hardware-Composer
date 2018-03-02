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

#ifndef COMMON_COMPOSITOR_GL_NATIVEGLRESOURCE_H_
#define COMMON_COMPOSITOR_GL_NATIVEGLRESOURCE_H_

#include <vector>

#include "nativegpuresource.h"
#include "dcshim.h"

namespace hwcomposer {

struct OverlayLayer;

class NativeGLResource : public NativeGpuResource {
 public:
  NativeGLResource() = default;
  ~NativeGLResource() override;

  bool PrepareResources(const std::vector<OverlayBuffer*>& buffers) override;
  GpuResourceHandle GetResourceHandle(uint32_t layer_index) const override;

  void ReleaseGPUResources(const std::vector<ResourceHandle>& handles) override;

  void HandleTextureUploads(
      const std::vector<OverlayBuffer*>& buffers) override;

 private:
  std::vector<GLuint> layer_textures_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_GL_NATIVEGLRESOURCE_H_
