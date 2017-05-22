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

#ifndef CC_LAYER_RENDERER_H_
#define CC_LAYER_RENDERER_H_

#include "platformdefines.h"
#include "layerrenderer.h"
#include <stdio.h>

class CCLayerRenderer : public LayerRenderer {
 public:
  CCLayerRenderer(hwcomposer::NativeBufferHandler* buffer_handler);
  ~CCLayerRenderer() override;

  bool Init(uint32_t width, uint32_t height, uint32_t format,
            glContext* gl = NULL, const char* resource_path = NULL) override;
  void Draw(int64_t* pfence) override;
};

#endif
