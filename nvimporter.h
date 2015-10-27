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

#ifndef ANDROID_NV_IMPORTER_H_
#define ANDROID_NV_IMPORTER_H_

#include "drmresources.h"
#include "importer.h"

#include <stdatomic.h>

#include <hardware/gralloc.h>

namespace android {

class NvImporter : public Importer {
 public:
  NvImporter(DrmResources *drm);
  ~NvImporter() override;

  int Init();

  int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) override;
  int ReleaseBuffer(hwc_drm_bo_t *bo) override;

 private:
  typedef struct NvBuffer {
    NvImporter *importer;
    hwc_drm_bo_t bo;
    atomic_int ref;
  } NvBuffer_t;

  static void NvGrallocRelease(void *nv_buffer);
  void ReleaseBufferImpl(hwc_drm_bo_t *bo);

  NvBuffer_t *GrallocGetNvBuffer(buffer_handle_t handle);
  int GrallocSetNvBuffer(buffer_handle_t handle, NvBuffer_t *buf);

  DrmResources *drm_;

  const gralloc_module_t *gralloc_;
};
}

#endif
