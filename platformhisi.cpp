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

#include "platformhisi.h"
#include "drmdevice.h"
#include "platform.h"

#include <drm/drm_fourcc.h>
#include <stdatomic.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cinttypes>

#include <hardware/gralloc.h>
#include <log/log.h>
#include "gralloc_priv.h"

#define MALI_ALIGN(value, base) (((value) + ((base)-1)) & ~((base)-1))

namespace android {

Importer *Importer::CreateInstance(DrmDevice *drm) {
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

HisiImporter::HisiImporter(DrmDevice *drm)
    : DrmGenericImporter(drm), drm_(drm) {
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

int HisiImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {
  private_handle_t const *hnd = reinterpret_cast<private_handle_t const *>(
      handle);
  if (!hnd)
    return -EINVAL;

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), hnd->share_fd, &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", hnd->share_fd, ret);
    return ret;
  }

  int32_t fmt = ConvertHalFormatToDrm(hnd->req_format);
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

  switch (fmt) {
    case DRM_FORMAT_YVU420: {
      int align = 128;
      if (hnd->usage &
          (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
        align = 16;
      int adjusted_height = MALI_ALIGN(hnd->height, 2);
      int y_size = adjusted_height * hnd->byte_stride;
      int vu_stride = MALI_ALIGN(hnd->byte_stride / 2, align);
      int v_size = vu_stride * (adjusted_height / 2);

      /* V plane*/
      bo->gem_handles[1] = gem_handle;
      bo->pitches[1] = vu_stride;
      bo->offsets[1] = y_size;
      /* U plane */
      bo->gem_handles[2] = gem_handle;
      bo->pitches[2] = vu_stride;
      bo->offsets[2] = y_size + v_size;
      break;
    }
    default:
      break;
  }

  ret = drmModeAddFB2(drm_->fd(), bo->width, bo->height, bo->format,
                      bo->gem_handles, bo->pitches, bo->offsets, &bo->fb_id, 0);
  if (ret) {
    ALOGE("could not create drm fb %d", ret);
    return ret;
  }

  return ret;
}

std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageGreedy>();
  return planner;
}
}  // namespace android
