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

#ifndef RENDERER_H_
#define RENDERER_H_

#include <vector>

namespace hwcomposer {

class NativeSurface;
struct RenderState;

class Renderer {
 public:
  Renderer() = default;
  virtual ~Renderer() {
  }
  Renderer(const Renderer& rhs) = delete;
  Renderer& operator=(const Renderer& rhs) = delete;

  virtual bool Init() = 0;
  virtual void Draw(const std::vector<RenderState>& commands,
                    NativeSurface* surface) = 0;

  virtual void RestoreState() = 0;

  virtual bool MakeCurrent() = 0;
};

}  // namespace hwcomposer
#endif  // RENDERER_H_
