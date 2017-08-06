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

#ifndef COMMON_COMPOSITOR_GL_GLRENDERER_H_
#define COMMON_COMPOSITOR_GL_GLRENDERER_H_

#include <memory>
#include <vector>

#include "renderer.h"

#include "egloffscreencontext.h"
#include "glprogram.h"

namespace hwcomposer {

class GLRenderer : public Renderer {
 public:
  GLRenderer() = default;
  ~GLRenderer();

  bool Init() override;
  bool Draw(const std::vector<RenderState> &commands, NativeSurface *surface,
            bool clear_surface) override;

  void InsertFence(int32_t kms_fence) override;

  void RestoreState() override;

  bool MakeCurrent() override;

  void SetExplicitSyncSupport(bool disable_explicit_sync) override;

 private:
  GLProgram *GetProgram(unsigned texture_count);

  EGLOffScreenContext context_;

  std::vector<std::unique_ptr<GLProgram>> programs_;
  GLuint vertex_array_ = 0;
  bool disable_explicit_sync_ = false;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_GL_GLRENDERER_H_
