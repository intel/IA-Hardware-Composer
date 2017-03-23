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

#ifndef SCOPED_RENDERER_STATE_H_
#define SCOPED_RENDERER_STATE_H_

namespace hwcomposer {

class Renderer;

struct ScopedRendererState {
  ScopedRendererState(Renderer* renderer);

  ScopedRendererState(const ScopedRendererState& rhs) = delete;

  ~ScopedRendererState();

  ScopedRendererState& operator=(const ScopedRendererState& rhs) = delete;

  bool IsValid() const {
    return is_valid_;
  }

 private:
  Renderer* renderer_;
  bool is_valid_;
};

}  // namespace hwcomposer
#endif  // SCOPED_RENDERER_STATE_H_
