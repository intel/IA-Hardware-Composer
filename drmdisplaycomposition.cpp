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

int DrmDisplayComposition::SetLayers(DrmHwcLayer *layers, size_t num_layers,
                                     bool geometry_changed) {
  if (!validate_composition_type(DRM_COMPOSITION_TYPE_FRAME))
    return -EINVAL;

  geometry_changed_ = geometry_changed;

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
  composition_planes_.emplace_back(DrmCompositionPlaneType::kDisable, plane,
                                   crtc_);
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

void DrmDisplayComposition::EmplaceCompositionPlane(
    DrmCompositionPlaneType type, std::vector<DrmPlane *> *primary_planes,
    std::vector<DrmPlane *> *overlay_planes) {
  DrmPlane *plane = TakePlane(crtc_, primary_planes, overlay_planes);
  if (plane == NULL) {
    ALOGE(
        "Failed to add composition plane because there are no planes "
        "remaining");
    return;
  }
  composition_planes_.emplace_back(type, plane, crtc_);
}

void DrmDisplayComposition::EmplaceCompositionPlane(
    size_t source_layer, std::vector<DrmPlane *> *primary_planes,
    std::vector<DrmPlane *> *overlay_planes) {
  DrmPlane *plane = TakePlane(crtc_, primary_planes, overlay_planes);
  if (plane == NULL) {
    ALOGE(
        "Failed to add composition plane because there are no planes "
        "remaining");
    return;
  }
  composition_planes_.emplace_back(DrmCompositionPlaneType::kLayer, plane,
                                   crtc_, source_layer);
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

static void SeparateLayers(DrmHwcLayer *layers, size_t *used_layers,
                           size_t num_used_layers,
                           size_t *protected_layers,
                           size_t num_protected_layers,
                           DrmHwcRect<int> *exclude_rects,
                           size_t num_exclude_rects,
                           std::vector<DrmCompositionRegion> &regions) {
  if (num_used_layers > 64) {
    ALOGE("Failed to separate layers because there are more than 64");
    return;
  }

  // Index at which the actual layers begin
  size_t layer_offset = num_exclude_rects + num_protected_layers;

  if (num_used_layers + layer_offset > 64) {
    ALOGW(
        "Exclusion rectangles are being truncated to make the rectangle count "
        "fit into 64");
    num_exclude_rects = 64 - num_used_layers - num_protected_layers;
  }

  // We inject all the exclude rects into the rects list. Any resulting rect
  // that includes ANY of the first num_exclude_rects is rejected. After the
  // exclude rects, we add the protected layers. The rects that intersect with
  // the protected layer will be inspected and only those which are above the
  // protected layer will be included in the composition regions.
  std::vector<DrmHwcRect<int>> layer_rects(num_used_layers + layer_offset);
  std::copy(exclude_rects, exclude_rects + num_exclude_rects,
            layer_rects.begin());
  std::transform(
      protected_layers, protected_layers + num_protected_layers,
      layer_rects.begin() + num_exclude_rects,
      [=](size_t layer_index) { return layers[layer_index].display_frame; });
  std::transform(
      used_layers, used_layers + num_used_layers,
      layer_rects.begin() + layer_offset,
      [=](size_t layer_index) { return layers[layer_index].display_frame; });

  std::vector<separate_rects::RectSet<uint64_t, int>> separate_regions;
  separate_rects::separate_rects_64(layer_rects, &separate_regions);
  uint64_t exclude_mask = ((uint64_t)1 << num_exclude_rects) - 1;
  uint64_t protected_mask = (((uint64_t)1 << num_protected_layers) - 1) <<
                            num_exclude_rects;

  for (separate_rects::RectSet<uint64_t, int> &region : separate_regions) {
    if (region.id_set.getBits() & exclude_mask)
      continue;

    // If a rect intersects a protected layer, we need to remove the layers
    // from the composition region which appear *below* the protected layer.
    // This effectively punches a hole through the composition layer such
    // that the protected layer can be placed below the composition and not
    // be occluded by things like the background.
    uint64_t protected_intersect = region.id_set.getBits() & protected_mask;
    for (size_t i = 0; protected_intersect && i < num_protected_layers; ++i) {
      // Only exclude layers if they intersect this particular protected layer
      if (!(protected_intersect & (1 << (i + num_exclude_rects))))
        continue;

      for (size_t j = 0; j < num_used_layers; ++j) {
        if (used_layers[j] < protected_layers[i])
          region.id_set.subtract(j + layer_offset);
      }
    }
    if (!(region.id_set.getBits() >> layer_offset))
      continue;

    regions.emplace_back(DrmCompositionRegion{
        region.rect,
        SetBitsToVector(region.id_set.getBits() >> layer_offset, used_layers)});
  }
}

int DrmDisplayComposition::CreateAndAssignReleaseFences() {
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
    if (plane.type() == DrmCompositionPlaneType::kLayer) {
      for (auto i : plane.source_layers()) {
        DrmHwcLayer *source_layer = &layers_[i];
        comp_layers.emplace(source_layer);
        pre_comp_layers.erase(source_layer);
      }
    }
  }

  for (DrmHwcLayer *layer : squash_layers) {
    if (!layer->release_fence)
      continue;
    int ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0)
      return ret;
  }
  timeline_squash_done_ = timeline_;

  for (DrmHwcLayer *layer : pre_comp_layers) {
    if (!layer->release_fence)
      continue;
    int ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0)
      return ret;
  }
  timeline_pre_comp_done_ = timeline_;

  for (DrmHwcLayer *layer : comp_layers) {
    if (!layer->release_fence)
      continue;
    int ret = layer->release_fence.Set(CreateNextTimelineFence());
    if (ret < 0)
      return ret;
  }

  return 0;
}

