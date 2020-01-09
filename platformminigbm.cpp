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

#define LOG_TAG "hwc-platform-drm-minigbm"

#include "platformminigbm.h"
#include "i915_private_types.h"
#include "drmdevice.h"
#include "platform.h"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <hardware/gralloc.h>
#include <log/log.h>

#include "cros_gralloc_handle.h"

namespace android {

Importer *Importer::CreateInstance(DrmDevice *drm) {
  DrmMinigbmImporter *importer = new DrmMinigbmImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the minigbm importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}

DrmMinigbmImporter::DrmMinigbmImporter(DrmDevice *drm)
    : DrmGenericImporter(drm), drm_(drm) {
}

DrmMinigbmImporter::~DrmMinigbmImporter() {
}

int DrmMinigbmImporter::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
    return ret;
  }

  if (strcasecmp(gralloc_->common.author, "Chrome OS"))
    ALOGW("Using non-minigbm gralloc module: %s/%s\n", gralloc_->common.name,
          gralloc_->common.author);

  return 0;
}

int DrmMinigbmImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {
  cros_gralloc_handle *gr_handle = (cros_gralloc_handle *)handle;
  if (!gr_handle) {
    return -EINVAL;
  }

  bool vendor_flag = false;
  if (gr_handle->format == DRM_FORMAT_NV12_Y_TILED_INTEL) {
    vendor_flag = true;
  }

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), gr_handle->fds[0], &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", gr_handle->fds[0], ret);
    return ret;
  }

  memset(bo, 0, sizeof(hwc_drm_bo_t));
  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->hal_format = gr_handle->droid_format;
  if (vendor_flag) {
    bo->format = DRM_FORMAT_NV12;
  }
  else
  {
    bo->format = gr_handle->format;
  }
  bo->usage = gr_handle->usage;
  bo->pixel_stride = gr_handle->pixel_stride;

  uint32_t numPlanes = gr_handle->base.numFds;
  for (uint32_t i = 0; i < numPlanes; ++i) {
    bo->pitches[i] = gr_handle->strides[i];
    bo->offsets[i] = gr_handle->offsets[i];
    bo->gem_handles[i] = gem_handle;
  }

  if (vendor_flag == false) {
    ret = drmModeAddFB2(drm_->fd(), bo->width, bo->height, bo->format,
                        bo->gem_handles, bo->pitches, bo->offsets, &bo->fb_id, 0);
  }
  else {
    uint32_t flag = DRM_MODE_FB_MODIFIERS;
    uint64_t modifiers[4];
    uint32_t numPlanes = gr_handle->base.numFds;

    for (uint32_t i = 0; i < numPlanes; i++) {
      modifiers[i] = I915_FORMAT_MOD_Y_TILED;
    }
    for (uint32_t i = numPlanes; i < 4; i++) {
      modifiers[i] = DRM_FORMAT_MOD_NONE;
    }

    ret = drmModeAddFB2WithModifiers(
              drm_->fd(), bo->width, bo->height, bo->format, bo->gem_handles,
              bo->pitches, bo->offsets, modifiers, &bo->fb_id, flag);
  }

  if (ret) {
    ALOGE("could not create drm fb %d", ret);
  }

  return ret;
}

std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageGreedy>();
  return planner;
}

}  // namespace android
