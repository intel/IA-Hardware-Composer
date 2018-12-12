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

#ifndef COMMON_COMPOSITOR_GL_GLPROGRAM_H_
#define COMMON_COMPOSITOR_GL_GLPROGRAM_H_

#include <vector>

#include "shim.h"

namespace hwcomposer {

struct RenderState;

class GLProgram {
 public:
  GLProgram();
  GLProgram(const GLProgram& rhs) = delete;
  GLProgram& operator=(const GLProgram& rhs) = delete;

  ~GLProgram();

  bool Init(unsigned texture_count);
  void UseProgram(const RenderState& cmd, GLuint viewport_width,
                  GLuint viewport_height);

 private:
  GLint program_;
  GLint viewport_loc_;
  GLint crop_loc_;
  GLint alpha_loc_;
  GLint premult_loc_;
  GLint tex_matrix_loc_;
  GLint solid_color_loc_;
  bool initialized_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_GL_GLPROGRAM_H_
