/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ANDROID_PLATFORM_DRM_MINIGBM_H_
#define ANDROID_PLATFORM_DRM_MINIGBM_H_

#include "drmdevice.h"
#include "platform.h"
#include "platformdrmgeneric.h"

#include <hardware/gralloc.h>

namespace android {

class DrmMinigbmImporter : public DrmGenericImporter {
 public:
  DrmMinigbmImporter(DrmDevice *drm);
  ~DrmMinigbmImporter() override;

  int Init();

  int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) override;

 private:
  DrmDevice *drm_;

  const gralloc_module_t *gralloc_;
};

}  // namespace android

#endif
