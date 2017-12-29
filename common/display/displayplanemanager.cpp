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

#include "displayplanemanager.h"

#include "displayplane.h"
#include "factory.h"
#include "hwctrace.h"
#include "nativesurface.h"
#include "overlaylayer.h"

namespace hwcomposer {

DisplayPlaneManager::DisplayPlaneManager(int gpu_fd,
                                         DisplayPlaneHandler *plane_handler,
                                         ResourceManager *resource_manager)
    : plane_handler_(plane_handler),
      resource_manager_(resource_manager),
      width_(0),
      height_(0),
      gpu_fd_(gpu_fd) {
}

DisplayPlaneManager::~DisplayPlaneManager() {
}

bool DisplayPlaneManager::Initialize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  bool status = plane_handler_->PopulatePlanes(overlay_planes_);
  if (!overlay_planes_.empty()) {
    if (overlay_planes_.size() > 1) {
      cursor_plane_ = overlay_planes_.back().get();
      bool needs_cursor_wa = false;
#ifdef DISABLE_CURSOR_PLANE
      needs_cursor_wa = overlay_planes_.size() > 3;
#endif
      // If this is a universal plane, let's not restrict it to
      // cursor usage only.
      if (!needs_cursor_wa && cursor_plane_->IsUniversal()) {
        cursor_plane_ = NULL;
      }
    }
  }

  return status;
}

