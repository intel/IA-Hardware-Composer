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

#include <set>
#include <utility>

#include <drm/drm_fourcc.h>

#include <overlaylayer.h>
#include <nativebufferhandler.h>

#include "displayplane.h"
#include "hwctrace.h"
#include "nativesync.h"
#include "overlaybuffer.h"

namespace hwcomposer {

DisplayPlaneManager::DisplayPlaneManager(int gpu_fd, uint32_t pipe_id,
                                         uint32_t crtc_id)
    : crtc_id_(crtc_id), pipe_(pipe_id), gpu_fd_(gpu_fd) {
}

DisplayPlaneManager::~DisplayPlaneManager() {
}

bool DisplayPlaneManager::Initialize() {
  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(gpu_fd_));
  if (!plane_resources) {
    ETRACE("Failed to get plane resources");
    return false;
  }

  uint32_t num_planes = plane_resources->count_planes;
  uint32_t pipe_bit = 1 << pipe_;
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

  return true;
}

bool DisplayPlaneManager::BeginFrameUpdate(
    std::vector<OverlayLayer> &layers, NativeBufferHandler *buffer_handler) {
  if (cursor_plane_)
    cursor_plane_->SetEnabled(false);

  for (auto i = overlay_planes_.begin(); i != overlay_planes_.end(); ++i) {
    (*i)->SetEnabled(false);
  }

  size_t size = layers.size();
  std::vector<std::unique_ptr<OverlayBuffer>>().swap(in_flight_buffers_);
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    OverlayLayer *layer = &layers.at(layer_index);
    HwcBuffer bo;
    if (!buffer_handler->ImportBuffer(layer->GetNativeHandle(), &bo)) {
      ETRACE("Failed to Import buffer.");
      return false;
    }

    in_flight_buffers_.emplace_back(new OverlayBuffer());
    OverlayBuffer *buffer = in_flight_buffers_.back().get();
    buffer->Initialize(bo);
    layer->SetBuffer(buffer);
  }

  return true;
}

std::tuple<bool, DisplayPlaneStateList> DisplayPlaneManager::ValidateLayers(
    std::vector<OverlayLayer> &layers, bool pending_modeset) {
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
  if ((pending_modeset && layers.size() > 1) ||
      FallbacktoGPU(current_plane, primary_layer, commit_planes)) {
    DisplayPlaneState &last_plane = composition.back();
    render_layers = true;
    // Case where we have just one layer which needs to be composited using
    // GPU.
    last_plane.ForceGPURendering();

    for (auto i = layer_begin; i != layer_end; ++i) {
      last_plane.AddLayer(i->GetIndex());
    }
    // We need to composite primary using GPU, lets use this for
    // all layers in this case.
    return std::make_tuple(render_layers, std::move(composition));
  }

  // We are just compositing Primary layer and nothing else.
  if (layers.size() == 1) {
    return std::make_tuple(render_layers, std::move(composition));
  }

  // Retrieve cursor layer data and delete it from the layers.
  for (auto j = layers.rbegin(); j != layers.rend(); ++j) {
    if (j->GetBuffer()->GetUsage() & kLayerCursor) {
      cursor_layer = &(*(j));
      layer_end = std::next(j).base();
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
        if (!FallbacktoGPU(j->get(), layer, commit_planes)) {
          composition.emplace_back(j->get(), layer, index);
          break;
        } else {
          last_plane.AddLayer(index);
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
      last_plane.AddLayer(i->GetIndex());
    }

    if (last_plane.GetCompositionState() == DisplayPlaneState::State::kRender)
      render_layers = true;
  }

  // Handle Cursor layer.
  DisplayPlane *cursor_plane = NULL;
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
      }
    }

    // We need to do this here to avoid compositing cursor with any previous
    // pre-composited planes.
    if (cursor_plane) {
      composition.emplace_back(cursor_plane, cursor_layer,
                               cursor_layer->GetIndex());
    } else {
      DisplayPlaneState &last_plane = composition.back();
      render_layers = true;
      last_plane.AddLayer(cursor_layer->GetIndex());
    }
  }

  return std::make_tuple(render_layers, std::move(composition));
}

bool DisplayPlaneManager::CommitFrame(DisplayPlaneStateList &comp_planes,
                                      drmModeAtomicReqPtr pset,
                                      bool needs_modeset,
                                      std::unique_ptr<NativeSync> &sync_object,
                                      ScopedFd &fence) {
  CTRACE();
  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  uint32_t flags = 0;
  if (needs_modeset) {
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  } else {
#ifdef DISABLE_OVERLAY_USAGE
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
#else
    flags |= DRM_MODE_ATOMIC_NONBLOCK;
#endif
  }

  for (DisplayPlaneState &comp_plane : comp_planes) {
    DisplayPlane *plane = comp_plane.plane();
    OverlayLayer *layer = comp_plane.GetOverlayLayer();
    if (!plane->UpdateProperties(pset, crtc_id_, layer))
      return false;

    plane->SetEnabled(true);
  }

  // Disable unused planes.
  if (cursor_plane_ && !cursor_plane_->IsEnabled()) {
    cursor_plane_->Disable(pset);
  }

  for (auto i = overlay_planes_.begin(); i != overlay_planes_.end(); ++i) {
    if ((*i)->IsEnabled())
      continue;

    (*i)->Disable(pset);
  }

  int ret = drmModeAtomicCommit(gpu_fd_, pset, flags, NULL);
  if (ret) {
    if (ret == -EBUSY) {
#ifndef DISABLE_EXPLICIT_SYNC
      if (fence.get() != -1) {
        if (!sync_object->Wait(fence.get())) {
          ETRACE("Failed to wait for fence ret=%s\n", PRINTERROR());
          return false;
        }
      }

      ret = drmModeAtomicCommit(gpu_fd_, pset, flags, NULL);
#else
      /* FIXME - In case of EBUSY, we spin until succeed. What we
       * probably should do is to queue commits and process them later.
       */
      ret = -EBUSY;
      while (ret == -EBUSY)
        ret = drmModeAtomicCommit(gpu_fd_, pset, flags, NULL);
#endif
    }
  }

  fence.Close();

  if (ret) {
    ETRACE("Failed to commit pset ret=%s\n", PRINTERROR());
    return false;
  }

  if (!needs_modeset)
    current_sync_.reset(sync_object.release());

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
}

bool DisplayPlaneManager::TestCommit(
    const std::vector<OverlayPlane> &commit_planes) const {
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());
  for (auto i = commit_planes.begin(); i != commit_planes.end(); i++) {
    if (!(i->plane->UpdateProperties(pset.get(), crtc_id_, i->layer))) {
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

void DisplayPlaneManager::EndFrameUpdate() {
  displayed_buffers_.swap(in_flight_buffers_);
}

bool DisplayPlaneManager::FallbacktoGPU(
    DisplayPlane *target_plane, OverlayLayer *layer,
    const std::vector<OverlayPlane> &commit_planes) const {
#ifdef DISABLE_OVERLAY_USAGE
  return true;
#endif

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

}  // namespace hwcomposer
