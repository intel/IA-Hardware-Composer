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

DrmCompositionLayer::DrmCompositionLayer(DrmCrtc *crtc, DrmHwcLayer &&l)
    : crtc(crtc),
      sf_handle(l.sf_handle),
      buffer(std::move(l.buffer)),
      handle(std::move(l.handle)),
      transform(l.transform),
      blending(l.blending),
      alpha(l.alpha),
      source_crop(l.source_crop),
      display_frame(l.display_frame),
      source_damage(l.source_damage),
      acquire_fence(std::move(l.acquire_fence)) {
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
  if (timeline_fd_ >= 0) {
    FinishComposition();
    close(timeline_fd_);
    timeline_fd_ = -1;
  }
}

int DrmDisplayComposition::Init(DrmResources *drm, DrmCrtc *crtc,
                                Importer *importer, uint64_t frame_no) {
  drm_ = drm;
  crtc_ = crtc;  // Can be NULL if we haven't modeset yet
  importer_ = importer;
  frame_no_ = frame_no;

  int ret = sw_sync_timeline_create();
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

int DrmDisplayComposition::SetLayers(DrmHwcLayer *layers, size_t num_layers,
                                     std::vector<DrmPlane *> *primary_planes,
                                     std::vector<DrmPlane *> *overlay_planes) {
  int ret = 0;
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    DrmHwcLayer *layer = &layers[layer_index];

    layers_.emplace_back(crtc_, std::move(*layer));
    DrmCompositionLayer *c_layer = &layers_.back();

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
        DrmCompositionLayer &pre_comp_layer = layers_.back();
        pre_comp_layer.crtc = crtc_;

        pre_composition_layer_index_ = layers_.size() - 1;

        // This is all to fix up the previous layer, which has now become part
        // of the set of pre-composition layers because we are stealing its
        // plane.
        DrmCompositionLayer &last_c_layer = layers_[layers_.size() - 3];
        std::swap(pre_comp_layer.plane, last_c_layer.plane);
        OutputFd &last_release_fence = layers[layer_index - 1].release_fence;
        last_release_fence.Set(CreateNextTimelineFence());
        ret = last_release_fence.get();
        if (ret < 0) {
          ALOGE("Could not create release fence %d", ret);
          goto fail;
        }
      }
    }

    if (c_layer->plane == NULL) {
      // Layers to be pre composited all get the earliest release fences as they
      // will get released soonest.
      layer->release_fence.Set(CreateNextTimelineFence());
      ret = layer->release_fence.get();
      if (ret < 0) {
        ALOGE("Could not create release fence %d", ret);
        goto fail;
      }
    }
  }

  timeline_pre_comp_done_ = timeline_;

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    DrmHwcLayer *layer = &layers[layer_index];
    if (layer->release_fence.get() >= 0)
      continue;

    ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0) {
      ALOGE("Could not create release fence %d", ret);
      goto fail;
    }
  }

  type_ = DRM_COMPOSITION_TYPE_FRAME;
  return 0;

fail:

  for (size_t c_layer_index = 0; c_layer_index < layers_.size();
       c_layer_index++) {
    DrmCompositionLayer &c_layer = layers_[c_layer_index];
    if (c_layer.plane != NULL) {
      std::vector<DrmPlane *> *return_to =
          (c_layer.plane->type() == DRM_PLANE_TYPE_PRIMARY) ? primary_planes
                                                            : overlay_planes;
      return_to->insert(return_to->begin() + c_layer_index, c_layer.plane);
    }
  }

  layers_.clear();

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

int DrmDisplayComposition::SetDisplayMode(const DrmMode &display_mode) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_MODESET))
    return -EINVAL;
  display_mode_ = display_mode;
  dpms_mode_ = DRM_MODE_DPMS_ON;
  type_ = DRM_COMPOSITION_TYPE_MODESET;
  return 0;
}

int DrmDisplayComposition::AddPlaneDisable(DrmPlane *plane) {
  layers_.emplace_back();
  DrmCompositionLayer &c_layer = layers_.back();
  c_layer.crtc = NULL;
  c_layer.plane = plane;
  return 0;
}

void DrmDisplayComposition::RemoveNoPlaneLayers() {
  layers_.erase(
      std::remove_if(layers_.begin(), layers_.end(),
                     [](DrmCompositionLayer &l) { return l.plane == NULL; }),
      layers_.end());
}

int DrmDisplayComposition::SignalPreCompositionDone() {
  return IncreaseTimelineToPoint(timeline_pre_comp_done_);
}

int DrmDisplayComposition::FinishComposition() {
  return IncreaseTimelineToPoint(timeline_);
}

std::vector<DrmCompositionLayer>
    *DrmDisplayComposition::GetCompositionLayers() {
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

const DrmMode &DrmDisplayComposition::display_mode() const {
  return display_mode_;
}

Importer *DrmDisplayComposition::importer() const {
  return importer_;
}
}
