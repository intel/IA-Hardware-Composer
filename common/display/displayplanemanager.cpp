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
                                         NativeBufferHandler *buffer_handler,
                                         DisplayPlaneHandler *plane_handler)
    : buffer_handler_(buffer_handler),
      plane_handler_(plane_handler),
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

    primary_plane_ = overlay_planes_.at(0).get();
  }

  return status;
}

bool DisplayPlaneManager::ValidateLayers(
    std::vector<OverlayLayer> &layers,
    std::vector<OverlayLayer *> &cursor_layers, bool pending_modeset,
    bool disable_overlay, DisplayPlaneStateList &composition) {
  CTRACE();
  // Let's mark all planes as free to be used.
  for (auto j = overlay_planes_.begin(); j != overlay_planes_.end(); ++j) {
    j->get()->SetInUse(false);
  }

  std::vector<OverlayPlane> commit_planes;
  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  bool render_layers = false;
  // We start off with Primary plane.
  DisplayPlane *current_plane = primary_plane_;
  OverlayLayer *primary_layer = &(*(layers.begin()));
  commit_planes.emplace_back(OverlayPlane(current_plane, primary_layer));
  composition.emplace_back(current_plane, primary_layer,
                           primary_layer->GetZorder());
  current_plane->SetInUse(true);
  ++layer_begin;
  // Lets ensure we fall back to GPU composition in case
  // primary layer cannot be scanned out directly.
  bool prefer_seperate_plane = primary_layer->PreferSeparatePlane();
  bool force_gpu = (pending_modeset && layers.size() > 1) || disable_overlay;

  if (force_gpu || FallbacktoGPU(current_plane, primary_layer, commit_planes)) {
    render_layers = true;
    if (force_gpu || !prefer_seperate_plane) {
      DisplayPlaneState &last_plane = composition.back();
      for (auto i = layer_begin; i != layer_end; ++i) {
        last_plane.AddLayer(i->GetZorder(), i->GetDisplayFrame(),
                            i->IsCursorLayer());
        i->GPURendered();
      }

      ResetPlaneTarget(last_plane, commit_planes.back());
      // We need to composite primary using GPU, lets use this for
      // all layers in this case.
      return render_layers;
    } else {
      DisplayPlaneState &last_plane = composition.back();
      if (primary_layer->IsVideoLayer())
        last_plane.SetVideoPlane();

      ResetPlaneTarget(last_plane, commit_planes.back());
    }
  }

  // We are just compositing Primary layer and nothing else.
  if (layers.size() == 1) {
    return render_layers;
  }

  if (layer_begin != layer_end) {
    // Handle layers for overlay
    uint32_t index = 0;
    for (auto j = overlay_planes_.begin() + 1; j != overlay_planes_.end();
         ++j) {
      DisplayPlaneState &last_plane = composition.back();
#ifdef DISABLE_CURSOR_PLANE
      if (cursor_plane_ == j->get())
        continue;
#endif

      // Handle remaining overlay planes.
      for (auto i = layer_begin; i != layer_end; ++i) {
        OverlayLayer *layer = &(*(i));
        if (layer->IsCursorLayer()) {
          continue;
        }

        commit_planes.emplace_back(OverlayPlane(j->get(), layer));
        index = i->GetZorder();
        ++layer_begin;
        // If we are able to composite buffer with the given plane, lets use
        // it.
        bool fall_back = FallbacktoGPU(j->get(), layer, commit_planes);
        if (!fall_back || prefer_seperate_plane ||
            layer->PreferSeparatePlane()) {
          composition.emplace_back(j->get(), layer, index);
          j->get()->SetInUse(true);
          if (fall_back) {
            DisplayPlaneState &last_plane = composition.back();
            if (layer->IsVideoLayer()) {
              last_plane.SetVideoPlane();
            }

            ResetPlaneTarget(last_plane, commit_planes.back());
            render_layers = true;
          }

          prefer_seperate_plane = layer->PreferSeparatePlane();
          break;
        } else {
          last_plane.AddLayer(i->GetZorder(), i->GetDisplayFrame(), false);
          commit_planes.pop_back();
        }
      }

      if (last_plane.GetCompositionState() == DisplayPlaneState::State::kRender)
        render_layers = true;
    }

    DisplayPlaneState &last_plane = composition.back();
    // We dont have any additional planes. Pre composite remaining layers
    // to the last overlay plane.
    for (auto i = layer_begin; i != layer_end; ++i) {
      if (i->IsCursorLayer()) {
        continue;
      }

      last_plane.AddLayer(i->GetZorder(), i->GetDisplayFrame(), false);
    }

    if (last_plane.GetCompositionState() == DisplayPlaneState::State::kRender)
      render_layers = true;
  }

  bool render_cursor_layer = ValidateCursorLayer(cursor_layers, composition);
  if (!render_layers) {
    render_layers = render_cursor_layer;
  }

  if (render_layers) {
    ValidateFinalLayers(composition, layers);
    for (DisplayPlaneState &plane : composition) {
      if (plane.GetCompositionState() == DisplayPlaneState::State::kRender) {
        const std::vector<size_t> &source_layers = plane.source_layers();
        size_t layers_size = source_layers.size();
        for (size_t i = 0; i < layers_size; i++) {
          size_t source_index = source_layers.at(i);
          OverlayLayer &layer = layers.at(source_index);
          layer.GPURendered();
        }
      }
    }
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
    if ((cursor_plane_ == plane.plane()) && (!cursor_plane_->IsUniversal()))
      continue;

    last_plane = &plane;
    break;
  }

  return last_plane;
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
  for (DisplayPlaneState &temp : composition) {
    commit_planes.emplace_back(
        OverlayPlane(temp.plane(), temp.GetOverlayLayer()));
  }

  uint32_t total_size = cursor_layers.size();
  bool gpu_rendered = false;
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
      cursor_layer->GPURendered();
      last_plane->AddLayer(cursor_layer->GetZorder(),
                           cursor_layer->GetDisplayFrame(),
                           cursor_layer->IsCursorLayer());
      if (!last_plane->GetOffScreenTarget()) {
        ResetPlaneTarget(*last_plane, commit_planes.back());
      }

      gpu_rendered = true;
      status = true;
    } else {
      if (gpu_rendered) {
        std::vector<CompositionRegion> &comp_regions =
            last_plane->GetCompositionRegion();
        std::vector<CompositionRegion>().swap(comp_regions);
        std::vector<NativeSurface *> &surfaces = last_plane->GetSurfaces();
        size_t size = surfaces.size();
        const HwcRect<int> &current_rect = last_plane->GetDisplayFrame();
        for (size_t i = 0; i < size; i++) {
          surfaces.at(i)->ResetDisplayFrame(current_rect);
        }

        last_plane->SwapSurfaceIfNeeded();
        gpu_rendered = false;
      }
      composition.emplace_back(plane, cursor_layer, cursor_layer->GetZorder());
      plane->SetInUse(true);
      last_plane = GetLastUsedOverlay(composition);
    }

    cursor_index++;
  }

  // We dont have any additional planes. Pre composite remaining cursor layers
  // to the last overlay plane.
  for (uint32_t i = cursor_index; i < total_size; i++) {
    OverlayLayer *cursor_layer = cursor_layers.at(i);
    last_plane->AddLayer(cursor_layer->GetZorder(),
                         cursor_layer->GetDisplayFrame(), true);
    cursor_layer->GPURendered();
    gpu_rendered = true;
    status = true;
  }

  if (gpu_rendered) {
    if (!last_plane->GetOffScreenTarget()) {
      SetOffScreenPlaneTarget(*last_plane);
    }

    last_plane->SwapSurfaceIfNeeded();
    std::vector<CompositionRegion> &comp_regions =
        last_plane->GetCompositionRegion();
    std::vector<CompositionRegion>().swap(comp_regions);
    std::vector<NativeSurface *> &surfaces = last_plane->GetSurfaces();
    size_t size = surfaces.size();
    const HwcRect<int> &current_rect = last_plane->GetDisplayFrame();
    for (size_t i = 0; i < size; i++) {
      surfaces.at(i)->ResetDisplayFrame(current_rect);
    }
  }

  return status;
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

