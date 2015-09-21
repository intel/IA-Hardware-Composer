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

#define LOG_TAG "hwc-drm-display-composition"

#include "drmdisplaycomposition.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"

#include <stdlib.h>

#include <cutils/log.h>
#include <sw_sync.h>
#include <sync/sync.h>
#include <xf86drmMode.h>

namespace android {

static native_handle_t *dup_buffer_handle(buffer_handle_t handle) {
  native_handle_t *new_handle =
      native_handle_create(handle->numFds, handle->numInts);
  if (new_handle == NULL)
    return NULL;

  const int *old_data = handle->data;
  int *new_data = new_handle->data;
  for (int i = 0; i < handle->numFds; i++) {
    *new_data = dup(*old_data);
    old_data++;
    new_data++;
  }
  memcpy(new_data, old_data, sizeof(int) * handle->numInts);

  return new_handle;
}

static void free_buffer_handle(native_handle_t *handle) {
  int ret = native_handle_close(handle);
  if (ret)
    ALOGE("Failed to close native handle %d", ret);
  ret = native_handle_delete(handle);
  if (ret)
    ALOGE("Failed to delete native handle %d", ret);
}

DrmCompositionLayer::DrmCompositionLayer()
    : crtc(NULL), plane(NULL), handle(NULL) {
  memset(&layer, 0, sizeof(layer));
  layer.releaseFenceFd = -1;
  layer.acquireFenceFd = -1;
  memset(&bo, 0, sizeof(bo));
}

DrmDisplayComposition::DrmDisplayComposition()
    : drm_(NULL),
      importer_(NULL),
      type_(DRM_COMPOSITION_TYPE_EMPTY),
      timeline_fd_(-1),
      timeline_(0),
      timeline_current_(0),
      timeline_pre_comp_done_(0),
      pre_composition_layer_index_(-1),
      dpms_mode_(DRM_MODE_DPMS_ON),
      frame_no_(0) {
}

DrmDisplayComposition::~DrmDisplayComposition() {
  for (DrmCompositionLayerVector_t::iterator iter = layers_.begin();
       iter != layers_.end(); ++iter) {
    if (importer_ && iter->bo.fb_id)
      importer_->ReleaseBuffer(&iter->bo);

    if (iter->handle) {
      gralloc_->unregisterBuffer(gralloc_, iter->handle);
      free_buffer_handle(iter->handle);
    }

    if (iter->layer.acquireFenceFd >= 0)
      close(iter->layer.acquireFenceFd);
  }

  if (timeline_fd_ >= 0) {
    FinishComposition();
    close(timeline_fd_);
    timeline_fd_ = -1;
  }
}

int DrmDisplayComposition::Init(DrmResources *drm, DrmCrtc *crtc,
                                Importer *importer, uint64_t frame_no) {
  drm_ = drm;
  crtc_ = crtc; // Can be NULL if we haven't modeset yet
  importer_ = importer;
  frame_no_ = frame_no;

  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
    return ret;
  }

  ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    return ret;
  }
  timeline_fd_ = ret;
  return 0;
}

DrmCompositionType DrmDisplayComposition::type() const {
  return type_;
}

bool DrmDisplayComposition::validate_composition_type(DrmCompositionType des) {
  return type_ == DRM_COMPOSITION_TYPE_EMPTY || type_ == des;
}

static DrmPlane *TakePlane(DrmCrtc *crtc, std::vector<DrmPlane *> *planes) {
  for (auto iter = planes->begin(); iter != planes->end(); ++iter) {
    if ((*iter)->GetCrtcSupported(*crtc)) {
      DrmPlane *plane = *iter;
      planes->erase(iter);
      return plane;
    }
  }
  return NULL;
}

static DrmPlane *TakePlane(DrmCrtc *crtc,
                           std::vector<DrmPlane *> *primary_planes,
                           std::vector<DrmPlane *> *overlay_planes) {
  DrmPlane *plane = TakePlane(crtc, primary_planes);
  if (plane)
    return plane;
  return TakePlane(crtc, overlay_planes);
}

