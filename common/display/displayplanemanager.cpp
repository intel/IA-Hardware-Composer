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
  return plane_handler_->PopulatePlanes(primary_plane_, cursor_plane_,
                                        overlay_planes_);
}

bool DisplayPlaneManager::ValidateLayers(std::vector<OverlayLayer> &layers,
                                         bool pending_modeset,
                                         bool disable_overlay,
                                         DisplayPlaneStateList &composition) {
  CTRACE();
  std::vector<OverlayPlane> commit_planes;
  OverlayLayer *cursor_layer = NULL;
  auto layer_begin = layers.begin();
  auto layer_end = layers.end();
  bool render_layers = false;
  // We start off with Primary plane.
  DisplayPlane *current_plane = primary_plane_.get();
  OverlayLayer *primary_layer = &(*(layers.begin()));
  commit_planes.emplace_back(OverlayPlane(current_plane, primary_layer));
  composition.emplace_back(current_plane, primary_layer,
                           primary_layer->GetIndex());
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
        last_plane.AddLayer(i->GetIndex(), i->GetDisplayFrame());
      }

      ResetPlaneTarget(last_plane, commit_planes.back());
      // We need to composite primary using GPU, lets use this for
      // all layers in this case.
      return render_layers;
    } else {
      ResetPlaneTarget(composition.back(), commit_planes.back());
    }
  }

  // We are just compositing Primary layer and nothing else.
  if (layers.size() == 1) {
    return render_layers;
  }

  // Retrieve cursor layer data.
  DisplayPlane *cursor_plane = NULL;
  for (auto j = layer_end - 1; j >= layer_begin; j--) {
    if (j->GetBuffer()->GetUsage() & kLayerCursor) {
      cursor_layer = &(*(j));
      // Handle Cursor layer.
      if (cursor_layer) {
        // Handle Cursor layer. If we have dedicated cursor plane, try using it
        // to composite cursor layer.
        if (cursor_plane_)
          cursor_plane = cursor_plane_.get();
        if (cursor_plane) {
          commit_planes.emplace_back(OverlayPlane(cursor_plane, cursor_layer));
          // Lets ensure we fall back to GPU composition in case
          // cursor layer cannot be scanned out directly.
          if (FallbacktoGPU(cursor_plane, cursor_layer, commit_planes)) {
            cursor_plane = NULL;
            commit_planes.pop_back();
	    j->GPURenderedCursor();
          } else
            layer_end = j;
        }
      }
      break;
    }
  }

  if (layer_begin != layer_end) {
    // Handle layers for overlay
    uint32_t index = 0;
    for (auto j = overlay_planes_.begin(); j != overlay_planes_.end(); ++j) {
      DisplayPlaneState &last_plane = composition.back();
      // Handle remaining overlay planes.
      for (auto i = layer_begin; i != layer_end; ++i) {
        OverlayLayer *layer = &(*(i));
        commit_planes.emplace_back(OverlayPlane(j->get(), layer));
        index = i->GetIndex();
        ++layer_begin;
        // If we are able to composite buffer with the given plane, lets use
        // it.
        bool fall_back = FallbacktoGPU(j->get(), layer, commit_planes);
        if (!fall_back || prefer_seperate_plane ||
            layer->PreferSeparatePlane()) {
          composition.emplace_back(j->get(), layer, index);
          if (fall_back) {
            ResetPlaneTarget(composition.back(), commit_planes.back());
            render_layers = true;
          }

          prefer_seperate_plane = layer->PreferSeparatePlane();
          break;
        } else {
          last_plane.AddLayer(i->GetIndex(), i->GetDisplayFrame());
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
      last_plane.AddLayer(i->GetIndex(), i->GetDisplayFrame());
    }

    if (last_plane.GetCompositionState() == DisplayPlaneState::State::kRender)
      render_layers = true;
  }

  if (cursor_plane) {
    composition.emplace_back(cursor_plane, cursor_layer,
                             cursor_layer->GetIndex());
  }

  if (render_layers) {
    ValidateFinalLayers(composition, layers);
  }

  return render_layers;
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
  for (auto &fb : cursor_surfaces_) {
    if (!fb->InUse()) {
      surface = fb.get();
      break;
    }
  }

  if (!surface) {
    NativeSurface *new_surface = CreateBackBuffer(width, height);
    new_surface->Init(buffer_handler_, true);
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
  for (auto &fb : surfaces_) {
    if (!fb->InUse()) {
      surface = fb.get();
      break;
    }
  }

  if (!surface) {
    NativeSurface *new_surface = CreateBackBuffer(width_, height_);
    new_surface->Init(buffer_handler_);
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
    DisplayPlane *current_plane = primary_plane_.get();
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
                             primary_layer->GetIndex());
    DisplayPlaneState &last_plane = composition.back();
    last_plane.ForceGPURendering();
    ++layer_begin;

    for (auto i = layer_begin; i != layers.end(); ++i) {
      last_plane.AddLayer(i->GetIndex(), i->GetDisplayFrame());
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