int DrmDisplayComposition::Plan(SquashState *squash,
                                std::vector<DrmPlane *> *primary_planes,
                                std::vector<DrmPlane *> *overlay_planes) {
  if (type_ != DRM_COMPOSITION_TYPE_FRAME)
    return 0;

  size_t planes_can_use =
      CountUsablePlanes(crtc_, primary_planes, overlay_planes);
  if (planes_can_use == 0) {
    ALOGE("Display %d has no usable planes", crtc_->display());
    return -ENODEV;
  }

  bool use_squash_framebuffer = false;
  // Used to determine which layers were entirely squashed
  std::vector<int> layer_squash_area(layers_.size(), 0);
  // Used to avoid rerendering regions that were squashed
  std::vector<DrmHwcRect<int>> exclude_rects;
  if (squash != NULL && planes_can_use >= 3) {
    if (geometry_changed_) {
      squash->Init(layers_.data(), layers_.size());
    } else {
      std::vector<bool> changed_regions;
      squash->GenerateHistory(layers_.data(), layers_.size(), changed_regions);

      std::vector<bool> stable_regions;
      squash->StableRegionsWithMarginalHistory(changed_regions, stable_regions);

      // Only if SOME region is stable
      use_squash_framebuffer =
          std::find(stable_regions.begin(), stable_regions.end(), true) !=
          stable_regions.end();

      squash->RecordHistory(layers_.data(), layers_.size(), changed_regions);

      // Changes in which regions are squashed triggers a rerender via
      // squash_regions.
      bool render_squash = squash->RecordAndCompareSquashed(stable_regions);

      for (size_t region_index = 0; region_index < stable_regions.size();
           region_index++) {
        const SquashState::Region &region = squash->regions()[region_index];
        if (!stable_regions[region_index])
          continue;

        exclude_rects.emplace_back(region.rect);

        if (render_squash) {
          squash_regions_.emplace_back();
          squash_regions_.back().frame = region.rect;
        }

        int frame_area = region.rect.area();
        // Source layers are sorted front to back i.e. top layer has lowest
        // index.
        for (size_t layer_index = layers_.size();
             layer_index-- > 0;  // Yes, I double checked this
             /* See condition */) {
          if (!region.layer_refs[layer_index])
            continue;
          layer_squash_area[layer_index] += frame_area;
          if (render_squash)
            squash_regions_.back().source_layers.push_back(layer_index);
        }
      }
    }
  }

  // All protected layers get first usage of planes
  std::vector<size_t> layers_remaining;
  std::vector<size_t> protected_layers;
  for (size_t layer_index = 0; layer_index < layers_.size(); layer_index++) {
    if (!layers_[layer_index].protected_usage() || planes_can_use == 0) {
      layers_remaining.push_back(layer_index);
      continue;
    }
    protected_layers.push_back(layer_index);
    planes_can_use--;
  }

  if (planes_can_use == 0 && layers_remaining.size() > 0) {
    for (auto i : protected_layers)
      EmplaceCompositionPlane(i, primary_planes, overlay_planes);

    ALOGE("Protected layers consumed all hardware planes");
    return CreateAndAssignReleaseFences();
  }

  std::vector<size_t> layers_remaining_if_squash;
  for (size_t layer_index : layers_remaining) {
    if (layer_squash_area[layer_index] <
        layers_[layer_index].display_frame.area())
      layers_remaining_if_squash.push_back(layer_index);
  }

  if (use_squash_framebuffer) {
    if (planes_can_use > 1 || layers_remaining_if_squash.size() == 0) {
      layers_remaining = std::move(layers_remaining_if_squash);
      planes_can_use--;  // Reserve plane for squashing
    } else {
      use_squash_framebuffer = false;  // The squash buffer is still rendered
    }
  }

  if (layers_remaining.size() > planes_can_use)
    planes_can_use--;  // Reserve one for pre-compositing

  // Whatever planes that are not reserved get assigned a layer
  size_t last_hw_comp_layer = 0;
  size_t protected_idx = 0;
  while(last_hw_comp_layer < layers_remaining.size() && planes_can_use > 0) {
    size_t idx = layers_remaining[last_hw_comp_layer];

    // Put the protected layers into the composition at the right place. We've
    // already reserved them by decrementing planes_can_use, so no need to do
    // that again.
    if (protected_idx < protected_layers.size() &&
        idx > protected_layers[protected_idx]) {
      EmplaceCompositionPlane(protected_layers[protected_idx], primary_planes,
                              overlay_planes);
      protected_idx++;
      continue;
    }

    EmplaceCompositionPlane(layers_remaining[last_hw_comp_layer],
                            primary_planes, overlay_planes);
    last_hw_comp_layer++;
    planes_can_use--;
  }

  layers_remaining.erase(layers_remaining.begin(),
                         layers_remaining.begin() + last_hw_comp_layer);

  // Enqueue the rest of the protected layers (if any) between the hw composited
  // overlay layers and the squash/precomp layers.
  for (size_t i = protected_idx; i < protected_layers.size(); ++i)
    EmplaceCompositionPlane(protected_layers[i], primary_planes,
                            overlay_planes);

  if (layers_remaining.size() > 0) {
    EmplaceCompositionPlane(DrmCompositionPlaneType::kPrecomp, primary_planes,
                            overlay_planes);
    SeparateLayers(layers_.data(), layers_remaining.data(),
                   layers_remaining.size(), protected_layers.data(),
                   protected_layers.size(), exclude_rects.data(),
                   exclude_rects.size(), pre_comp_regions_);
  }

  if (use_squash_framebuffer) {
    EmplaceCompositionPlane(DrmCompositionPlaneType::kSquash, primary_planes,
                            overlay_planes);
  }

  return CreateAndAssignReleaseFences();
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

static void DumpTransform(uint32_t transform, std::ostringstream *out) {
  *out << "[";

  if (transform == 0)
    *out << "IDENTITY";

  bool separator = false;
  if (transform & DrmHwcTransform::kFlipH) {
    *out << "FLIPH";
    separator = true;
  }
  if (transform & DrmHwcTransform::kFlipV) {
    if (separator)
      *out << "|";
    *out << "FLIPV";
    separator = true;
  }
  if (transform & DrmHwcTransform::kRotate90) {
    if (separator)
      *out << "|";
    *out << "ROTATE90";
    separator = true;
  }
  if (transform & DrmHwcTransform::kRotate180) {
    if (separator)
      *out << "|";
    *out << "ROTATE180";
    separator = true;
  }
  if (transform & DrmHwcTransform::kRotate270) {
    if (separator)
      *out << "|";
    *out << "ROTATE270";
    separator = true;
  }

  uint32_t valid_bits = DrmHwcTransform::kFlipH | DrmHwcTransform::kFlipH |
                        DrmHwcTransform::kRotate90 |
                        DrmHwcTransform::kRotate180 |
                        DrmHwcTransform::kRotate270;
  if (transform & ~valid_bits) {
    if (separator)
      *out << "|";
    *out << "INVALID";
  }
  *out << "]";
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

    if (layer.protected_usage())
      *out << " protected";

    *out << " transform=";
    DumpTransform(layer.transform, out);
    *out << " blending[a=" << (int)layer.alpha
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
         << " plane=" << (comp_plane.plane() ? comp_plane.plane()->id() : -1)
         << " type=";
    switch (comp_plane.type()) {
      case DrmCompositionPlaneType::kDisable:
        *out << "DISABLE";
        break;
      case DrmCompositionPlaneType::kLayer:
        *out << "LAYER";
        break;
      case DrmCompositionPlaneType::kPrecomp:
        *out << "PRECOMP";
        break;
      case DrmCompositionPlaneType::kSquash:
        *out << "SQUASH";
        break;
      default:
        *out << "<invalid>";
        break;
    }

    *out << " source_layer=";
    for (auto i : comp_plane.source_layers()) {
      *out << i << " ";
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