void DisplayPlaneManager::SetOffScreenCursorPlaneTarget(
    DisplayPlaneState &plane, uint32_t width, uint32_t height) {
  NativeSurface *surface = NULL;
  uint32_t preferred_format = plane.plane()->GetPreferredFormat();
  for (auto &fb : cursor_surfaces_) {
    if (!fb->InUse()) {
      uint32_t surface_format = fb->GetLayer()->GetBuffer()->GetFormat();
      if (preferred_format == surface_format) {
        surface = fb.get();
        break;
      }
    }
  }

  if (!surface) {
    NativeSurface *new_surface = Create3DBuffer(width, height);
    new_surface->Init(buffer_handler_, preferred_format, true);
    cursor_surfaces_.emplace_back(std::move(new_surface));
    surface = cursor_surfaces_.back().get();
  }

  surface->SetPlaneTarget(plane, gpu_fd_);
  plane.SetOffScreenTarget(surface);
  plane.ForceGPURendering();
}

void DisplayPlaneManager::ReleaseAllOffScreenTargets() {
  CTRACE();
  std::vector<std::unique_ptr<NativeSurface>>().swap(surfaces_);
  std::vector<std::unique_ptr<NativeSurface>>().swap(cursor_surfaces_);
}

void DisplayPlaneManager::ReleaseFreeOffScreenTargets() {
  std::vector<std::unique_ptr<NativeSurface>> surfaces;
  std::vector<std::unique_ptr<NativeSurface>> cursor_surfaces;
  for (auto &fb : surfaces_) {
    if (fb->InUse()) {
      surfaces.emplace_back(fb.release());
    }
  }

  for (auto &cursor_fb : cursor_surfaces_) {
    if (cursor_fb->InUse()) {
      cursor_surfaces.emplace_back(cursor_fb.release());
    }
  }

  surfaces.swap(surfaces_);
  cursor_surfaces.swap(cursor_surfaces_);
}

