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

#include <drm_fourcc.h>

#include <set>
#include <utility>

#include "displayplane.h"
#include "factory.h"
#include "hwctrace.h"
#include "nativesurface.h"
#include "overlaylayer.h"

namespace hwcomposer {

DisplayPlaneManager::DisplayPlaneManager(int gpu_fd, uint32_t crtc_id,
                                         NativeBufferHandler *buffer_handler)
    : buffer_handler_(buffer_handler),
      width_(0),
      height_(0),
      crtc_id_(crtc_id),
      gpu_fd_(gpu_fd) {
}

DisplayPlaneManager::~DisplayPlaneManager() {
}

bool DisplayPlaneManager::Initialize(uint32_t pipe_id, uint32_t width,
                                     uint32_t height) {
  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(gpu_fd_));
  if (!plane_resources) {
    ETRACE("Failed to get plane resources");
    return false;
  }

  uint32_t num_planes = plane_resources->count_planes;
  uint32_t pipe_bit = 1 << pipe_id;
  std::set<uint32_t> plane_ids;
  for (uint32_t i = 0; i < num_planes; ++i) {
    ScopedDrmPlanePtr drm_plane(
        drmModeGetPlane(gpu_fd_, plane_resources->planes[i]));
    if (!drm_plane) {
      ETRACE("Failed to get plane ");
      return false;
    }

    if (!(pipe_bit & drm_plane->possible_crtcs))
      continue;

    uint32_t formats_size = drm_plane->count_formats;
    plane_ids.insert(drm_plane->plane_id);
    std::unique_ptr<DisplayPlane> plane(
        CreatePlane(drm_plane->plane_id, drm_plane->possible_crtcs));
    std::vector<uint32_t> supported_formats(formats_size);
    for (uint32_t j = 0; j < formats_size; j++)
      supported_formats[j] = drm_plane->formats[j];

    if (plane->Initialize(gpu_fd_, supported_formats)) {
      if (plane->type() == DRM_PLANE_TYPE_CURSOR) {
        cursor_plane_.reset(plane.release());
      } else if (plane->type() == DRM_PLANE_TYPE_PRIMARY) {
        plane->SetEnabled(true);
        primary_plane_.reset(plane.release());
      } else if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        overlay_planes_.emplace_back(plane.release());
      }
    }
  }

  if (!primary_plane_) {
    ETRACE("Failed to get primary plane for display %d", crtc_id_);
    return false;
  }

  // We expect layers to be in ascending order.
  std::sort(
      overlay_planes_.begin(), overlay_planes_.end(),
      [](const std::unique_ptr<DisplayPlane> &l,
         const std::unique_ptr<DisplayPlane> &r) { return l->id() < r->id(); });

  width_ = width;
  height_ = height;

  return true;
}

std::tuple<bool, DisplayPlaneStateList> DisplayPlaneManager::ValidateLayers(
    std::vector<OverlayLayer> &layers, bool pending_modeset,
    bool disable_overlay) {
  CTRACE();
  DisplayPlaneStateList composition;
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
      return std::make_tuple(render_layers, std::move(composition));
    } else {
      ResetPlaneTarget(composition.back(), commit_planes.back());
    }
  }

  // We are just compositing Primary layer and nothing else.
  if (layers.size() == 1) {
    return std::make_tuple(render_layers, std::move(composition));
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

  return std::make_tuple(render_layers, std::move(composition));
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

bool DisplayPlaneManager::CommitFrame(const DisplayPlaneStateList &comp_planes,
                                      drmModeAtomicReqPtr pset,
                                      uint32_t flags) {
  CTRACE();
  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  // Disable any cursor/overlay planes assuming they will not
  // be used for this commit.
  if (cursor_plane_)
    cursor_plane_->SetEnabled(false);

  for (auto i = overlay_planes_.begin(); i != overlay_planes_.end(); ++i) {
    (*i)->SetEnabled(false);
  }

  for (const DisplayPlaneState &comp_plane : comp_planes) {
    DisplayPlane *plane = comp_plane.plane();
    const OverlayLayer *layer = comp_plane.GetOverlayLayer();
    int32_t fence = layer->GetAcquireFence();
    if (fence > 0) {
      plane->SetNativeFence(dup(fence));
    } else {
      plane->SetNativeFence(-1);
    }
    if (!plane->UpdateProperties(pset, crtc_id_, layer))
      return false;

    plane->SetEnabled(true);
  }

  // Disable unused planes.
  if (cursor_plane_ && !cursor_plane_->IsEnabled()) {
    cursor_plane_->Disable(pset);
  }

  for (auto i = overlay_planes_.begin(); i != overlay_planes_.end(); ++i) {
    DisplayPlane *plane = (*i).get();
    if (plane->IsEnabled())
      continue;

    plane->Disable(pset);
  }

  int ret = drmModeAtomicCommit(gpu_fd_, pset, flags, NULL);
  if (ret) {
    ETRACE("Failed to commit pset ret=%s\n", PRINTERROR());
    return false;
  }

  return true;
}

void DisplayPlaneManager::DisablePipe(drmModeAtomicReqPtr property_set) {
  CTRACE();
  // Disable planes.
  if (cursor_plane_)
    cursor_plane_->Disable(property_set);

  for (auto i = overlay_planes_.begin(); i != overlay_planes_.end(); ++i) {
    (*i)->Disable(property_set);
  }

  primary_plane_->Disable(property_set);

  int ret = drmModeAtomicCommit(gpu_fd_, property_set,
                                DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
  if (ret)
    ETRACE("Failed to disable pipe:%s\n", PRINTERROR());

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

bool DisplayPlaneManager::TestCommit(
    const std::vector<OverlayPlane> &commit_planes) const {
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());
  for (auto i = commit_planes.begin(); i != commit_planes.end(); i++) {
    if (!(i->plane->UpdateProperties(pset.get(), crtc_id_, i->layer, true))) {
      return false;
    }
  }

  if (drmModeAtomicCommit(gpu_fd_, pset.get(), DRM_MODE_ATOMIC_TEST_ONLY,
                          NULL)) {
    IDISPLAYMANAGERTRACE("Test Commit Failed. %s ", PRINTERROR());
    return false;
  }

  return true;
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
  if (!TestCommit(commit_planes)) {
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

  if (!TestCommit(commit_planes)) {
    return true;
  }

  return false;
}

std::unique_ptr<DisplayPlane> DisplayPlaneManager::CreatePlane(
    uint32_t plane_id, uint32_t possible_crtcs) {
  return std::unique_ptr<DisplayPlane>(
      new DisplayPlane(plane_id, possible_crtcs));
}

bool DisplayPlaneManager::CheckPlaneFormat(uint32_t format) {
  return primary_plane_->IsSupportedFormat(format);
}

}  // namespace hwcomposer
