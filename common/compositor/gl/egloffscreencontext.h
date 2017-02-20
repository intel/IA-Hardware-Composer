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

#ifndef EGL_CONTEXT_H_
#define EGL_CONTEXT_H_

#include "shim.h"

namespace hwcomposer {

class EGLOffScreenContext {
 public:
  EGLOffScreenContext();
  ~EGLOffScreenContext();

  bool Init();

  EGLint GetSyncFD();

  EGLDisplay GetDisplay() const {
    return egl_display_;
  }

 private:
  EGLDisplay egl_display_;
  EGLContext egl_ctx_;
};

}  // namespace hwcomposer
#endif  // EGL_CONTEXT_H_
