/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdint.h>
#include "compositor.h"

struct hwc_layer_1;
struct hwc_import_context;

namespace android {

class GLComposition;

class GLCompositor : public Compositor, public Targeting {
 public:
  GLCompositor();
  virtual ~GLCompositor();

  virtual int Init();
  virtual Targeting *targeting();
  virtual int CreateTarget(sp<android::GraphicBuffer> &buffer);
  virtual void SetTarget(int target);
  virtual void ForgetTarget(int target);
  virtual Composition *CreateComposition(Importer *importer);
  virtual int QueueComposition(Composition *composition);
  virtual int Composite();

 private:
  struct priv_data;
  struct texture_from_handle;

  struct priv_data *priv_;

  int BeginContext();
  int EndContext();
  int GenerateShaders();
  int DoComposition(const GLComposition &composition);
  int DoFenceWait(int acquireFenceFd);
  int CreateTextureFromHandle(buffer_handle_t handle,
                              struct texture_from_handle *tex);
  void DestroyTextureFromHandle(const struct texture_from_handle &tex);
  void CheckAndDestroyTarget(int target_handle);

  friend GLComposition;
};

}  // namespace android