int DrmDisplayComposition::CreateNextTimelineFence() {
  ++timeline_;
  return sw_sync_fence_create(timeline_fd_, "drm_fence", timeline_);
}

int DrmDisplayComposition::IncreaseTimelineToPoint(int point) {
  int timeline_increase = point - timeline_current_;
  if (timeline_increase <= 0)
    return 0;

  int ret = sw_sync_timeline_inc(timeline_fd_, timeline_increase);
  if (ret)
    ALOGE("Failed to increment sync timeline %d", ret);
  else
    timeline_current_ = point;

  return ret;
}

int DrmDisplayComposition::SetLayers(hwc_layer_1_t *layers, size_t num_layers,
                                     size_t *layer_indices,
                                     std::vector<DrmPlane *> *primary_planes,
                                     std::vector<DrmPlane *> *overlay_planes) {
  int ret = 0;
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    hwc_layer_1_t *layer = &layers[layer_indices[layer_index]];

    native_handle_t *handle_copy = dup_buffer_handle(layer->handle);
    if (handle_copy == NULL) {
      ALOGE("Failed to duplicate handle");
      return -ENOMEM;
    }

    int ret = gralloc_->registerBuffer(gralloc_, handle_copy);
    if (ret) {
      ALOGE("Failed to register buffer handle %d", ret);
      free_buffer_handle(handle_copy);
      return ret;
    }

    layers_.emplace_back();
    DrmCompositionLayer_t *c_layer = &layers_.back();
    c_layer->layer = *layer;
    c_layer->handle = handle_copy;
    c_layer->crtc = crtc_;

    ret = importer_->ImportBuffer(layer->handle, &c_layer->bo);
    if (ret) {
      ALOGE("Failed to import handle of layer %d", ret);
      goto fail;
    }

    if (pre_composition_layer_index_ == -1) {
      c_layer->plane = TakePlane(crtc_, primary_planes, overlay_planes);
      if (c_layer->plane == NULL) {
        if (layers_.size() <= 1) {
          ALOGE("Failed to match any planes to the crtc of this display");
          ret = -ENODEV;
          goto fail;
        }

        layers_.emplace_back();
        // c_layer's address might have changed when we resized the vector
        c_layer = &layers_[layers_.size() - 2];
        DrmCompositionLayer_t &pre_comp_layer = layers_.back();
        pre_comp_layer.crtc = crtc_;
        hwc_layer_1_t &pre_comp_output_layer = pre_comp_layer.layer;
        memset(&pre_comp_output_layer, 0, sizeof(pre_comp_output_layer));
        pre_comp_output_layer.compositionType = HWC_OVERLAY;
        pre_comp_output_layer.acquireFenceFd = -1;
        pre_comp_output_layer.releaseFenceFd = -1;
        pre_comp_output_layer.planeAlpha = 0xff;
        pre_comp_output_layer.visibleRegionScreen.numRects = 1;
        pre_comp_output_layer.visibleRegionScreen.rects =
            &pre_comp_output_layer.displayFrame;

        pre_composition_layer_index_ = layers_.size() - 1;

        // This is all to fix up the previous layer, which has now become part
        // of the set of pre-composition layers because we are stealing its
        // plane.
        DrmCompositionLayer_t &last_c_layer = layers_[layers_.size() - 3];
        std::swap(pre_comp_layer.plane, last_c_layer.plane);
        hwc_layer_1_t *last_layer = &layers[layer_indices[layer_index - 1]];
        ret = last_layer->releaseFenceFd = CreateNextTimelineFence();
        if (ret < 0) {
          ALOGE("Could not create release fence %d", ret);
          goto fail;
        }
      }
    }

    if (c_layer->plane == NULL) {
      // Layers to be pre composited all get the earliest release fences as they
      // will get released soonest.
      ret = layer->releaseFenceFd = CreateNextTimelineFence();
      if (ret < 0) {
        ALOGE("Could not create release fence %d", ret);
        goto fail;
      }
    }
  }

  timeline_pre_comp_done_ = timeline_;

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    hwc_layer_1_t *layer = &layers[layer_indices[layer_index]];
    if (layer->releaseFenceFd >= 0)
      continue;

    ret = layer->releaseFenceFd = CreateNextTimelineFence();
    if (ret < 0) {
      ALOGE("Could not create release fence %d", ret);
      goto fail;
    }
  }

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    hwc_layer_1_t *layer = &layers[layer_indices[layer_index]];
    layer->acquireFenceFd = -1;  // We own this now
  }

  type_ = DRM_COMPOSITION_TYPE_FRAME;
  return 0;

