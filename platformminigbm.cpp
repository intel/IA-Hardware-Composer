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
#include "drmdevice.h"
#include "platform.h"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <hardware/gralloc.h>
#include <log/log.h>

#include "cros_gralloc_handle.h"
#include "vautils.h"
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
  importer->EnableVaRender();
  return importer;
}


void DrmMinigbmImporter::EnableVaRender() {
  if (!media_renderer_) {
    media_renderer_.reset(new VARenderer());
    if (!media_renderer_->Init(drm_->fd())) {
      ALOGE("Failed to initialize Media va Renderer \n");
      media_renderer_.reset(nullptr);
    }
  }
}

DrmMinigbmImporter::DrmMinigbmImporter(DrmDevice *drm)
    : DrmGenericImporter(drm), drm_(drm) {
}

DrmMinigbmImporter::~DrmMinigbmImporter() {
  if (NULL != media_renderer_)
    media_renderer_.reset(nullptr);
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

#ifdef ENABLE_DUMP_YUV_DATA
#define DUMP_DATA_FILE "/data/temp"

static void DumpData(buffer_handle_t handle){
  if (NULL == handle)
    return;
  cros_gralloc_handle *gr_handle = (cros_gralloc_handle *)handle;
  native_handle_t *handle_copy;
  uint8_t* pixels;
  GraphicBufferMapper &gm(GraphicBufferMapper::get());
  int ret = gm.importBuffer(handle, gr_handle->width, gr_handle->height, 1, gr_handle->format/*DRM_FORMAT_YUV420*/, gr_handle->usage,
                          gr_handle->pixel_stride, const_cast<buffer_handle_t *>(&handle_copy));
  if (ret != 0)
    ALOGE("in platformminigbm.cpp function %s,line %d ret=%d::fail to import buffer\n",__FUNCTION__,__LINE__,ret);
    ret = gm.lock(handle_copy, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_NEVER,
                     Rect(gr_handle->width, gr_handle->height), reinterpret_cast<void**>(&pixels));
    if (ret != 0)
      ALOGE("in platformminigbm.cpp function %s,line %d ret=%d::fail to lock buffer\n",__FUNCTION__,__LINE__,ret);
    int file_fd = 0;
    file_fd = open(DUMP_DATA_FILE, O_RDWR|O_CREAT|O_APPEND, 0666);
    if (file_fd == -1) {
      ALOGE("in platformminigbm.cpp function %s,line %d::fail to open file\n",__FUNCTION__,__LINE__);
      return;
    }
    write(file_fd, pixels, gr_handle->sizes[0]);
    gm.unlock(handle_copy);
    gm.freeBuffer(handle_copy);
    close(file_fd);
}
#endif

int DrmMinigbmImporter::ImportBuffer(DrmHwcLayer* layer, hwc_drm_bo_t *bo){
  cros_gralloc_handle *gr_handle = (cros_gralloc_handle *)layer->get_usable_handle();
  int ret = 0;
  uint32_t flag =0;
  bool vendor_flag = false;
  if (!gr_handle) {
    return -EINVAL;
  }
  uint32_t gem_handle;
  memset(bo, 0, sizeof(hwc_drm_bo_t));
  if (IsSupportedMediaFormat(gr_handle->format)) {
    media_renderer_->startRender(layer, DRM_FORMAT_ABGR8888);
    //for avoid flushing when do the rotation
    if(bak_transform != layer->transform){
      bak_transform = layer->transform;
      return -EINVAL;
    }
    gr_handle =  (cros_gralloc_handle *)media_renderer_->getPreBuffer();
    vendor_flag = true;
  }
  ret = drmPrimeFDToHandle(drm_->fd(), gr_handle->fds[0], &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", gr_handle->fds[0], ret);
    return ret;
  }
  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->hal_format = gr_handle->droid_format;
  bo->format = gr_handle->format;
  bo->usage = gr_handle->usage;
  bo->pixel_stride = gr_handle->pixel_stride;
  bo->pitches[0] = gr_handle->strides[0];
  bo->offsets[0] = gr_handle->offsets[0];
  bo->gem_handles[0] = gem_handle;
#ifdef ENABLE_DUMP_YUV_DATA
  if(vendor_flag){
    DumpData((buffer_handle_t)gr_handle);
  }
#endif
  if ((vendor_flag) && ((layer->transform == kHwcTransform270) || (layer->transform == kHwcTransform90))) {
     flag = DRM_MODE_FB_MODIFIERS;
     uint64_t modifiers[4];
     uint32_t numplanes = gr_handle->base.numFds;

     for (uint32_t i = 0; i < numplanes; i++) {
       if(vendor_flag){
           modifiers[i] = I915_FORMAT_MOD_Y_TILED;
       }
     }
     for (uint32_t i = numplanes; i < 4; i++) {
       modifiers[i] = DRM_FORMAT_MOD_NONE;
     }
     ret = drmModeAddFB2WithModifiers(
           drm_->fd(), bo->width, bo->height, bo->format, bo->gem_handles,
           bo->pitches, bo->offsets, modifiers, &bo->fb_id, flag);
  }else
    ret = drmModeAddFB2(drm_->fd(), bo->width, bo->height, bo->format,
          bo->gem_handles, bo->pitches, bo->offsets, &bo->fb_id, flag);
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
