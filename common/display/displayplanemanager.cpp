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
    std::vector<OverlayLayer> &layers,
    std::vector<OverlayLayer *> &cursor_layers, bool pending_modeset,
    bool disable_overlay, DisplayPlaneStateList &composition) {
  CTRACE();

  bool force_gpu = disable_overlay || (pending_modeset && (layers.size() > 1));

  // In case we are forcing GPU composition for all layers and using a single
  // plane.
  if (force_gpu) {
    ForceGpuForAllLayers(composition, layers);
    return true;
  }

  // Let's mark all planes as free to be used.
  for (auto j = overlay_planes_.begin(); j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  // Let's reset some of the layer's state.
  size_t size = layers.size();
  for (size_t i = 0; i != size; ++i) {
    OverlayLayer &layer = layers.at(i);
    layer.GPURendered(false);
    layer.UsePlaneScalar(false);
  }

  std::vector<OverlayPlane> commit_planes;
  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  bool render_layers = false;
  OverlayLayer *previous_layer = NULL;

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
    for (auto j = overlay_planes_.begin(); j != overlay_end; ++j) {
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
        // Ignore cursor layer as it will handled separately.
        if (layer->IsCursorLayer()) {
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
        ++layer_begin;
        // If we are able to composite buffer with the given plane, lets use
        // it.
        bool fall_back = FallbacktoGPU(plane, layer, commit_planes);
        if (!fall_back || prefer_seperate_plane) {
          composition.emplace_back(plane, layer, layer->GetZorder());
          plane->SetInUse(true);
          DisplayPlaneState &last_plane = composition.back();
          if (layer->IsVideoLayer()) {
            last_plane.SetVideoPlane();
          }

          if (fall_back) {
            ResetPlaneTarget(last_plane, commit_planes.back());
          }
          break;
        } else {
          if (composition.empty()) {
            // If we are here, it means the layer failed with
            // Primary. Let's force GPU for all layers.
            // FIXME: We should try to use overlay for
            // other layers in this case.
            ForceGpuForAllLayers(composition, layers);
            return true;
          } else {
            commit_planes.pop_back();
            DisplayPlaneState &last_plane = composition.back();
            last_plane.AddLayer(layer);
            if (!last_plane.GetOffScreenTarget()) {
              ResetPlaneTarget(last_plane, commit_planes.back());
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
          previous_layer = NULL;
          continue;
        }

        last_plane.AddLayer(previous_layer);
      }

      if (last_plane.NeedsOffScreenComposition()) {
        if (previous_layer) {
          // In this case we need to fallback to 3Dcomposition till Media
          // backend adds support for multiple layers.
          bool force_buffer = false;
          if (is_video && last_plane.GetSourceLayers().size() > 1 &&
              last_plane.GetOffScreenTarget()) {
            last_plane.ReleaseSurfaces(false);
            force_buffer = true;
          }

          if (!last_plane.GetOffScreenTarget() || force_buffer) {
            ResetPlaneTarget(last_plane, commit_planes.back());
          }

          ValidateForDisplayScaling(composition.back(), commit_planes,
                                    previous_layer);
        }

        render_layers = true;
      }
    }
  }

  bool render_cursor_layer = ValidateCursorLayer(cursor_layers, composition);
  if (!render_layers) {
    render_layers = render_cursor_layer;
  }

  if (render_layers) {
    ValidateFinalLayers(composition, layers);
    for (DisplayPlaneState &plane : composition) {
      if (plane.NeedsOffScreenComposition()) {
        const std::vector<size_t> &source_layers = plane.GetSourceLayers();
        size_t layers_size = source_layers.size();
        bool useplanescalar = plane.IsUsingPlaneScalar();
        for (size_t i = 0; i < layers_size; i++) {
          size_t source_index = source_layers.at(i);
          OverlayLayer &layer = layers.at(source_index);
          layer.GPURendered(true);
          layer.UsePlaneScalar(useplanescalar);
        }
      }
    }
  }

  return render_layers;
}

bool DisplayPlaneManager::ReValidateLayers(std::vector<OverlayLayer> &layers,
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

  bool render_layers = false;
  // If this combination fails just fall back to full validation.
  if (plane_handler_->TestCommit(commit_planes)) {
    *request_full_validation = false;
    for (DisplayPlaneState &plane : composition) {
      const std::vector<size_t> &source_layers = plane.GetSourceLayers();
      size_t layers_size = source_layers.size();
      bool useplanescalar = plane.IsUsingPlaneScalar();
      bool use_gpu = plane.NeedsOffScreenComposition();
      if (use_gpu) {
        render_layers = true;
      }

      for (size_t i = 0; i < layers_size; i++) {
        size_t source_index = source_layers.at(i);
        OverlayLayer &layer = layers.at(source_index);
        layer.GPURendered(use_gpu);
        layer.UsePlaneScalar(useplanescalar);
      }
    }
  } else {
    *request_full_validation = true;
  }

  return render_layers;
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

void DisplayPlaneManager::PreparePlaneForCursor(DisplayPlaneState *plane,
                                                bool reset_buffer) {
  NativeSurface *surface = plane->GetOffScreenTarget();
  if (surface && reset_buffer) {
    surface->SetInUse(false);
  }

  if (!surface || reset_buffer) {
    SetOffScreenPlaneTarget(*plane);
  }

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

bool DisplayPlaneManager::ValidateCursorLayer(
    std::vector<OverlayLayer *> &cursor_layers,
    DisplayPlaneStateList &composition) {
  CTRACE();
  if (cursor_layers.empty()) {
    return false;
  }

  std::vector<OverlayPlane> commit_planes;
  DisplayPlaneState *last_plane = GetLastUsedOverlay(composition);
  bool is_video = last_plane->IsVideoPlane();
  for (DisplayPlaneState &temp : composition) {
    commit_planes.emplace_back(
        OverlayPlane(temp.GetDisplayPlane(), temp.GetOverlayLayer()));
  }

  uint32_t total_size = cursor_layers.size();
  bool status = false;
  uint32_t cursor_index = 0;
  for (auto j = overlay_planes_.rbegin(); j != overlay_planes_.rend(); ++j) {
    if (cursor_index == total_size)
      break;

    DisplayPlane *plane = j->get();
    if (plane->InUse())
      break;

#ifdef DISABLE_CURSOR_PLANE
    if (cursor_plane_ == plane)
      continue;
#endif
    OverlayLayer *cursor_layer = cursor_layers.at(cursor_index);
    commit_planes.emplace_back(OverlayPlane(plane, cursor_layer));
    // Lets ensure we fall back to GPU composition in case
    // cursor layer cannot be scanned out directly.
    if (FallbacktoGPU(plane, cursor_layer, commit_planes)) {
      commit_planes.pop_back();
      cursor_layer->GPURendered(true);
      last_plane->AddLayer(cursor_layer);
      bool reset_overlay = false;
      if (!last_plane->GetOffScreenTarget() || is_video)
        reset_overlay = true;

      PreparePlaneForCursor(last_plane, is_video);

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
      status = true;
    } else {
      composition.emplace_back(plane, cursor_layer, cursor_layer->GetZorder());
      plane->SetInUse(true);
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
    last_plane->AddLayer(cursor_layer);
    cursor_layer->GPURendered(true);
    status = true;
    last_layer = cursor_layer;
  }

  if (last_layer) {
    PreparePlaneForCursor(last_plane, is_video);
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
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers) {
  std::vector<OverlayPlane> commit_planes;
  for (DisplayPlaneState &plane : composition) {
    if (plane.NeedsOffScreenComposition() && !plane.GetOffScreenTarget()) {
      EnsureOffScreenTarget(plane);
    }

    commit_planes.emplace_back(
        OverlayPlane(plane.GetDisplayPlane(), plane.GetOverlayLayer()));
  }

  // If this combination fails just fall back to 3D for all layers.
  if (!plane_handler_->TestCommit(commit_planes)) {
    ForceGpuForAllLayers(composition, layers);
  }
}

bool DisplayPlaneManager::FallbacktoGPU(
    DisplayPlane *target_plane, OverlayLayer *layer,
    const std::vector<OverlayPlane> &commit_planes) const {
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

  return false;
}

bool DisplayPlaneManager::CheckPlaneFormat(uint32_t format) {
  return overlay_planes_.at(0)->IsSupportedFormat(format);
}

void DisplayPlaneManager::ForceGpuForAllLayers(
    DisplayPlaneStateList &composition, std::vector<OverlayLayer> &layers) {
  // Let's mark all planes as free to be used.
  for (auto j = overlay_planes_.begin(); j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  bool free_surfaces = !composition.empty();

  if (free_surfaces) {
    for (DisplayPlaneState &plane : composition) {
      if (plane.GetOffScreenTarget()) {
        plane.GetOffScreenTarget()->SetInUse(false);
      }
    }
  }

  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  DisplayPlaneStateList().swap(composition);
  OverlayLayer *primary_layer = &(*(layers.begin()));
  DisplayPlane *current_plane = overlay_planes_.at(0).get();

  composition.emplace_back(current_plane, primary_layer,
                           primary_layer->GetZorder());
  DisplayPlaneState &last_plane = composition.back();
  last_plane.ForceGPURendering();

  for (auto i = layer_begin; i != layer_end; ++i) {
    last_plane.AddLayer(&(*(i)));
    i->GPURendered(true);
  }

  EnsureOffScreenTarget(last_plane);
  current_plane->SetInUse(true);

  if (free_surfaces) {
    ReleaseFreeOffScreenTargets();
  }
}

}  // namespace hwcomposer