void DisplayPlaneManager::EnsureOffScreenTarget(DisplayPlaneState &plane) {
  NativeSurface *surface = NULL;
  bool video_separate = plane.IsVideoPlane();
  uint32_t preferred_format = 0;
  if (video_separate) {
    preferred_format = plane.plane()->GetPreferredVideoFormat();
  } else {
    preferred_format = plane.plane()->GetPreferredFormat();
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
    } else {
      new_surface = Create3DBuffer(width_, height_);
    }

    new_surface->Init(buffer_handler_, preferred_format);
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
    if (plane.GetCompositionState() == DisplayPlaneState::State::kRender &&
        !plane.GetOffScreenTarget()) {
      EnsureOffScreenTarget(plane);
    }

    commit_planes.emplace_back(
        OverlayPlane(plane.plane(), plane.GetOverlayLayer()));
  }

  // If this combination fails just fall back to 3D for all layers.
  if (!plane_handler_->TestCommit(commit_planes)) {
    // We start off with Primary plane.
    DisplayPlane *current_plane = primary_plane_;
    for (DisplayPlaneState &plane : composition) {
      if (plane.GetCompositionState() == DisplayPlaneState::State::kRender) {
        plane.GetOffScreenTarget()->SetInUse(false);
      }
    }

    DisplayPlaneStateList().swap(composition);
    auto layer_begin = layers.begin();
    OverlayLayer *primary_layer = &(*(layer_begin));
    commit_planes.emplace_back(OverlayPlane(current_plane, primary_layer));
    composition.emplace_back(current_plane, primary_layer,
                             primary_layer->GetZorder());
    current_plane->SetInUse(true);
    DisplayPlaneState &last_plane = composition.back();
    last_plane.ForceGPURendering();
    ++layer_begin;

    for (auto i = layer_begin; i != layers.end(); ++i) {
      last_plane.AddLayer(i->GetZorder(), i->GetDisplayFrame(),
                          i->IsCursorLayer());
    }

    EnsureOffScreenTarget(last_plane);
    ReleaseFreeOffScreenTargets();
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
  return primary_plane_->IsSupportedFormat(format);
}

}  // namespace hwcomposer
