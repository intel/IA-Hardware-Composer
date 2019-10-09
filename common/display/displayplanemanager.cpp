/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <algorithm>

#include "displayplanemanager.h"

#include "displayplane.h"
#include "drm/drmplane.h"
#include "factory.h"
#include "hwctrace.h"
#include "nativesurface.h"
#include "overlaylayer.h"

#include "hwcutils.h"

namespace hwcomposer {

DisplayPlaneManager::DisplayPlaneManager(DisplayPlaneHandler *plane_handler,
                                         ResourceManager *resource_manager)
    : plane_handler_(plane_handler),
      resource_manager_(resource_manager),
      cursor_plane_(nullptr),
      width_(0),
      height_(0),
      total_overlays_(0),
      display_transform_(kIdentity),
      release_surfaces_(false) {
}

DisplayPlaneManager::~DisplayPlaneManager() {
}

void DisplayPlaneManager::ResizeOverlays() {
  if (!overlay_planes_.empty()) {
    total_overlays_ = overlay_planes_.size();
    if (total_overlays_ > 1) {
      cursor_plane_ = overlay_planes_.back().get();
      // If this is a universal plane, let's not restrict it to
      // cursor usage only.
      if (cursor_plane_->IsUniversal()) {
        cursor_plane_ = NULL;
      } else {
        total_overlays_--;
      }
    }
  }
  IPLANERESERVEDTRACE(
      "ResizeOverlays, overlay_planes_.size: %d, total_overlays_: %d, "
      "cursor_plane_ is NULL?: %d",
      overlay_planes_.size(), total_overlays_, cursor_plane_ == NULL);
}

bool DisplayPlaneManager::Initialize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  bool status = plane_handler_->PopulatePlanes(overlay_planes_);
  ResizeOverlays();
  return status;
}

void DisplayPlaneManager::ResetPlanes(drmModeAtomicReqPtr pset) {
  for (auto j = overlay_planes_.begin(); j < overlay_planes_.end(); j++) {
    if (!j->get()->InUse()) {
      DrmPlane *drmplane = (DrmPlane *)(j->get());
      drmplane->Disable(pset);
    }
  }
}

