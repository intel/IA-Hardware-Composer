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

#define LOG_TAG "hwc-platform-hisi"

#include "drmresources.h"
#include "platform.h"
#include "platformhisi.h"


#include <drm/drm_fourcc.h>
#include <cinttypes>
#include <stdatomic.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>
#include <hardware/gralloc.h>
#include "gralloc_priv.h"


namespace android {

Importer *Importer::CreateInstance(DrmResources *drm) {
  HisiImporter *importer = new HisiImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the hisi importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}

HisiImporter::HisiImporter(DrmResources *drm) : DrmGenericImporter(drm), drm_(drm) {
}

HisiImporter::~HisiImporter() {
}

int HisiImporter::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
    return ret;
  }

  if (strcasecmp(gralloc_->common.author, "ARM Ltd."))
    ALOGW("Using non-ARM gralloc module: %s/%s\n", gralloc_->common.name,
          gralloc_->common.author);

  return 0;
}

EGLImageKHR HisiImporter::ImportImage(EGLDisplay egl_display, buffer_handle_t handle) {
  private_handle_t const *hnd = reinterpret_cast < private_handle_t const *>(handle);
  if (!hnd)
    return NULL;

  EGLint fmt = ConvertHalFormatToDrm(hnd->req_format);
  if (fmt < 0)
	return NULL;

  EGLint attr[] = {
    EGL_WIDTH, hnd->width,
    EGL_HEIGHT, hnd->height,
    EGL_LINUX_DRM_FOURCC_EXT, fmt,
    EGL_DMA_BUF_PLANE0_FD_EXT, hnd->share_fd,
    EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
    EGL_DMA_BUF_PLANE0_PITCH_EXT, hnd->byte_stride,
    EGL_NONE,
  };
  return eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attr);
}

int HisiImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {
  private_handle_t const *hnd = reinterpret_cast < private_handle_t const *>(handle);
  if (!hnd)
    return -EINVAL;

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), hnd->share_fd, &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", hnd->share_fd, ret);
    return ret;
  }

  EGLint fmt = ConvertHalFormatToDrm(hnd->req_format);
  if (fmt < 0)
	return fmt;

  memset(bo, 0, sizeof(hwc_drm_bo_t));
  bo->width = hnd->width;
  bo->height = hnd->height;
  bo->format = fmt;
  bo->usage = hnd->usage;
  bo->pitches[0] = hnd->byte_stride;
  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;

  ret = drmModeAddFB2(drm_->fd(), bo->width, bo->height, bo->format,
                      bo->gem_handles, bo->pitches, bo->offsets, &bo->fb_id, 0);
  if (ret) {
    ALOGE("could not create drm fb %d", ret);
    return ret;
  }

  return ret;
}

std::unique_ptr<Planner> Planner::CreateInstance(DrmResources *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageGreedy>();
  return planner;
}
}


