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

#include <algorithm>
#include <unordered_set>

#include <cutils/log.h>
#include <sw_sync.h>
#include <sync/sync.h>
#include <xf86drmMode.h>

namespace android {

const size_t DrmCompositionPlane::kSourceNone;
const size_t DrmCompositionPlane::kSourcePreComp;
const size_t DrmCompositionPlane::kSourceSquash;
const size_t DrmCompositionPlane::kSourceLayerMax;

DrmDisplayComposition::~DrmDisplayComposition() {
  if (timeline_fd_ >= 0) {
    SignalCompositionDone();
    close(timeline_fd_);
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

bool DrmDisplayComposition::validate_composition_type(DrmCompositionType des) {
  return type_ == DRM_COMPOSITION_TYPE_EMPTY || type_ == des;
}

int DrmDisplayComposition::CreateNextTimelineFence() {
  ++timeline_;
  return sw_sync_fence_create(timeline_fd_, "hwc drm display composition fence",
                              timeline_);
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

int DrmDisplayComposition::SetLayers(DrmHwcLayer *layers, size_t num_layers) {
  int ret = 0;
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  for (size_t layer_index = 0; layer_index < num_layers; layer_index++) {
    layers_.emplace_back(std::move(layers[layer_index]));
  }

  type_ = DRM_COMPOSITION_TYPE_FRAME;
  return 0;
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
  composition_planes_.emplace_back(
      DrmCompositionPlane{plane, crtc_, DrmCompositionPlane::kSourceNone});
  return 0;
}

static size_t CountUsablePlanes(DrmCrtc *crtc,
                                std::vector<DrmPlane *> *primary_planes,
                                std::vector<DrmPlane *> *overlay_planes) {
  return std::count_if(
             primary_planes->begin(), primary_planes->end(),
             [=](DrmPlane *plane) { return plane->GetCrtcSupported(*crtc); }) +
         std::count_if(
             overlay_planes->begin(), overlay_planes->end(),
             [=](DrmPlane *plane) { return plane->GetCrtcSupported(*crtc); });
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

static std::vector<size_t> SetBitsToVector(uint64_t in, size_t *index_map) {
  std::vector<size_t> out;
  size_t msb = sizeof(in) * 8 - 1;
  uint64_t mask = (uint64_t)1 << msb;
  for (size_t i = msb; mask != (uint64_t)0; i--, mask >>= 1)
    if (in & mask)
      out.push_back(index_map[i]);
  return out;
}

static void SeperateLayers(DrmHwcLayer *layers, size_t *used_layers,
                           size_t num_used_layers,
                           DrmHwcRect<int> *exclude_rects,
                           size_t num_exclude_rects,
                           std::vector<DrmCompositionRegion> &regions) {
  if (num_used_layers > 64) {
    ALOGE("Failed to separate layers because there are more than 64");
    return;
  }

  if (num_used_layers + num_exclude_rects > 64) {
    ALOGW(
        "Exclusion rectangles are being truncated to make the rectangle count "
        "fit into 64");
    num_exclude_rects = 64 - num_used_layers;
  }

  // We inject all the exclude rects into the rects list. Any resulting rect
  // that includes ANY of the first num_exclude_rects is rejected.
  std::vector<DrmHwcRect<int>> layer_rects(num_used_layers + num_exclude_rects);
  std::copy(exclude_rects, exclude_rects + num_exclude_rects,
            layer_rects.begin());
  std::transform(
      used_layers, used_layers + num_used_layers,
      layer_rects.begin() + num_exclude_rects,
      [=](size_t layer_index) { return layers[layer_index].display_frame; });

  std::vector<seperate_rects::RectSet<uint64_t, int>> seperate_regions;
  seperate_rects::seperate_rects_64(layer_rects, &seperate_regions);
  uint64_t exclude_mask = ((uint64_t)1 << num_exclude_rects) - 1;

  for (seperate_rects::RectSet<uint64_t, int> &region : seperate_regions) {
    if (region.id_set.getBits() & exclude_mask)
      continue;
    regions.emplace_back(DrmCompositionRegion{
        region.rect,
        SetBitsToVector(region.id_set.getBits() >> num_exclude_rects,
                        used_layers)});
  }
}

int DrmDisplayComposition::Plan(SquashState *squash,
                                std::vector<DrmPlane *> *primary_planes,
                                std::vector<DrmPlane *> *overlay_planes) {
  size_t planes_can_use =
      CountUsablePlanes(crtc_, primary_planes, overlay_planes);
  if (planes_can_use == 0) {
    ALOGE("Display %d has no usable planes", crtc_->display());
    return -ENODEV;
  }

  std::vector<int> layer_squash_area(layers_.size());
  if (squash != NULL && planes_can_use >= 3) {
    std::vector<bool> changed_regions;
    squash->GenerateHistory(layers_.data(), changed_regions);

    std::vector<bool> stable_regions;
    squash->StableRegionsWithMarginalHistory(changed_regions, stable_regions);

    squash->RecordHistory(layers_.data(), changed_regions);

    squash->RecordSquashed(stable_regions);

    for (size_t region_index = 0; region_index < stable_regions.size();
         region_index++) {
      const SquashState::Region &region = squash->regions()[region_index];
      if (stable_regions[region_index]) {
        squash_regions_.emplace_back();
        DrmCompositionRegion &squash_region = squash_regions_.back();
        squash_region.frame = region.rect;
        for (size_t layer_index = 0; layer_index < SquashState::kMaxLayers;
             layer_index++) {
          if (region.layer_refs[layer_index]) {
            squash_region.source_layers.push_back(layer_index);
            layer_squash_area[layer_index] += squash_region.frame.area();
          }
        }
      }
    }
  }

  std::vector<size_t> layers_remaining;
  for (size_t layer_index = 0; layer_index < layers_.size(); layer_index++) {
    // Skip layers that were completely squashed
    if (layer_squash_area[layer_index] >=
        layers_[layer_index].display_frame.area()) {
      continue;
    }

    layers_remaining.push_back(layer_index);
  }

  size_t layer_to_composite = layers_remaining.size();
  size_t num_layers_to_pre_composite = 0;
  if (squash_regions_.size() > 0) {
    layers_remaining.push_back(DrmCompositionPlane::kSourceSquash);
  }

  if (layers_remaining.size() > planes_can_use) {
    layers_remaining.insert(layers_remaining.begin() + layer_to_composite,
                            DrmCompositionPlane::kSourcePreComp);
    size_t num_layers_to_pre_composite =
        layer_to_composite - planes_can_use + 1;
    size_t first_layer_to_pre_composite = planes_can_use - 1;
    SeperateLayers(layers_.data(),
                   &layers_remaining[first_layer_to_pre_composite],
                   num_layers_to_pre_composite, NULL, 0, pre_comp_regions_);
    layers_remaining.erase(
        layers_remaining.begin() + first_layer_to_pre_composite,
        layers_remaining.begin() + layer_to_composite);
  }

  for (size_t i : layers_remaining) {
    composition_planes_.emplace_back(DrmCompositionPlane{
        TakePlane(crtc_, primary_planes, overlay_planes), crtc_, i});
  }

  std::unordered_set<DrmHwcLayer *> squash_layers;
  std::unordered_set<DrmHwcLayer *> pre_comp_layers;
  std::unordered_set<DrmHwcLayer *> comp_layers;

  for (const DrmCompositionRegion &region : squash_regions_) {
    for (size_t source_layer_index : region.source_layers) {
      DrmHwcLayer *source_layer = &layers_[source_layer_index];
      squash_layers.emplace(source_layer);
    }
  }

  for (const DrmCompositionRegion &region : pre_comp_regions_) {
    for (size_t source_layer_index : region.source_layers) {
      DrmHwcLayer *source_layer = &layers_[source_layer_index];
      pre_comp_layers.emplace(source_layer);
      squash_layers.erase(source_layer);
    }
  }

  for (const DrmCompositionPlane &plane : composition_planes_) {
    if (plane.source_layer <= DrmCompositionPlane::kSourceLayerMax) {
      DrmHwcLayer *source_layer = &layers_[plane.source_layer];
      comp_layers.emplace(source_layer);
      pre_comp_layers.erase(source_layer);
    }
  }

  for (DrmHwcLayer *layer : squash_layers) {
    int ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0)
      return ret;
  }
  timeline_squash_done_ = timeline_;

  for (DrmHwcLayer *layer : pre_comp_layers) {
    int ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0)
      return ret;
  }
  timeline_pre_comp_done_ = timeline_;

  for (DrmHwcLayer *layer : comp_layers) {
    int ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0)
      return ret;
  }

  return 0;
}

static const char *DrmCompositionTypeToString(DrmCompositionType type) {
  switch (type) {
    case DRM_COMPOSITION_TYPE_EMPTY:
      return "EMPTY";
    case DRM_COMPOSITION_TYPE_FRAME:
      return "FRAME";
    case DRM_COMPOSITION_TYPE_DPMS:
      return "DPMS";
    case DRM_COMPOSITION_TYPE_MODESET:
      return "MODESET";
    default:
      return "<invalid>";
  }
}

static const char *DPMSModeToString(int dpms_mode) {
  switch (dpms_mode) {
    case DRM_MODE_DPMS_ON:
      return "ON";
    case DRM_MODE_DPMS_OFF:
      return "OFF";
    default:
      return "<invalid>";
  }
}

static void DumpBuffer(const DrmHwcBuffer &buffer, std::ostringstream *out) {
  if (!buffer) {
    *out << "buffer=<invalid>";
    return;
  }

  *out << "buffer[w/h/format]=";
  *out << buffer->width << "/" << buffer->height << "/" << buffer->format;
}

static const char *TransformToString(DrmHwcTransform transform) {
  switch (transform) {
    case DrmHwcTransform::kIdentity:
      return "IDENTITY";
    case DrmHwcTransform::kFlipH:
      return "FLIPH";
    case DrmHwcTransform::kFlipV:
      return "FLIPV";
    case DrmHwcTransform::kRotate90:
      return "ROTATE90";
    case DrmHwcTransform::kRotate180:
      return "ROTATE180";
    case DrmHwcTransform::kRotate270:
      return "ROTATE270";
    default:
      return "<invalid>";
  }
}

static const char *BlendingToString(DrmHwcBlending blending) {
  switch (blending) {
    case DrmHwcBlending::kNone:
      return "NONE";
    case DrmHwcBlending::kPreMult:
      return "PREMULT";
    case DrmHwcBlending::kCoverage:
      return "COVERAGE";
    default:
      return "<invalid>";
  }
}

static void DumpRegion(const DrmCompositionRegion &region,
                       std::ostringstream *out) {
  *out << "frame";
  region.frame.Dump(out);
  *out << " source_layers=(";

  const std::vector<size_t> &source_layers = region.source_layers;
  for (size_t i = 0; i < source_layers.size(); i++) {
    *out << source_layers[i];
    if (i < source_layers.size() - 1) {
      *out << " ";
    }
  }

  *out << ")";
}

void DrmDisplayComposition::Dump(std::ostringstream *out) const {
  *out << "----DrmDisplayComposition"
       << " crtc=" << (crtc_ ? crtc_->id() : -1)
       << " type=" << DrmCompositionTypeToString(type_);

  switch (type_) {
    case DRM_COMPOSITION_TYPE_DPMS:
      *out << " dpms_mode=" << DPMSModeToString(dpms_mode_);
      break;
    case DRM_COMPOSITION_TYPE_MODESET:
      *out << " display_mode=" << display_mode_.h_display() << "x"
           << display_mode_.v_display();
      break;
    default:
      break;
  }

  *out << " timeline[current/squash/pre-comp/done]=" << timeline_current_ << "/"
       << timeline_squash_done_ << "/" << timeline_pre_comp_done_ << "/"
       << timeline_ << "\n";

  *out << "    Layers: count=" << layers_.size() << "\n";
  for (size_t i = 0; i < layers_.size(); i++) {
    const DrmHwcLayer &layer = layers_[i];
    *out << "      [" << i << "] ";

    DumpBuffer(layer.buffer, out);

    *out << " transform=" << TransformToString(layer.transform)
         << " blending[a=" << (int)layer.alpha
         << "]=" << BlendingToString(layer.blending) << " source_crop";
    layer.source_crop.Dump(out);
    *out << " display_frame";
    layer.display_frame.Dump(out);

    *out << "\n";
  }

  *out << "    Planes: count=" << composition_planes_.size() << "\n";
  for (size_t i = 0; i < composition_planes_.size(); i++) {
    const DrmCompositionPlane &comp_plane = composition_planes_[i];
    *out << "      [" << i << "]"
         << " plane=" << (comp_plane.plane ? comp_plane.plane->id() : -1)
         << " source_layer=";
    if (comp_plane.source_layer <= DrmCompositionPlane::kSourceLayerMax) {
      *out << comp_plane.source_layer;
    } else {
      switch (comp_plane.source_layer) {
        case DrmCompositionPlane::kSourceNone:
          *out << "NONE";
          break;
        case DrmCompositionPlane::kSourcePreComp:
          *out << "PRECOMP";
          break;
        case DrmCompositionPlane::kSourceSquash:
          *out << "SQUASH";
          break;
        default:
          *out << "<invalid>";
          break;
      }
    }

    *out << "\n";
  }

  *out << "    Squash Regions: count=" << squash_regions_.size() << "\n";
  for (size_t i = 0; i < squash_regions_.size(); i++) {
    *out << "      [" << i << "] ";
    DumpRegion(squash_regions_[i], out);
    *out << "\n";
  }

  *out << "    Pre-Comp Regions: count=" << pre_comp_regions_.size() << "\n";
  for (size_t i = 0; i < pre_comp_regions_.size(); i++) {
    *out << "      [" << i << "] ";
    DumpRegion(pre_comp_regions_[i], out);
    *out << "\n";
  }
}
}
