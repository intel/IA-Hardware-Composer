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

#ifndef GL_CUBE_LAYER_RENDERER_H_
#define GL_CUBE_LAYER_RENDERER_H_

#include "gllayerrenderer.h"

class GLCubeLayerRenderer : public GLLayerRenderer {
 public:
  GLCubeLayerRenderer(struct gbm_device* gbm_dev);
  ~GLCubeLayerRenderer() override;

  bool Init(uint32_t width, uint32_t height, uint32_t format,
            glContext* gl = NULL, const char* resource_path = NULL) override;
  void glDrawFrame() override;

 private:
  GLuint program_;
  GLint modelviewmatrix_, modelviewprojectionmatrix_, normalmatrix_;
  GLuint vbo_;
  GLuint positionsoffset_, colorsoffset_, normalsoffset_;
  uint32_t frame_count_ = 0;
};

#endif
