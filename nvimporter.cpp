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

#define LOG_TAG "hwc-nv-importer"

#include "drmresources.h"
#include "importer.h"
#include "nvimporter.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>
#include <hardware/gralloc.h>

namespace android {

#ifdef USE_NVIDIA_IMPORTER
// static
Importer *Importer::CreateInstance(DrmResources *drm) {
  NvImporter *importer = new NvImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the nv importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}
#endif

NvImporter::NvImporter(DrmResources *drm) : drm_(drm) {
}

NvImporter::~NvImporter() {
}

int NvImporter::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
    return ret;
  }

  if (strcasecmp(gralloc_->common.author, "NVIDIA"))
    ALOGW("Using non-NVIDIA gralloc module: %s/%s\n", gralloc_->common.name,
          gralloc_->common.author);

  return 0;
}

int NvImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {
  memset(bo, 0, sizeof(hwc_drm_bo_t));
  NvBuffer_t *buf = GrallocGetNvBuffer(handle);
  if (buf) {
    *bo = buf->bo;
    return 0;
  }

  buf = new NvBuffer_t();
  if (!buf) {
    ALOGE("Failed to allocate new NvBuffer_t");
    return -ENOMEM;
  }
  buf->importer = this;

  int ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_DRM_IMPORT,
                              drm_->fd(), handle, &buf->bo);
  if (ret) {
    ALOGE("GRALLOC_MODULE_PERFORM_DRM_IMPORT failed %d", ret);
    delete buf;
    return ret;
  }

  ret = drmModeAddFB2(drm_->fd(), buf->bo.width, buf->bo.height, buf->bo.format,
                      buf->bo.gem_handles, buf->bo.pitches, buf->bo.offsets,
                      &buf->bo.fb_id, 0);
  if (ret) {
    ALOGE("Failed to add fb %d", ret);
    ReleaseBufferImpl(&buf->bo);
    delete buf;
    return ret;
  }

  ret = GrallocSetNvBuffer(handle, buf);
  if (ret) {
    /* This will happen is persist.tegra.gpu_mapping_cache is 0/off,
     * or if NV gralloc runs out of "priv slots" (currently 3 per buffer,
     * only one of which should be used by drm_hwcomposer). */
    ALOGE("Failed to register free callback for imported buffer %d", ret);
    ReleaseBufferImpl(&buf->bo);
    delete buf;
    return ret;
  }
  *bo = buf->bo;
  return 0;
}

int NvImporter::ReleaseBuffer(hwc_drm_bo_t * /* bo */) {
  /* Stub this out since we don't want hwc releasing buffers */
  return 0;
}

// static
void NvImporter::ReleaseBufferCallback(void *nv_buffer) {
  NvBuffer_t *buf = (NvBuffer_t *)nv_buffer;
  buf->importer->ReleaseBufferImpl(&buf->bo);
  delete buf;
}

void NvImporter::ReleaseBufferImpl(hwc_drm_bo_t *bo) {
  if (bo->fb_id) {
    int ret = drmModeRmFB(drm_->fd(), bo->fb_id);
    if (ret)
      ALOGE("Failed to rm fb %d", ret);
  }

  struct drm_gem_close gem_close;
  memset(&gem_close, 0, sizeof(gem_close));
  int num_gem_handles = sizeof(bo->gem_handles) / sizeof(bo->gem_handles[0]);
  for (int i = 0; i < num_gem_handles; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret)
      ALOGE("Failed to close gem handle %d %d", i, ret);
    else
      bo->gem_handles[i] = 0;
  }
}

NvImporter::NvBuffer_t *NvImporter::GrallocGetNvBuffer(buffer_handle_t handle) {
  void *priv = NULL;
  int ret =
      gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_GET_IMPORTER_PRIVATE,
                        handle, ReleaseBufferCallback, &priv);
  return ret ? NULL : (NvBuffer_t *)priv;
}

int NvImporter::GrallocSetNvBuffer(buffer_handle_t handle, NvBuffer_t *buf) {
  return gralloc_->perform(gralloc_,
                           GRALLOC_MODULE_PERFORM_SET_IMPORTER_PRIVATE, handle,
                           ReleaseBufferCallback, buf);
}
}