fail:

  for (size_t c_layer_index = 0; c_layer_index < layers_.size();
       c_layer_index++) {
    DrmCompositionLayer_t &c_layer = layers_[c_layer_index];
    if (c_layer.handle) {
      gralloc_->unregisterBuffer(gralloc_, c_layer.handle);
      free_buffer_handle(c_layer.handle);
    }
    if (c_layer.bo.fb_id)
      importer_->ReleaseBuffer(&c_layer.bo);
    if (c_layer.plane != NULL) {
      std::vector<DrmPlane *> *return_to =
          (c_layer.plane->type() == DRM_PLANE_TYPE_PRIMARY) ? primary_planes
                                                            : overlay_planes;
      return_to->insert(return_to->begin() + c_layer_index, c_layer.plane);
    }
  }
  layers_.clear();

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    hwc_layer_1_t *layer = &layers[layer_indices[layer_index]];
    if (layer->releaseFenceFd >= 0) {
      close(layer->releaseFenceFd);
      layer->releaseFenceFd = -1;
    }
  }
  sw_sync_timeline_inc(timeline_fd_, timeline_ - timeline_current_);

  timeline_ = timeline_current_;
  return ret;
}

int DrmDisplayComposition::SetDpmsMode(uint32_t dpms_mode) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_DPMS))
    return -EINVAL;
  dpms_mode_ = dpms_mode;
  type_ = DRM_COMPOSITION_TYPE_DPMS;
  return 0;
}

int DrmDisplayComposition::AddPlaneDisable(DrmPlane *plane) {
  layers_.emplace_back();
  DrmCompositionLayer_t &c_layer = layers_.back();
  c_layer.crtc = NULL;
  c_layer.plane = plane;
  return 0;
}

void DrmDisplayComposition::RemoveNoPlaneLayers() {
  for (auto &comp_layer : layers_) {
    if (comp_layer.plane != NULL)
      continue;

    if (importer_ && comp_layer.bo.fb_id) {
      importer_->ReleaseBuffer(&comp_layer.bo);
    }

    if (comp_layer.handle) {
      gralloc_->unregisterBuffer(gralloc_, comp_layer.handle);
      free_buffer_handle(comp_layer.handle);
    }

    if (comp_layer.layer.acquireFenceFd >= 0) {
      close(comp_layer.layer.acquireFenceFd);
      comp_layer.layer.acquireFenceFd = -1;
    }
  }

  layers_.erase(
      std::remove_if(layers_.begin(), layers_.end(),
                     [](DrmCompositionLayer_t &l) { return l.plane == NULL; }),
      layers_.end());
}

int DrmDisplayComposition::SignalPreCompositionDone() {
  return IncreaseTimelineToPoint(timeline_pre_comp_done_);
}

int DrmDisplayComposition::FinishComposition() {
  return IncreaseTimelineToPoint(timeline_);
}

DrmCompositionLayerVector_t *DrmDisplayComposition::GetCompositionLayers() {
  return &layers_;
}

int DrmDisplayComposition::pre_composition_layer_index() const {
  return pre_composition_layer_index_;
}

uint32_t DrmDisplayComposition::dpms_mode() const {
  return dpms_mode_;
}

uint64_t DrmDisplayComposition::frame_no() const {
  return frame_no_;
}

Importer *DrmDisplayComposition::importer() const {
  return importer_;
}
}