bool DisplayPlaneManager::ValidateLayers(
    std::vector<OverlayLayer> &layers, int add_index, bool disable_overlay,
    DisplayPlaneStateList &composition,
    DisplayPlaneStateList &previous_composition,
    std::vector<NativeSurface *> &mark_later) {
  CTRACE();

  size_t video_layers = 0;
  if (total_overlays_ == 1)
    add_index = 0;
  for (size_t lindex = add_index; lindex < layers.size(); lindex++) {
    if (layers[lindex].IsVideoLayer())
      video_layers++;
  }

  // In case we are forcing GPU composition for all layers and using a single
  // plane. or only 1 plane is available for more than 1 layers
  if (disable_overlay || (total_overlays_ == 1 && layers.size() > 1)) {
    if (!video_layers) {
      ISURFACETRACE("Forcing GPU For all layers %d %d %d %d \n",
                    disable_overlay, composition.empty(), add_index <= 0,
                    layers.size());
      ForceGpuForAllLayers(composition, layers, mark_later, false);
    } else {
      ISURFACETRACE("Forcing VPP For all layers %d %d %d %d \n",
                    disable_overlay, composition.empty(), add_index <= 0,
                    layers.size());
      ForceVppForAllLayers(composition, layers, add_index, mark_later, false);
    }

    return true;
  }

  auto overlay_begin = overlay_planes_.begin();
  if (!composition.empty()) {
    overlay_begin = overlay_planes_.begin() + composition.size();
  }

  // Let's mark all planes as free to be used.
  for (auto j = overlay_begin; j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  size_t avail_planes = overlay_planes_.size() - composition.size();
  if (!(overlay_planes_[overlay_planes_.size() - 1]->IsUniversal()))
    avail_planes--;
  // If video layers is more than available planes
  // We are going to force all the layers bo be composited by VA path
  // cursor layer should not be handle by VPP
  if (video_layers >= avail_planes && video_layers > 0) {
    ForceVppForAllLayers(composition, layers, add_index, mark_later, false);
    return true;
  }

  std::vector<OverlayLayer *> cursor_layers;
  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  bool validate_final_layers = false;
  bool test_commit_done = false;
  OverlayLayer *previous_layer = NULL;

  if (add_index > 0) {
    layer_begin = layers.begin() + add_index;
  }

  if (layer_begin != layer_end) {
    auto overlay_end = overlay_planes_.end();
    if (cursor_plane_ && overlay_planes_.size() > 1) {
      overlay_end = overlay_planes_.end() - 1;
    }

    // Handle layers for overlays.
    auto j = overlay_begin;

    while (j <= overlay_end) {
      if (previous_layer && !composition.empty()) {
        DisplayPlaneState &last_plane = composition.back();
        if (last_plane.NeedsOffScreenComposition()) {
          ValidateForDisplayScaling(composition.back(), composition);
        }
      }

      // Let's break in case we have already mapped all our
      // layers.
      if (layer_begin == layer_end)
        break;

      // Handle remaining overlay planes.
      for (auto i = layer_begin; i != layer_end; ++i) {
        OverlayLayer *layer = &(*(i));
        ++layer_begin;
        DisplayPlane *plane = NULL;
        bool plane_index_moved = false;
        if (j < overlay_end) {
          plane = j->get();
          j++;
          plane_index_moved = true;
        } else if (j > overlay_begin) {
          plane = (j - 1)->get();
        }
        // Ignore cursor layer as it will handled separately.
        if (layer->IsCursorLayer() && cursor_plane_ != NULL) {
          cursor_layers.emplace_back(layer);
          if (plane_index_moved) {
            j--;
            plane = j->get();
          }
          continue;
        }

        bool prefer_seperate_plane = layer->PreferSeparatePlane();
        if (!prefer_seperate_plane && previous_layer) {
          prefer_seperate_plane = previous_layer->PreferSeparatePlane();
        }

        // Previous layer should not be used anywhere below, so can be
        // safely reset to current layer.
        previous_layer = layer;

        // No planes, need to Squash non video planes
        // No need to do squash, if only 1 overlay is available.
        if (j == overlay_end && total_overlays_ > 1) {
          bool needsquash =
              composition.back().IsVideoPlane() && (layer_begin != layer_end);
          if (!needsquash) {
            auto temp_iter = layer_begin - 1;
            while (temp_iter != layer_end) {
              if ((*temp_iter).IsVideoLayer()) {
                needsquash = true;
                break;
              }
              temp_iter++;
            }
          }
          if (needsquash) {
            // squash no video plane and return the
            ITRACE("ValidateLayers Squash non video planes need");
            size_t squashed_planes = SquashNonVideoPlanes(
                layers, composition, mark_later, &validate_final_layers);
            j -= squashed_planes;
            if (squashed_planes && j > overlay_begin)
              plane = (j - 1)->get();
          }
        }

        if (j < overlay_end || plane_index_moved) {
          // Separate plane added
          composition.emplace_back(plane, layer, this);
          DisplayPlaneState &last_plane = composition.back();
          ISURFACETRACE(
              "Added Layer[%d] into separate plane[%d](NotInUse): %d %d "
              "validate_final_layers: %d  \n",
              layer->GetZorder(), last_plane.GetDisplayPlane()->id(),
              layer->GetZorder(), composition.size(), validate_final_layers);

          // If we are able to composite buffer with the given plane, lets use
          // it.
          bool fall_back = false;
          if(plane) 
            fall_back = FallbacktoGPU(plane, layer, composition);
          test_commit_done = true;
          if (fall_back) {
            ISURFACETRACE(
                "Force GPU rander the plane[%d], for the layer[%d] isVideo: "
                "%d, isSolidColor: %d, alpha: %d",
                last_plane.GetDisplayPlane()->id(), layer->GetZorder(),
                layer->IsVideoLayer(), layer->IsSolidColor(),
                layer->GetAlpha());
            last_plane.ForceGPURendering();
          }
        } else {
          // Add to last plane when plane has been used up
          DisplayPlaneState &last_plane = composition.back();
          ISURFACETRACE(
              "Added Layer into last plane(InUse): %d %d "
              "validate_final_layers: %d  \n",
              layer->GetZorder(), composition.size(), validate_final_layers);
          last_plane.AddLayer(layer);
        }

        if (j == overlay_end) {
          bool needsquash =
              composition.back().IsVideoPlane() && (layer_begin != layer_end);
          if (needsquash) {
            while (SquashPlanesAsNeeded(layers, composition, mark_later,
                                        &validate_final_layers)) {
              j--;
            }
          }
        }
      }
    }

    if ((cursor_layers.size() > 0) && cursor_plane_) {
      if (cursor_layers.size() > 1) {
        ETRACE("More than 1 cursor layers found, we don't support it");
      }
      // Will only add one cursor layer, anyway
      composition.emplace_back(cursor_plane_, cursor_layers[0], this);
      bool fall_back =
          FallbacktoGPU(cursor_plane_, cursor_layers[0], composition);
      if (fall_back) {
        composition.pop_back();
        // fallback to GPU compostion for cursor layers
        composition.back().AddLayer(cursor_layers[0]);
      }
    }
  }

  return true;
}

DisplayPlaneState *DisplayPlaneManager::GetLastUsedOverlay(
    DisplayPlaneStateList &composition) {
  CTRACE();

  DisplayPlaneState *last_plane = NULL;
  size_t size = composition.size();
  for (size_t i = size; i > 0; i--) {
    DisplayPlaneState &plane = composition.at(i - 1);
    if (cursor_plane_ && (cursor_plane_ == plane.GetDisplayPlane()) &&
        (!cursor_plane_->IsUniversal()))
      continue;

    last_plane = &plane;
    break;
  }

  return last_plane;
}

void DisplayPlaneManager::ValidateForDisplayTransform(
    DisplayPlaneState &last_plane, const DisplayPlaneStateList &composition) {
  if (display_transform_ != kIdentity) {
    // No need for any check if we are relying on rotation during
    // 3D Composition pass.
    DisplayPlaneState::RotationType original_rotation =
        last_plane.GetRotationType();
    if (last_plane.RevalidationType() &
        DisplayPlaneState::ReValidationType::kRotation) {
      uint32_t validation_done = DisplayPlaneState::ReValidationType::kRotation;
      last_plane.SetRotationType(
          DisplayPlaneState::RotationType::kDisplayRotation, false);
      // Ensure Rotation doesn't impact the results.
      if (FallbacktoGPU(last_plane.GetDisplayPlane(),
                        last_plane.GetOffScreenTarget()->GetLayer(),
                        composition)) {
        last_plane.SetRotationType(
            DisplayPlaneState::RotationType::kGPURotation, false);
      }

      last_plane.RevalidationDone(validation_done);
    }

    if (original_rotation != last_plane.GetRotationType()) {
      last_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
    }
  }
}

void DisplayPlaneManager::ValidateForDownScaling(
    DisplayPlaneState &last_plane, const DisplayPlaneStateList &composition) {
#ifdef ENABLE_DOWNSCALING
  uint32_t original_downscaling_factor = last_plane.GetDownScalingFactor();
  if (last_plane.RevalidationType() &
      DisplayPlaneState::ReValidationType::kDownScaling) {
    last_plane.SetDisplayDownScalingFactor(1, false);
    if (!last_plane.IsUsingPlaneScalar() && last_plane.CanUseGPUDownScaling()) {
      last_plane.SetDisplayDownScalingFactor(4, false);
      if (!plane_handler_->TestCommit(composition)) {
        last_plane.SetDisplayDownScalingFactor(1, false);
      }
    }

    uint32_t validation_done =
        DisplayPlaneState::ReValidationType::kDownScaling;
    last_plane.RevalidationDone(validation_done);
  }

  if (original_downscaling_factor != last_plane.GetDownScalingFactor()) {
    last_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
  }
#else
  HWC_UNUSED(composition);
  HWC_UNUSED(last_plane);
#endif
}

void DisplayPlaneManager::ValidateForDisplayScaling(
    DisplayPlaneState &last_plane, const DisplayPlaneStateList &composition) {
  last_plane.ValidateReValidation();
  if (!(last_plane.RevalidationType() &
        DisplayPlaneState::ReValidationType::kUpScalar)) {
    return;
  }

  last_plane.RevalidationDone(DisplayPlaneState::ReValidationType::kUpScalar);

  bool old_state = last_plane.IsUsingPlaneScalar();
  if (old_state) {
    last_plane.UsePlaneScalar(false, false);
  }

  if (!last_plane.CanUseDisplayUpScaling()) {
    // If we used plane scalar, clear surfaces.
    if (old_state) {
      last_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
    }

    return;
  }

  // TODO: Scalars are limited in HW. Determine scaling ratio
  // which would really benefit vs doing it in GPU side.

  // Display frame and Source rect are different, let's check if
  // we can take advantage of scalars attached to this plane.
  if (last_plane.IsVideoPlane()) {
    last_plane.UsePlaneScalar(false, false);
    return;
  }
  last_plane.UsePlaneScalar(true, false);

  bool fall_back =
      FallbacktoGPU(last_plane.GetDisplayPlane(),
                    last_plane.GetOffScreenTarget()->GetLayer(), composition);
  if (fall_back) {
    last_plane.UsePlaneScalar(false, false);
  }

  if (old_state != last_plane.IsUsingPlaneScalar()) {
    last_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
  }
}

void DisplayPlaneManager::ReleaseUnreservedPlanes(
    std::vector<uint32_t> &reserved_planes) {
  uint32_t plane_index = 0;
  for (std::vector<std::unique_ptr<DisplayPlane>>::iterator iter =
           overlay_planes_.begin();
       iter != overlay_planes_.end();) {
    if (std::find(reserved_planes.begin(), reserved_planes.end(),
                  plane_index) != reserved_planes.end()) {
      IPLANERESERVEDTRACE("Remaining Plane[%d]", plane_index);
      iter++;
    } else {
      IPLANERESERVEDTRACE("Erasing Plane[%d]", plane_index);
      iter = overlay_planes_.erase(iter);
    }
    plane_index++;
  }
  ResizeOverlays();
}

void DisplayPlaneManager::ReleaseAllOffScreenTargets() {
  CTRACE();
  std::vector<std::unique_ptr<NativeSurface>>().swap(surfaces_);
}

void DisplayPlaneManager::ReleaseFreeOffScreenTargets(bool forced) {
  if (!release_surfaces_ && !forced)
    return;
#ifdef SURFACE_RECYCLE_TRACING
  ISURFACERECYCLETRACE(
      "invoking ReleaseFreeOffScreenTargets --forced:%d, "
      "--release_surfaces_:%d, surfaces_.size() = %d",
      forced, release_surfaces_, surfaces_.size());
#endif
  std::vector<std::unique_ptr<NativeSurface>> surfaces;
  for (auto &fb : surfaces_) {
    if (fb->IsOnScreen()) {
      surfaces.emplace_back(fb.release());
    }
  }

  surfaces.swap(surfaces_);
#ifdef SURFACE_RECYCLE_TRACING
  ISURFACERECYCLETRACE(
      "After ReleaseFreeOffScreenTargets surfaces_.size() = %d",
      surfaces_.size());
#endif
  release_surfaces_ = false;
}

void DisplayPlaneManager::SetDisplayTransform(uint32_t transform) {
  display_transform_ = transform;
}

void DisplayPlaneManager::EnsureOffScreenTarget(DisplayPlaneState &plane) {
  NativeSurface *surface = NULL;
  // We only use media formats when video compostion for 1 layer
  int dest_x = plane.GetDisplayFrame().left;
  int dest_w = plane.GetDisplayFrame().right - dest_x;

  bool video_separate =
      plane.IsVideoPlane() && (plane.GetSourceLayers().size() == 1);
  uint32_t preferred_format = 0;
  uint32_t usage = hwcomposer::kLayerNormal;
  if (video_separate && !(dest_w % 2 || dest_x % 2)) {
    preferred_format = plane.GetDisplayPlane()->GetPreferredVideoFormat();
  } else {
    preferred_format = plane.GetDisplayPlane()->GetPreferredFormat();
  }

  uint64_t preferred_modifier =
      plane.GetDisplayPlane()->GetPreferredFormatModifier();
  if (plane.IsVideoPlane())
    preferred_modifier = 0;
  size_t surface_index = 0;
  for (auto &srf : surfaces_) {
    if (srf->GetSurfaceAge() == -1) {
      OverlayBuffer *layer_buffer = srf->GetLayer()->GetBuffer();
      if (!layer_buffer) {
#ifdef SURFACE_RECYCLE_TRACING
        ISURFACERECYCLETRACE(
            "Layer buffer is null, skip surface[%d] for plane[%d]/layer",
            surface_index, plane.GetDisplayPlane()->id());
#endif
        surface_index++;
        continue;
      }
      uint32_t surface_format = layer_buffer->GetFormat();
      if ((preferred_format == surface_format) &&
          (preferred_modifier == srf->GetModifier())) {
#ifdef SURFACE_RECYCLE_TRACING
        ISURFACERECYCLETRACE("Reuse surface[%d] for the plane[%d].",
                             surface_index, plane.GetDisplayPlane()->id());
#endif
        surface = srf.get();
        break;
      }
    }
    surface_index++;
  }

  if (!surface) {
    NativeSurface *new_surface = NULL;
    if (video_separate) {
#ifdef SURFACE_RECYCLE_TRACING
      ISURFACERECYCLETRACE("CreateVideoSurface for plane[%d]",
                           plane.GetDisplayPlane()->id());
#endif
      new_surface = CreateVideoSurface(width_, height_);
      usage = hwcomposer::kLayerVideo;
    } else {
#ifdef SURFACE_RECYCLE_TRACING
      ISURFACERECYCLETRACE("Create3DSurface for plane[%d]",
                           plane.GetDisplayPlane()->id());
#endif
      new_surface = Create3DSurface(width_, height_);
    }

    bool modifer_succeeded = false;
    new_surface->Init(resource_manager_, preferred_format, usage,
                      preferred_modifier, &modifer_succeeded);
    if (video_separate)
      new_surface->GetLayer()->SetVideoLayer(true);

    if (modifer_succeeded) {
      plane.GetDisplayPlane()->PreferredFormatModifierValidated();
    } else {
      plane.GetDisplayPlane()->BlackListPreferredFormatModifier();
    }

    surfaces_.emplace_back(std::move(new_surface));
#ifdef SURFACE_RECYCLE_TRACING
    ISURFACERECYCLETRACE("Add new surface into surfaces_[%d]",
                         surfaces_.size());
#endif
    surface = surfaces_.back().get();
  }

  surface->SetPlaneTarget(plane);
  plane.SetOffScreenTarget(surface);
}

bool DisplayPlaneManager::FallbacktoGPU(
    DisplayPlane *target_plane, OverlayLayer *layer,
    const DisplayPlaneStateList &composition) const {
  // SolidColor can't be scanout directly

  layer->SupportedDisplayComposition(OverlayLayer::kGpu);
  if (layer->IsSolidColor())
    return true;
  // We need video process to apply effects
  // Such as deinterlace, so always fallback to GPU
  if (layer->IsVideoLayer())
    return true;

  if (!target_plane->ValidateLayer(layer)) {
    return true;
  }

  OverlayBuffer *layer_buffer = layer->GetBuffer();
  if (!layer_buffer)
    return true;

  if (layer_buffer->GetFb() == 0) {
    return true;
  }

  // TODO(kalyank): Take relevant factors into consideration to determine if
  // Plane Composition makes sense. i.e. layer size etc
  if (!plane_handler_->TestCommit(composition)) {
    return true;
  }
  layer->SupportedDisplayComposition(OverlayLayer::kAll);
  return false;
}

bool DisplayPlaneManager::CheckPlaneFormat(uint32_t format) {
  return overlay_planes_.at(0)->IsSupportedFormat(format);
}

void DisplayPlaneManager::ForceVppForAllLayers(
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers,
    size_t add_index, std::vector<NativeSurface *> &mark_later,
    bool recycle_resources) {
  auto layer_begin = layers.begin() + add_index;
  // all planes already assigned, let's reset them into one plane VPP
  if (composition.size() >= overlay_planes_.size()) {
    layer_begin = layers.begin();
    for (DisplayPlaneState &plane : composition) {
      MarkSurfacesForRecycling(&plane, mark_later, recycle_resources);
    }
    DisplayPlaneStateList().swap(composition);
    auto overlay_begin = overlay_planes_.begin();
    // Let's mark all planes as free to be used.
    for (auto j = overlay_begin; j < overlay_planes_.end(); ++j) {
      j->get()->SetInUse(false);
    }
  }

  auto layer_end = layers.end();
  OverlayLayer *primary_layer = &(*(layer_begin));
  DisplayPlane *current_plane = overlay_planes_.at(composition.size()).get();
  composition.emplace_back(current_plane, primary_layer, this);
  DisplayPlaneState &last_plane = composition.back();
  layer_begin++;
  ISURFACETRACE("Added layer in ForceVPPForAllLayers: %d \n",
                primary_layer->GetZorder());

  for (auto i = layer_begin; i != layer_end; ++i) {
    ISURFACETRACE("Added layer in ForceVPPForAllLayers: %d \n", i->GetZorder());
    last_plane.AddLayer(&(*(i)));
    i->SetLayerComposition(OverlayLayer::kGpu);
  }
  last_plane.SetVideoPlane(true);
  if (last_plane.NeedsSurfaceAllocation())
    EnsureOffScreenTarget(last_plane);
  current_plane->SetInUse(true);
  // Check for Any display transform to be applied.
  ValidateForDisplayTransform(last_plane, composition);
  // Check for any change to scalar usage.
  ValidateForDisplayScaling(last_plane, composition);
  // Check for Downscaling.
  ValidateForDownScaling(last_plane, composition);
  // Reset andy Scanout validation state.
  uint32_t validation_done = DisplayPlaneState::ReValidationType::kScanout;
  last_plane.RevalidationDone(validation_done);
}

void DisplayPlaneManager::ForceGpuForAllLayers(
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers,
    std::vector<NativeSurface *> &mark_later, bool recycle_resources) {
  // Let's mark all planes as free to be used.
  for (auto j = overlay_planes_.begin(); j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  if (!composition.empty()) {
    for (DisplayPlaneState &plane : composition) {
      MarkSurfacesForRecycling(&plane, mark_later, recycle_resources);
    }
  }

  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  DisplayPlaneStateList().swap(composition);
  OverlayLayer *primary_layer = &(*(layers.begin()));
  DisplayPlane *current_plane = overlay_planes_.at(0).get();

  composition.emplace_back(current_plane, primary_layer, this);
  DisplayPlaneState &last_plane = composition.back();
  layer_begin++;
  ISURFACETRACE("Added layer in ForceGpuForAllLayers: %d \n",
                primary_layer->GetZorder());

  for (auto i = layer_begin; i != layer_end; ++i) {
    ISURFACETRACE("Added layer in ForceGpuForAllLayers: %d \n", i->GetZorder());
    last_plane.AddLayer(&(*(i)));
    i->SetLayerComposition(OverlayLayer::kGpu);
  }

  if (last_plane.NeedsSurfaceAllocation())
    EnsureOffScreenTarget(last_plane);
  current_plane->SetInUse(true);
  // Check for Any display transform to be applied.
  ValidateForDisplayTransform(last_plane, composition);
  // Check for any change to scalar usage.
  ValidateForDisplayScaling(last_plane, composition);
  // Check for Downscaling.
  ValidateForDownScaling(last_plane, composition);
  // Reset andy Scanout validation state.
  uint32_t validation_done = DisplayPlaneState::ReValidationType::kScanout;
  last_plane.RevalidationDone(validation_done);
}

void DisplayPlaneManager::ReleasedSurfaces() {
  release_surfaces_ = true;
}

void DisplayPlaneManager::MarkSurfacesForRecycling(
    DisplayPlaneState *plane, std::vector<NativeSurface *> &mark_later,
    bool recycle_resources, bool reset_plane_surfaces) {
  const std::vector<NativeSurface *> &surfaces = plane->GetSurfaces();
  if (!surfaces.empty()) {
    release_surfaces_ = true;
    size_t size = surfaces.size();
    if (recycle_resources) {
      // Make sure we don't mark current on-screen surface or
      // one in flight. These surfaces will be added as part of
      // mark_later to be recycled later.
      for (uint32_t i = 0; i < size; i++) {
        NativeSurface *surface = surfaces.at(i);
        if (surface->GetSurfaceAge() >= 0 && surface->IsOnScreen()) {
#ifdef SURFACE_RECYCLE_TRACING
          ISURFACERECYCLETRACE(
              "MarkSurfacesForRecycling Reuse/Later surface[%d] plane[%d]", i,
              plane->GetDisplayPlane()->id());
#endif
          mark_later.emplace_back(surface);
        } else {
#ifdef SURFACE_RECYCLE_TRACING
          ISURFACERECYCLETRACE(
              "MarkSurfaces for recycling/SurfaceAge(-1) surface[%d] plane[%d]",
              i, plane->GetDisplayPlane()->id());
#endif
          surface->SetSurfaceAge(-1);
        }
      }
    } else {
      for (uint32_t i = 0; i < size; i++) {
        NativeSurface *surface = surfaces.at(i);
#ifdef SURFACE_RECYCLE_TRACING
        ISURFACERECYCLETRACE(
            "Recycle_resources is false SurfaceAge(-1) surface[%d] plane[%d]",
            i, plane->GetDisplayPlane()->id());
#endif
        surface->SetSurfaceAge(-1);
      }
    }

    if (reset_plane_surfaces)
      plane->ReleaseSurfaces();
  }
}

bool DisplayPlaneManager::ReValidatePlanes(
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers,
    std::vector<NativeSurface *> &mark_later, bool *request_full_validation,
    bool needs_revalidation_checks, bool re_validate_commit) {
  ISURFACETRACE(
      "ReValidatePlanes called needs_revalidation_checks %d re_validate_commit "
      "%d  \n",
      needs_revalidation_checks, re_validate_commit);
  // Let's first check the current combination works.
  *request_full_validation = false;
  bool render = false;
  bool reset_composition_region = false;
  for (DisplayPlaneState &temp : composition) {
    if (temp.Scanout()) {
      continue;
    }
    render = true;
  }

  if (re_validate_commit) {
    // If this combination fails just fall back to full validation.
    if (!plane_handler_->TestCommit(composition)) {
      ISURFACETRACE(
          "ReValidatePlanes Test commit failed. Forcing full validation. \n");
      *request_full_validation = true;
      return render;
    }
  }

  if (!needs_revalidation_checks) {
    return render;
  }

  uint32_t index = 0;

  for (DisplayPlaneState &last_plane : composition) {
    if (!last_plane.NeedsOffScreenComposition()) {
      index++;
      reset_composition_region = false;
      continue;
    }

    if (reset_composition_region) {
      last_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
    }

    reset_composition_region = false;
    uint32_t revalidation_type = last_plane.RevalidationType();

    if (!revalidation_type) {
      render = true;
      index++;
      continue;
    }

    uint32_t validation_done = DisplayPlaneState::ReValidationType::kScanout;
    if (revalidation_type & DisplayPlaneState::ReValidationType::kScanout) {
      const std::vector<size_t> &source_layers = last_plane.GetSourceLayers();
      bool uses_scalar = last_plane.IsUsingPlaneScalar();
      // Store current layer to re-set in case commit fails.
      const OverlayLayer *current_layer = last_plane.GetOverlayLayer();
      OverlayLayer *layer = &(layers.at(source_layers.at(0)));
      last_plane.SetOverlayLayer(layer);
      // Disable GPU Rendering.
      last_plane.DisableGPURendering();
      if (uses_scalar)
        last_plane.UsePlaneScalar(false, false);

      layer->SetLayerComposition(OverlayLayer::kDisplay);

      // If this combination fails just fall back to original state.
      if (FallbacktoGPU(last_plane.GetDisplayPlane(), layer, composition)) {
        // Reset to old state.
        layer->SetLayerComposition(OverlayLayer::kGpu);
        last_plane.SetOverlayLayer(current_layer);
        if (uses_scalar)
          last_plane.UsePlaneScalar(true, false);
      } else {
        ISURFACETRACE("ReValidatePlanes called: moving to scan \n");
        MarkSurfacesForRecycling(&last_plane, mark_later, true);
        last_plane.SetOverlayLayer(layer);
        reset_composition_region = true;
      }
    }

    render = true;
    index++;
    if (revalidation_type & DisplayPlaneState::ReValidationType::kUpScalar) {
      ValidateForDisplayScaling(last_plane, composition);
      validation_done |= DisplayPlaneState::ReValidationType::kUpScalar;
    }

    if (revalidation_type & DisplayPlaneState::ReValidationType::kRotation) {
      validation_done |= DisplayPlaneState::ReValidationType::kRotation;
      // Save old rotation type.
      DisplayPlaneState::RotationType old_type = last_plane.GetRotationType();
      DisplayPlaneState::RotationType new_type = old_type;
      if (old_type == DisplayPlaneState::RotationType::kGPURotation) {
        last_plane.SetRotationType(
            DisplayPlaneState::RotationType::kDisplayRotation, false);
      } else if (re_validate_commit) {
        // We should have already done a full commit check above.
        // As their is no state change we can avoid another test
        // commit here.
        last_plane.RevalidationDone(validation_done);
        continue;
      }

      // Check if we can rotate using Display plane.
      if (last_plane.NeedsSurfaceAllocation()) {
        EnsureOffScreenTarget(last_plane);
      }
      if (FallbacktoGPU(last_plane.GetDisplayPlane(),
                        last_plane.GetOffScreenTarget()->GetLayer(),
                        composition)) {
        new_type = DisplayPlaneState::RotationType::kGPURotation;
      }

      if (old_type != new_type) {
        // Set new rotation type. Clear surfaces in case type has changed.
        last_plane.SetRotationType(new_type, true);
      }
    }

    if (revalidation_type & DisplayPlaneState::ReValidationType::kDownScaling) {
      validation_done |= DisplayPlaneState::ReValidationType::kDownScaling;
      // Make sure we are not handling upscaling.
      if (last_plane.IsUsingPlaneScalar()) {
        ITRACE(
            "We are using upscaling and also trying to validate for "
            "downscaling \n");
        if (last_plane.GetDownScalingFactor() > 1)
          last_plane.SetDisplayDownScalingFactor(1, true);
      } else {
        // Check for Downscaling.
        ValidateForDownScaling(last_plane, composition);
      }
    }

    last_plane.RevalidationDone(validation_done);
  }

  return render;
}

size_t DisplayPlaneManager::SquashNonVideoPlanes(
    const std::vector<OverlayLayer> &layers, DisplayPlaneStateList &composition,
    std::vector<NativeSurface *> &mark_later, bool *validate_final_layers) {
  size_t composition_index = composition.size() - 1;
  size_t squashed_count = 0;

  while (composition_index > 0) {
    DisplayPlaneState &last_plane = composition.at(composition_index);
    DisplayPlaneState &scanout_plane = composition.at(composition_index - 1);

    if (!last_plane.IsVideoPlane() && !scanout_plane.IsVideoPlane()) {
      ISURFACETRACE("Squasing non video planes. \n");
      const std::vector<size_t> &new_layers = last_plane.GetSourceLayers();
      for (const size_t &index : new_layers) {
        scanout_plane.AddLayer(&(layers.at(index)));
      }

      scanout_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
      last_plane.GetDisplayPlane()->SetInUse(false);
      MarkSurfacesForRecycling(&last_plane, mark_later, true);
      size_t top = composition.size() - 1;

      while (top > composition_index) {
        DisplayPlaneState &temp = composition.at(top);
        DisplayPlaneState &temp1 = composition.at(top - 1);
        temp.SetDisplayPlane(temp1.GetDisplayPlane());
        top--;
      }
      composition.erase(composition.begin() + composition_index);
      squashed_count++;

      if (scanout_plane.NeedsSurfaceAllocation()) {
        scanout_plane.ForceGPURendering();
        *validate_final_layers = true;
      }
    }
    composition_index--;
  }

  return squashed_count;
}

bool DisplayPlaneManager::SquashPlanesAsNeeded(
    const std::vector<OverlayLayer> &layers, DisplayPlaneStateList &composition,
    std::vector<NativeSurface *> &mark_later, bool *validate_final_layers) {
  bool status = false;
  if (composition.size() > 1) {
    DisplayPlaneState &last_plane = composition.back();
    DisplayPlaneState &scanout_plane = composition.at(composition.size() - 2);
    ISURFACETRACE(
        "ANALAYZE scanout_plane: scanout_plane.NeedsOffScreenComposition() %d "
        "scanout_plane.IsCursorPlane() %d scanout_plane.IsVideoPlane() %d  \n",
        scanout_plane.NeedsOffScreenComposition(),
        scanout_plane.IsCursorPlane(), scanout_plane.IsVideoPlane());

    ISURFACETRACE(
        "ANALAYZE last_plane: last_plane.NeedsOffScreenComposition() %d "
        "last_plane.IsCursorPlane() %d last_plane.IsVideoPlane() %d  \n",
        last_plane.NeedsOffScreenComposition(), last_plane.IsCursorPlane(),
        last_plane.IsVideoPlane());

    if (!scanout_plane.IsCursorPlane() && !scanout_plane.IsVideoPlane()) {
      ISURFACETRACE("ANALAYZE AnalyseOverlap: %d \n",
                    AnalyseOverlap(scanout_plane.GetDisplayFrame(),
                                   last_plane.GetDisplayFrame()));
      ISURFACETRACE("ANALAYZE Scanout Display Rect %d %d %d %d \n",
                    scanout_plane.GetDisplayFrame().left,
                    scanout_plane.GetDisplayFrame().top,
                    scanout_plane.GetDisplayFrame().right,
                    scanout_plane.GetDisplayFrame().bottom);
      ISURFACETRACE("ANALAYZE Last offscreen plane rect %d %d %d %d \n",
                    last_plane.GetDisplayFrame().left,
                    last_plane.GetDisplayFrame().top,
                    last_plane.GetDisplayFrame().right,
                    last_plane.GetDisplayFrame().bottom);
    }
    const HwcRect<int> &display_frame = scanout_plane.GetDisplayFrame();
    const HwcRect<int> &target_frame = last_plane.GetDisplayFrame();
    if (!scanout_plane.IsCursorPlane() && !scanout_plane.IsVideoPlane() &&
        (AnalyseOverlap(display_frame, target_frame) != kOutside)) {
      ISURFACETRACE("Squasing planes. \n");
      const std::vector<size_t> &new_layers = last_plane.GetSourceLayers();
      for (const size_t &index : new_layers) {
        scanout_plane.AddLayer(&(layers.at(index)));
      }

      scanout_plane.RefreshSurfaces(NativeSurface::kFullClear, true);

      last_plane.GetDisplayPlane()->SetInUse(false);
      MarkSurfacesForRecycling(&last_plane, mark_later, true);
      composition.pop_back();
      status = true;

      DisplayPlaneState &squashed_plane = composition.back();
      if (squashed_plane.NeedsSurfaceAllocation()) {
        squashed_plane.ForceGPURendering();
        *validate_final_layers = true;
      }
    }
  }

  return status;
}

bool DisplayPlaneManager::ForceSeparatePlane(
    const DisplayPlaneState &last_plane, const OverlayLayer *target_layer) {
  if (last_plane.IsVideoPlane() || last_plane.IsCursorPlane())
    return true;
  else {
    if (!target_layer)
      return false;
    const HwcRect<int> &display_frame = last_plane.GetDisplayFrame();
    const HwcRect<int> &layer_frame = target_layer->GetDisplayFrame();
    return (AnalyseOverlap(display_frame, layer_frame) == kOutside);
  }
}

}  // namespace hwcomposer