bool DisplayPlaneManager::ValidateLayers(
    std::vector<OverlayLayer> &layers, int add_index, bool check_plane,
    bool pending_modeset, bool disable_overlay, bool recycle_resources,
    DisplayPlaneStateList &composition,
    DisplayPlaneStateList &previous_composition,
    std::vector<NativeSurface *> &mark_later) {
  CTRACE();
  std::vector<OverlayPlane> commit_planes;

  for (DisplayPlaneState &temp : composition) {
    commit_planes.emplace_back(
        OverlayPlane(temp.GetDisplayPlane(), temp.GetOverlayLayer()));
  }

  if (check_plane && add_index == -1) {
    bool temp;
    // We are only revalidating planes and can avoid full validation.
    return ReValidatePlanes(commit_planes, composition, layers, mark_later,
                            &temp, recycle_resources);
  }

  if (!previous_composition.empty() && add_index <= 0) {
    for (DisplayPlaneState &plane : previous_composition) {
      MarkSurfacesForRecycling(&plane, mark_later, recycle_resources);
    }
  }

  if (!composition.empty() && add_index <= 0) {
    for (DisplayPlaneState &plane : previous_composition) {
      MarkSurfacesForRecycling(&plane, mark_later, recycle_resources);
    }

    DisplayPlaneStateList().swap(composition);
  }

  bool force_gpu = disable_overlay || (pending_modeset && (layers.size() > 1));
#ifdef SURFACE_TRACING
  if (add_index <= 0) {
    ISURFACETRACE("FUll validation Being performed. \n");
  }
#endif

  // In case we are forcing GPU composition for all layers and using a single
  // plane.
  if (force_gpu) {
#ifdef SURFACE_TRACING
    ISURFACETRACE("Forcing GPU For all layers %d %d %d \n", disable_overlay,
                  pending_modeset, layers.size() > 1);
#endif
    ForceGpuForAllLayers(commit_planes, composition, layers, mark_later,
                         recycle_resources);
    return true;
  }

  auto overlay_begin = overlay_planes_.begin();
  if (add_index > 0) {
    overlay_begin = overlay_planes_.begin() + composition.size();
  }

  // Let's mark all planes as free to be used.
  for (auto j = overlay_begin; j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  // Let's reset some of the layer's state.
  if (add_index != -1) {
    size_t size = layers.size();
    for (size_t i = add_index; i != size; ++i) {
      OverlayLayer &layer = layers.at(i);
      layer.SetLayerComposition(OverlayLayer::kAll);
      layer.UsePlaneScalar(false);
    }
  }

  std::vector<OverlayLayer *> cursor_layers;
  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  bool render_layers = false;
  bool validate_final_layers = false;
  OverlayLayer *previous_layer = NULL;

  if (add_index > 0) {
    layer_begin = layers.begin() + add_index;
  }

  if (layer_begin != layer_end) {
    auto overlay_end = overlay_planes_.end();
#ifdef DISABLE_CURSOR_PLANE
    overlay_end = overlay_planes_.end() - 1;
#else
    if (!cursor_plane_->IsUniversal()) {
      overlay_end = overlay_planes_.end() - 1;
    }
#endif

    // Handle layers for overlays.
    for (auto j = overlay_begin; j != overlay_end; ++j) {
      DisplayPlane *plane = j->get();
      if (previous_layer && !composition.empty()) {
        DisplayPlaneState &last_plane = composition.back();
        if (last_plane.NeedsOffScreenComposition()) {
          ValidateForDisplayScaling(composition.back(), commit_planes,
                                    previous_layer);
          render_layers = true;
        }
      }

      // Handle remaining overlay planes.
      for (auto i = layer_begin; i != layer_end; ++i) {
        OverlayLayer *layer = &(*(i));
        ++layer_begin;
        // Ignore cursor layer as it will handled separately.
        if (layer->IsCursorLayer()) {
          cursor_layers.emplace_back(layer);
          continue;
        }

        bool prefer_seperate_plane = layer->PreferSeparatePlane();
        if (!prefer_seperate_plane && previous_layer) {
          prefer_seperate_plane = previous_layer->PreferSeparatePlane();
        }

        // Previous layer should not be used anywhere below, so can be
        // safely reset to current layer.
        previous_layer = layer;

        commit_planes.emplace_back(OverlayPlane(plane, layer));
        // If we are able to composite buffer with the given plane, lets use
        // it.
        bool fall_back = FallbacktoGPU(plane, layer, commit_planes);
        validate_final_layers = false;
        if (!fall_back || prefer_seperate_plane) {
          composition.emplace_back(plane, layer, layer->GetZorder());
          plane->SetInUse(true);
          DisplayPlaneState &last_plane = composition.back();
          if (layer->IsVideoLayer()) {
            last_plane.SetVideoPlane();
          }

          if (fall_back) {
            ResetPlaneTarget(last_plane, commit_planes.back());
            validate_final_layers = true;
          }
          break;
        } else {
          if (composition.empty()) {
            // If we are here, it means the layer failed with
            // Primary. Let's force GPU for all layers.
            // FIXME: We should try to use overlay for
            // other layers in this case.
            ForceGpuForAllLayers(commit_planes, composition, layers, mark_later,
                                 recycle_resources);
            return true;
          } else {
            commit_planes.pop_back();
            DisplayPlaneState &last_plane = composition.back();
#ifdef SURFACE_TRACING
            ISURFACETRACE("Added Layer: %d \n", layer->GetZorder());
#endif
            last_plane.AddLayer(layer);
            if (!last_plane.GetOffScreenTarget()) {
              ResetPlaneTarget(last_plane, commit_planes.back());
              validate_final_layers = true;
            }
          }
        }
      }
    }

    if (layer_begin != layer_end) {
      DisplayPlaneState &last_plane = composition.back();
      bool is_video = last_plane.IsVideoPlane();
      previous_layer = NULL;
      // We dont have any additional planes. Pre composite remaining layers
      // to the last overlay plane.
      for (auto i = layer_begin; i != layer_end; ++i) {
        previous_layer = &(*(i));
        // Ignore cursor layer as it will handled separately.
        if (previous_layer->IsCursorLayer()) {
          cursor_layers.emplace_back(previous_layer);
          previous_layer = NULL;
          continue;
        }
#ifdef SURFACE_TRACING
        ISURFACETRACE("Added Layer: %d \n", previous_layer->GetZorder());
#endif
        last_plane.AddLayer(previous_layer);
      }

      if (last_plane.NeedsOffScreenComposition()) {
        // In this case we need to fallback to 3Dcomposition till Media
        // backend adds support for multiple layers.
        bool force_buffer = false;
        if (is_video && last_plane.GetSourceLayers().size() > 1 &&
            last_plane.GetOffScreenTarget()) {
          MarkSurfacesForRecycling(&last_plane, mark_later, recycle_resources);
          force_buffer = true;
        }

        if (!last_plane.GetOffScreenTarget() || force_buffer) {
          ResetPlaneTarget(last_plane, commit_planes.back());
          validate_final_layers = true;
        }

        if (previous_layer) {
          ValidateForDisplayScaling(composition.back(), commit_planes,
                                    previous_layer);
        }

        render_layers = true;
        commit_planes.back().layer = last_plane.GetOverlayLayer();
      }
    }
  }

  if (!cursor_layers.empty()) {
    bool render_cursor_layer = ValidateCursorLayer(
        commit_planes, cursor_layers, mark_later, composition,
        &validate_final_layers, recycle_resources);
    if (!render_layers) {
      render_layers = render_cursor_layer;
    }
  }

  if (check_plane) {
    // We are only revalidating planes and can avoid full validation.
    bool status =
        ReValidatePlanes(commit_planes, composition, layers, mark_later,
                         &validate_final_layers, recycle_resources);
    if (!render_layers) {
      render_layers = status;
    }
  }

  if (render_layers) {
    if (validate_final_layers) {
      ValidateFinalLayers(commit_planes, composition, layers, mark_later,
                          recycle_resources);
    }

    for (DisplayPlaneState &plane : composition) {
      if (plane.NeedsOffScreenComposition()) {
        plane.RefreshSurfaces(true);
        const std::vector<size_t> &source_layers = plane.GetSourceLayers();
        size_t layers_size = source_layers.size();
        bool useplanescalar = plane.IsUsingPlaneScalar();
        for (size_t i = 0; i < layers_size; i++) {
          size_t source_index = source_layers.at(i);
          OverlayLayer &layer = layers.at(source_index);
          layer.SetLayerComposition(OverlayLayer::kGpu);
          layer.UsePlaneScalar(useplanescalar);
        }
      }
    }
  }

  return render_layers;
}

void DisplayPlaneManager::ReValidateLayers(std::vector<OverlayLayer> &layers,
                                           DisplayPlaneStateList &composition,
                                           bool *request_full_validation) {
  CTRACE();
  std::vector<OverlayPlane> commit_planes;
  for (DisplayPlaneState &temp : composition) {
    commit_planes.emplace_back(
        OverlayPlane(temp.GetDisplayPlane(), temp.GetOverlayLayer()));
    // Check if we can still need/use scalar for this plane.
    if (temp.IsUsingPlaneScalar()) {
      const std::vector<size_t> &source = temp.GetSourceLayers();
      size_t total_layers = source.size();
      ValidateForDisplayScaling(
          temp, commit_planes, &(layers.at(source.at(total_layers - 1))), true);
    }
  }

  // If this combination fails just fall back to full validation.
  if (plane_handler_->TestCommit(commit_planes)) {
    *request_full_validation = false;
  } else {
#ifdef SURFACE_TRACING
    ISURFACETRACE(
        "ReValidateLayers Test commit failed. Forcing full validation. \n");
#endif
    *request_full_validation = true;
  }
}

DisplayPlaneState *DisplayPlaneManager::GetLastUsedOverlay(
    DisplayPlaneStateList &composition) {
  CTRACE();

  DisplayPlaneState *last_plane = NULL;
  size_t size = composition.size();
  for (size_t i = size; i > 0; i--) {
    DisplayPlaneState &plane = composition.at(i - 1);
    if ((cursor_plane_ == plane.GetDisplayPlane()) &&
        (!cursor_plane_->IsUniversal()))
      continue;

    last_plane = &plane;
    break;
  }

  return last_plane;
}

void DisplayPlaneManager::PreparePlaneForCursor(
    DisplayPlaneState *plane, std::vector<NativeSurface *> &mark_later,
    bool *validate_final_layers, bool reset_buffer, bool recycle_resources) {
  NativeSurface *surface = NULL;
  if (reset_buffer) {
    MarkSurfacesForRecycling(plane, mark_later, recycle_resources);
    surface = NULL;
  } else {
    surface = plane->GetOffScreenTarget();
  }

  if (!surface) {
    SetOffScreenPlaneTarget(*plane);
    *validate_final_layers = true;
  } else {
    // If Last frame surface is re-cycled and surfaces are
    // less than 3, make sure we have the offscreen surface
    // which is not in queued to be onscreen yet.
    if (plane->SurfaceRecycled() && (plane->GetSurfaces().size() < 3)) {
      SetOffScreenPlaneTarget(*plane);
    } else {
      plane->SwapSurfaceIfNeeded();
    }

    plane->RefreshSurfaces(true);
  }
}

bool DisplayPlaneManager::ValidateCursorLayer(
    std::vector<OverlayPlane> &commit_planes,
    std::vector<OverlayLayer *> &cursor_layers,
    std::vector<NativeSurface *> &mark_later,
    DisplayPlaneStateList &composition, bool *validate_final_layers,
    bool recycle_resources) {
  CTRACE();
  if (cursor_layers.empty()) {
    return false;
  }

  DisplayPlaneState *last_plane = GetLastUsedOverlay(composition);
  bool is_video = last_plane->IsVideoPlane();

  uint32_t total_size = cursor_layers.size();
  bool status = false;
  uint32_t cursor_index = 0;
  auto overlay_end = overlay_planes_.end();
  auto overlay_begin = overlay_planes_.begin() + composition.size();
#ifdef DISABLE_CURSOR_PLANE
  overlay_end = overlay_planes_.end() - 1;
#endif
  for (auto j = overlay_begin; j != overlay_end; ++j) {
    if (cursor_index == total_size)
      break;

    DisplayPlane *plane = j->get();
    if (plane->InUse()) {
      ETRACE("Trying to use a plane for cursor which is already in use. \n");
    }

    OverlayLayer *cursor_layer = cursor_layers.at(cursor_index);
    commit_planes.emplace_back(OverlayPlane(plane, cursor_layer));
    bool fall_back = true;
    LayerResultCache *cached_plane = NULL;
    if (!results_cache_.empty()) {
      size_t size = results_cache_.size();
      for (size_t i = 0; i < size; i++) {
        LayerResultCache &cache = results_cache_.at(i);
        if (cache.plane_ != plane)
          continue;

        uint32_t layer_transform = cursor_layer->GetPlaneTransform();
        bool cached = false;
        if (cache.last_transform_ == layer_transform) {
          cached = true;
        }

        if (cached) {
          fall_back = false;
          cursor_layer->SupportedDisplayComposition(OverlayLayer::kAll);
          if (cursor_layer->GetBuffer()->GetFb() == 0) {
            if (!cursor_layer->GetBuffer()->CreateFrameBuffer(gpu_fd_)) {
              fall_back = true;
            }
          }

          if (!fall_back) {
            *validate_final_layers = false;
          }
        }

        if (!cached) {
          if (cache.last_failed_transform_ == layer_transform) {
            cached = true;
          }

          if (cached) {
            fall_back = true;
            status = true;
            cursor_layer->SupportedDisplayComposition(OverlayLayer::kGpu);
          }
        }

        cached_plane = &(results_cache_.at(i));
        break;
      }
    }

    // We don't have this in cache.
    if (fall_back && !status) {
      fall_back = FallbacktoGPU(plane, cursor_layer, commit_planes);
      if (!cached_plane) {
        results_cache_.emplace_back();
        cached_plane = &(results_cache_.back());
        cached_plane->plane_ = plane;
      }

      if (!fall_back) {
        cached_plane->last_transform_ = cursor_layer->GetPlaneTransform();
        *validate_final_layers = false;
      } else {
        status = true;
        cached_plane->last_failed_transform_ =
            cursor_layer->GetPlaneTransform();
      }
    }

    // Lets ensure we fall back to GPU composition in case
    // cursor layer cannot be scanned out directly.
    if (fall_back && !is_video) {
      commit_planes.pop_back();
      cursor_layer->SetLayerComposition(OverlayLayer::kGpu);
#ifdef SURFACE_TRACING
      ISURFACETRACE("Added CursorLayer: %d \n", cursor_layer->GetZorder());
#endif
      last_plane->AddLayer(cursor_layer);
      bool reset_overlay = false;
      if (!last_plane->GetOffScreenTarget() || is_video)
        reset_overlay = true;

      PreparePlaneForCursor(last_plane, mark_later, validate_final_layers,
                            is_video, recycle_resources);

      if (reset_overlay) {
        // Layer for the plane should have changed, reset commit planes.
        std::vector<OverlayPlane>().swap(commit_planes);
        for (DisplayPlaneState &temp : composition) {
          commit_planes.emplace_back(
              OverlayPlane(temp.GetDisplayPlane(), temp.GetOverlayLayer()));
        }
      }

      ValidateForDisplayScaling(*last_plane, commit_planes, cursor_layer,
                                false);
    } else {
      composition.emplace_back(plane, cursor_layer, cursor_layer->GetZorder());
      plane->SetInUse(true);
      if (fall_back) {
        DisplayPlaneState &temp = composition.back();
        temp.ForceGPURendering();
        SetOffScreenPlaneTarget(temp);
        cursor_layer->SetLayerComposition(OverlayLayer::kGpu);
      } else {
        cursor_layer->SetLayerComposition(OverlayLayer::kDisplay);
      }

      last_plane = GetLastUsedOverlay(composition);
      is_video = last_plane->IsVideoPlane();
    }

    cursor_index++;
  }

  // We dont have any additional planes. Pre composite remaining cursor layers
  // to the last overlay plane.
  OverlayLayer *last_layer = NULL;
  for (uint32_t i = cursor_index; i < total_size; i++) {
    OverlayLayer *cursor_layer = cursor_layers.at(i);
#ifdef SURFACE_TRACING
    ISURFACETRACE("Added CursorLayer: %d \n", cursor_layer->GetZorder());
#endif
    last_plane->AddLayer(cursor_layer);
    cursor_layer->SetLayerComposition(OverlayLayer::kGpu);
    status = true;
    last_layer = cursor_layer;
  }

  if (last_layer) {
    PreparePlaneForCursor(last_plane, mark_later, validate_final_layers,
                          is_video, recycle_resources);
    ValidateForDisplayScaling(*last_plane, commit_planes, last_layer, false);
  }

  return status;
}

void DisplayPlaneManager::ValidateForDisplayScaling(
    DisplayPlaneState &last_plane, std::vector<OverlayPlane> &commit_planes,
    OverlayLayer *current_layer, bool ignore_format) {
  size_t total_layers = last_plane.GetSourceLayers().size();

  if (last_plane.IsUsingPlaneScalar()) {
    last_plane.UsePlaneScalar(false);
    current_layer->UsePlaneScalar(false);
    last_plane.ResetSourceRectToDisplayFrame();
    last_plane.RefreshSurfaces(false);
  }

  // TODO: Handle case where all layers to be compoisted have same scaling
  // ratio.
  // We cannot use plane scaling for Layers with different scaling ratio.
  if (total_layers > 1) {
    return;
  }

  uint32_t display_frame_width = current_layer->GetDisplayFrameWidth();
  uint32_t display_frame_height = current_layer->GetDisplayFrameHeight();
  uint32_t source_crop_width = current_layer->GetSourceCropWidth();
  uint32_t source_crop_height = current_layer->GetSourceCropHeight();
  // Source and Display frame width, height are same and scaling is not needed.
  if ((display_frame_width == source_crop_width) &&
      (display_frame_height == source_crop_height)) {
    return;
  }

  // Case where we are not rotating the layer and format is supported by the
  // plane.
  // If we are here this means the layer cannot be scaled using display, just
  // return.
  if (!ignore_format &&
      (current_layer->GetPlaneTransform() == HWCTransform::kIdentity) &&
      last_plane.GetDisplayPlane()->IsSupportedFormat(
          current_layer->GetBuffer()->GetFormat())) {
    return;
  }

  // Display frame width, height is lesser than Source. Let's downscale
  // it with our compositor backend.
  if ((display_frame_width < source_crop_width) &&
      (display_frame_height < source_crop_height)) {
    return;
  }

  // Display frame height is less. If the cost of upscaling width is less
  // than downscaling height, than return.
  if ((display_frame_width > source_crop_width) &&
      (display_frame_height < source_crop_height)) {
    uint32_t width_cost =
        (display_frame_width - source_crop_width) * display_frame_height;
    uint32_t height_cost =
        (source_crop_height - display_frame_height) * display_frame_width;
    if (height_cost > width_cost) {
      return;
    }
  }

  // Display frame width is less. If the cost of upscaling height is less
  // than downscaling width, than return.
  if ((display_frame_width < source_crop_width) &&
      (display_frame_height > source_crop_height)) {
    uint32_t width_cost =
        (source_crop_width - display_frame_width) * display_frame_height;
    uint32_t height_cost =
        (display_frame_height - source_crop_height) * display_frame_width;
    if (width_cost > height_cost) {
      return;
    }
  }

  // TODO: Scalars are limited in HW. Determine scaling ratio
  // which would really benefit vs doing it in GPU side.

  // Display frame and Source rect are different, let's check if
  // we can take advantage of scalars attached to this plane.
  const HwcRect<float> &crop = current_layer->GetSourceCrop();
  last_plane.SetSourceCrop(crop);
  last_plane.RefreshSurfaces(false);

  OverlayPlane &last_overlay_plane = commit_planes.back();
  last_overlay_plane.layer = last_plane.GetOverlayLayer();

  bool fall_back =
      FallbacktoGPU(last_plane.GetDisplayPlane(),
                    last_plane.GetOffScreenTarget()->GetLayer(), commit_planes);
  if (fall_back) {
    last_plane.ResetSourceRectToDisplayFrame();
    last_plane.RefreshSurfaces(false);
  } else {
    last_plane.UsePlaneScalar(true);
    current_layer->UsePlaneScalar(true);
  }
}

void DisplayPlaneManager::ResetPlaneTarget(DisplayPlaneState &plane,
                                           OverlayPlane &overlay_plane) {
  SetOffScreenPlaneTarget(plane);
  overlay_plane.layer = plane.GetOverlayLayer();
}

void DisplayPlaneManager::SetOffScreenPlaneTarget(DisplayPlaneState &plane) {
  EnsureOffScreenTarget(plane);

  // Case where we have just one layer which needs to be composited using
  // GPU.
  plane.ForceGPURendering();
}

void DisplayPlaneManager::ReleaseAllOffScreenTargets() {
  CTRACE();
  std::vector<std::unique_ptr<NativeSurface>>().swap(surfaces_);
}

void DisplayPlaneManager::ReleaseFreeOffScreenTargets() {
  std::vector<std::unique_ptr<NativeSurface>> surfaces;
  for (auto &fb : surfaces_) {
    if (fb->InUse()) {
      surfaces.emplace_back(fb.release());
    }
  }

  surfaces.swap(surfaces_);
}

void DisplayPlaneManager::EnsureOffScreenTarget(DisplayPlaneState &plane) {
  NativeSurface *surface = NULL;
  bool video_separate = plane.IsVideoPlane();
  uint32_t preferred_format = 0;
  uint32_t usage = hwcomposer::kLayerNormal;
  if (video_separate) {
    preferred_format = plane.GetDisplayPlane()->GetPreferredVideoFormat();
  } else {
    preferred_format = plane.GetDisplayPlane()->GetPreferredFormat();
  }

  for (auto &fb : surfaces_) {
    if (!fb->InUse()) {
      uint32_t surface_format = fb->GetLayer()->GetBuffer()->GetFormat();
      if (preferred_format == surface_format) {
        surface = fb.get();
        break;
      }
    }
  }

  if (!surface) {
    NativeSurface *new_surface = NULL;
    if (video_separate) {
      new_surface = CreateVideoBuffer(width_, height_);
      usage = hwcomposer::kLayerVideo;
    } else {
      new_surface = Create3DBuffer(width_, height_);
    }

    new_surface->Init(resource_manager_, preferred_format, usage);
    surfaces_.emplace_back(std::move(new_surface));
    surface = surfaces_.back().get();
  }

  surface->SetPlaneTarget(plane, gpu_fd_);
  plane.SetOffScreenTarget(surface);
}

void DisplayPlaneManager::ValidateFinalLayers(
    std::vector<OverlayPlane> &commit_planes,
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers,
    std::vector<NativeSurface *> &mark_later, bool recycle_resources) {
  for (DisplayPlaneState &plane : composition) {
    if (plane.NeedsOffScreenComposition() && !plane.GetOffScreenTarget()) {
      EnsureOffScreenTarget(plane);
    }
  }

  // If this combination fails just fall back to 3D for all layers.
  if (!plane_handler_->TestCommit(commit_planes)) {
    ForceGpuForAllLayers(commit_planes, composition, layers, mark_later,
                         recycle_resources);
  }
}

bool DisplayPlaneManager::FallbacktoGPU(
    DisplayPlane *target_plane, OverlayLayer *layer,
    const std::vector<OverlayPlane> &commit_planes) const {
  // For Video, we always want to support Display Composition.
  if (layer->IsVideoLayer()) {
    layer->SupportedDisplayComposition(OverlayLayer::kAll);
  } else {
    layer->SupportedDisplayComposition(OverlayLayer::kGpu);
  }

  if (!target_plane->ValidateLayer(layer))
    return true;

  if (layer->GetBuffer()->GetFb() == 0) {
    if (!layer->GetBuffer()->CreateFrameBuffer(gpu_fd_)) {
      return true;
    }
  }

  // TODO(kalyank): Take relevant factors into consideration to determine if
  // Plane Composition makes sense. i.e. layer size etc
  if (!plane_handler_->TestCommit(commit_planes)) {
    return true;
  }

  layer->SupportedDisplayComposition(OverlayLayer::kAll);
  return false;
}

bool DisplayPlaneManager::CheckPlaneFormat(uint32_t format) {
  return overlay_planes_.at(0)->IsSupportedFormat(format);
}

void DisplayPlaneManager::ForceGpuForAllLayers(
    std::vector<OverlayPlane> &commit_planes,
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers,
    std::vector<NativeSurface *> &mark_later, bool recycle_resources) {
  // Let's mark all planes as free to be used.
  for (auto j = overlay_planes_.begin(); j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  bool free_surfaces = !composition.empty();

  if (free_surfaces) {
    for (DisplayPlaneState &plane : composition) {
      MarkSurfacesForRecycling(&plane, mark_later, recycle_resources);
    }
  }

  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  DisplayPlaneStateList().swap(composition);
  std::vector<OverlayPlane>().swap(commit_planes);
  OverlayLayer *primary_layer = &(*(layers.begin()));
  DisplayPlane *current_plane = overlay_planes_.at(0).get();

  composition.emplace_back(current_plane, primary_layer,
                           primary_layer->GetZorder());
  DisplayPlaneState &last_plane = composition.back();
  last_plane.ForceGPURendering();
  layer_begin++;

  for (auto i = layer_begin; i != layer_end; ++i) {
#ifdef SURFACE_TRACING
    ISURFACETRACE("Added layer in ForceGpuForAllLayers: %d \n", i->GetZorder());
#endif
    last_plane.AddLayer(&(*(i)));
    i->SetLayerComposition(OverlayLayer::kGpu);
  }

  EnsureOffScreenTarget(last_plane);
  current_plane->SetInUse(true);

  if (free_surfaces) {
    ReleaseFreeOffScreenTargets();
  }
}

void DisplayPlaneManager::MarkSurfacesForRecycling(
    DisplayPlaneState *plane, std::vector<NativeSurface *> &mark_later,
    bool recycle_resources) {
  const std::vector<NativeSurface *> &surfaces = plane->GetSurfaces();
  if (!surfaces.empty()) {
    size_t size = surfaces.size();
    // Make sure we don't mark current on-screen surface or
    // one in flight. These surfaces will be added as part of
    // mark_later to be recycled later.
    for (uint32_t i = 0; i < size; i++) {
      NativeSurface *surface = surfaces.at(i);
      bool in_use = false;
      if (!recycle_resources) {
        if (surface->GetSurfaceAge() > 0) {
          in_use = true;
          mark_later.emplace_back(surface);
        }
      }
      surface->SetInUse(in_use);
    }

    plane->ReleaseSurfaces();
  }
}

bool DisplayPlaneManager::ReValidatePlanes(
    std::vector<OverlayPlane> &commit_planes,
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers,
    std::vector<NativeSurface *> &mark_later, bool *validate_final_layers,
    bool recycle_resources) {
#ifdef SURFACE_TRACING
  ISURFACETRACE("ReValidatePlanes called \n");
#endif
  bool render = false;
  uint32_t index = 0;

  for (DisplayPlaneState &last_plane : composition) {
    if (last_plane.IsRevalidationNeeded()) {
      const std::vector<size_t> &source_layers = last_plane.GetSourceLayers();
      // Store current layer to re-set in case commit fails.
      const OverlayLayer *current_layer = last_plane.GetOverlayLayer();
      OverlayLayer *layer = &(layers.at(source_layers.at(0)));
      last_plane.SetOverlayLayer(layer);
      // Disable GPU Rendering.
      last_plane.DisableGPURendering();
      layer->SetLayerComposition(OverlayLayer::kDisplay);

      commit_planes.at(index).layer = last_plane.GetOverlayLayer();

      // If this combination fails just fall back to 3D for all layers.
      if (FallbacktoGPU(last_plane.GetDisplayPlane(), layer, commit_planes)) {
        // Reset to old state.
        last_plane.ForceGPURendering();
        layer->SetLayerComposition(OverlayLayer::kGpu);
        last_plane.SetOverlayLayer(current_layer);
      } else {
#ifdef SURFACE_TRACING
        ISURFACETRACE("ReValidatePlanes called: moving to scan \n");
#endif
        MarkSurfacesForRecycling(&last_plane, mark_later, recycle_resources);
        *validate_final_layers = false;
      }
    }

    if (last_plane.NeedsOffScreenComposition()) {
      render = true;
    }

    last_plane.RevalidationDone();

    index++;
  }

  return render;
}

}  // namespace hwcomposer
