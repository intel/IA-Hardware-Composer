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

#ifndef ANDROID_DRM_IMPORTER_H_
#define ANDROID_DRM_IMPORTER_H_

#include "drm_hwcomposer.h"

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

class DrmResources;

class Importer {
 public:
  virtual ~Importer() {
  }

  // Creates a platform-specific importer instance
  static Importer *CreateInstance(DrmResources *drm);

  // Imports the buffer referred to by handle into bo.
  //
  // Note: This can be called from a different thread than ReleaseBuffer. The
  //       implementation is responsible for ensuring thread safety.
  virtual int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) = 0;

  // Releases the buffer object (ie: does the inverse of ImportBuffer)
  //
  // Note: This can be called from a different thread than ImportBuffer. The
  //       implementation is responsible for ensuring thread safety.
  virtual int ReleaseBuffer(hwc_drm_bo_t *bo) = 0;
};
}

#endif
