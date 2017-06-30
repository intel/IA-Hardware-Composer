/*
// Copyright (c) 2017 Intel Corporation
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

#include "displayqueue.h"

#include <math.h>
#include <hwcdefs.h>
#include <hwclayer.h>

#include <vector>

#include "displayplanemanager.h"
#include "hwctrace.h"
#include "hwcutils.h"
#include "overlaylayer.h"
#include "vblankeventhandler.h"
#include "nativesurface.h"

#include "physicaldisplay.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, bool disable_overlay,
                           NativeBufferHandler* buffer_handler,
                           PhysicalDisplay* display)
    : frame_(0),
      gpu_fd_(gpu_fd),
      buffer_handler_(buffer_handler),
      display_(display) {
  compositor_.Init();
  disable_overlay_usage_ = disable_overlay;

  vblank_handler_.reset(new VblankEventHandler(this));

  /* use 0x80 as default brightness for all colors */
  brightness_ = 0x808080;
  /* use 0x80 as default brightness for all colors */
  contrast_ = 0x808080;
  /* use 1 as default gamma value */
  gamma_.red = 1;
  gamma_.green = 1;
  gamma_.blue = 1;
  needs_color_correction_ = true;
}

DisplayQueue::~DisplayQueue() {
}

bool DisplayQueue::Initialize(float refresh, uint32_t pipe,
                              DisplayPlaneHandler* plane_handler) {
  frame_ = 0;
  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, buffer_handler_, plane_handler));
  if (!display_plane_manager_->Initialize()) {
    ETRACE("Failed to initialize DisplayPlane Manager.");
    return false;
  }

  vblank_handler_->Init(refresh, gpu_fd_, pipe);

  return true;
}

bool DisplayQueue::SetPowerMode(uint32_t power_mode) {
  switch (power_mode) {
    case kOff:
      HandleExit();
      break;
    case kDoze:
      HandleExit();
      break;
    case kDozeSuspend:
      vblank_handler_->SetPowerMode(kDozeSuspend);
      break;
    case kOn:
      configuration_changed_ = true;
      needs_color_correction_ = true;
      vblank_handler_->SetPowerMode(kOn);
      break;
    default:
      break;
  }

  return true;
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers) {
  CTRACE();
  bool needs_gpu_composition = false;
  for (DisplayPlaneState& plane : previous_plane_state_) {
    composition->emplace_back(plane.plane());
    DisplayPlaneState& last_plane = composition->back();
    last_plane.AddLayers(plane.source_layers(), plane.GetDisplayFrame(),
                         plane.GetSurfaceDamage(), plane.GetCompositionState());

    if (plane.GetCompositionState() == DisplayPlaneState::State::kRender ||
        plane.SurfaceRecycled()) {
      bool content_changed = false;
      bool region_changed = false;
      const std::vector<size_t>& source_layers = plane.source_layers();
      size_t layers_size = source_layers.size();

      for (size_t i = 0; i < layers_size; i++) {
        size_t source_index = source_layers.at(i);
        const OverlayLayer& layer = layers.at(source_index);
        if (layer.HasLayerPositionChanged()) {
          region_changed = true;
        }

        if (layer.HasLayerContentChanged()) {
          content_changed = true;
          last_plane.AddSurfaceDamage(layer.GetSurfaceDamage());
        }
      }

      plane.TransferSurfaces(last_plane, content_changed);
      if (content_changed) {
        if (last_plane.GetSurfaces().size() == 3) {
          last_plane.GetOffScreenTarget()->RecycleSurface(last_plane);
        } else {
          display_plane_manager_->SetOffScreenPlaneTarget(last_plane);
          if (last_plane.GetSurfaces().size() == 3)
            last_plane.ResetSurfaceDamage();
        }

        last_plane.ForceGPURendering();
        needs_gpu_composition = true;
      } else {
        NativeSurface* surface = plane.GetOffScreenTarget();
        surface->RecycleSurface(last_plane);
        last_plane.ReUseOffScreenTarget();
      }

      if (region_changed) {
        const std::vector<CompositionRegion>& comp_regions =
            plane.GetCompositionRegion();
        last_plane.GetCompositionRegion().assign(comp_regions.begin(),
                                                 comp_regions.end());
      }
    } else {
      const OverlayLayer* layer =
          &(*(layers.begin() + last_plane.source_layers().front()));
      layer->GetBuffer()->CreateFrameBuffer(gpu_fd_);
      last_plane.SetOverlayLayer(layer);
    }
  }

  *render_layers = needs_gpu_composition;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence) {
  CTRACE();
  ScopedIdleStateTracker tracker(idle_tracker_);

  use_layer_cache_ = !configuration_changed_;
  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  std::vector<OverlayLayer> layers;
  std::vector<HwcRect<int>> layers_rects;
  bool frame_changed = (size != previous_size);
  bool idle_frame = tracker.RenderIdleMode();
  if (!use_layer_cache_ || idle_frame)
    frame_changed = true;

  bool layers_changed = frame_changed;
  *retire_fence = -1;
  uint32_t index = 0;

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;

    layers.emplace_back();
    OverlayLayer& overlay_layer = layers.back();
    overlay_layer.SetTransform(layer->GetTransform());
    overlay_layer.SetAlpha(layer->GetAlpha());
    overlay_layer.SetBlending(layer->GetBlending());
    overlay_layer.SetSourceCrop(layer->GetSourceCrop());
    overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    overlay_layer.SetLayerIndex(layer_index);
    overlay_layer.SetZorder(index);
    layers_rects.emplace_back(layer->GetDisplayFrame());
    overlay_layer.SetBuffer(buffer_handler_, layer->GetNativeHandle(),
                            layer->GetAcquireFence());
    overlay_layer.ValidateForOverlayUsage();
    index++;

    if (frame_changed) {
      layer->Validate();
      continue;
    }

    if (previous_size > layer_index) {
      overlay_layer.ValidatePreviousFrameState(
          in_flight_layers_.at(layer_index), layer);
    }

    if (overlay_layer.HasLayerAttributesChanged()) {
      layers_changed = true;
    }

    layer->Validate();
  }

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  bool validate_layers = layers_changed || tracker.RevalidateLayers();
  bool composition_passed = true;

  // Validate Overlays and Layers usage.
  if (!validate_layers) {
    // Before forcing layer validation check if content has changed
    // if not continue showing the current buffer.
    GetCachedLayers(layers, &current_composition_planes, &render_layers);
  } else {
    tracker.ResetTrackerState();
    MarkBackBuffersForReUse();
    render_layers = display_plane_manager_->ValidateLayers(
        layers, configuration_changed_, idle_frame || disable_overlay_usage_,
        current_composition_planes);
    configuration_changed_ = false;
  }

  DUMP_CURRENT_COMPOSITION_PLANES();

  if (render_layers) {
    if (!compositor_.BeginFrame(disable_overlay_usage_)) {
      ETRACE("Failed to initialize compositor.");
      composition_passed = false;
    }

    // Prepare for final composition.
    if (!compositor_.Draw(current_composition_planes, layers, layers_rects)) {
      ETRACE("Failed to prepare for the frame composition. ");
      composition_passed = false;
    }
  }

  if (!composition_passed) {
    UpdateSurfaceInUse(false, current_composition_planes);
    UpdateSurfaceInUse(true, previous_plane_state_);
    display_plane_manager_->ReleaseFreeOffScreenTargets();
    return false;
  }

  int32_t fence = 0;
  if (kms_fence_ > 0) {
    HWCPoll(kms_fence_, -1);
    close(kms_fence_);
    kms_fence_ = 0;
  }

  if (needs_color_correction_) {
    display_->SetColorCorrection(gamma_, contrast_, brightness_);
    needs_color_correction_ = false;
  }

  composition_passed =
      display_->Commit(current_composition_planes, previous_plane_state_,
                       disable_overlay_usage_, &fence);

  if (!composition_passed) {
    UpdateSurfaceInUse(false, current_composition_planes);
    UpdateSurfaceInUse(true, previous_plane_state_);
    display_plane_manager_->ReleaseFreeOffScreenTargets();
    return false;
  }

  in_flight_layers_.swap(layers);
  if (release_surfaces_) {
    display_plane_manager_->ReleaseFreeOffScreenTargets();
    release_surfaces_ = false;
  }

  UpdateSurfaceInUse(false, previous_plane_state_);
  previous_plane_state_.swap(current_composition_planes);
  UpdateSurfaceInUse(true, previous_plane_state_);
  release_surfaces_ = validate_layers;

  if (idle_frame)
    display_plane_manager_->ReleaseFreeOffScreenTargets();

  if (fence > 0) {
    if (render_layers)
      compositor_.InsertFence(dup(fence));

    *retire_fence = dup(fence);
    kms_fence_ = fence;

    SetReleaseFenceToLayers(fence, source_layers);
  } else {
    // This is the best we can do in this case, flush any 3D
    // operations and release buffers of previous layers.
    if (render_layers)
      compositor_.InsertFence(0);
  }

  return true;
}

void DisplayQueue::UpdateSurfaceInUse(
    bool in_use, DisplayPlaneStateList& current_composition_planes) {
  for (DisplayPlaneState& plane_state : current_composition_planes) {
    if (!in_use && !plane_state.ReleaseSurfaces())
      continue;

    if (in_use)
      plane_state.SurfaceTransitionComplete();

    std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    for (NativeSurface* surface : surfaces) {
      surface->SetInUse(in_use);
    }
  }
}

void DisplayQueue::MarkBackBuffersForReUse() {
  for (DisplayPlaneState& plane_state : previous_plane_state_) {
    std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    size_t size = surfaces.size();
    for (size_t i = 1; i < size; i++) {
        surfaces.at(i)->SetInUse(false);
    }
  }
}

void DisplayQueue::SetReleaseFenceToLayers(
    int32_t fence, std::vector<HwcLayer*>& source_layers) const {
  for (const DisplayPlaneState& plane : previous_plane_state_) {
    const std::vector<size_t>& layers = plane.source_layers();
    size_t size = layers.size();
    int32_t release_fence = -1;
    if (plane.GetCompositionState() == DisplayPlaneState::State::kScanout &&
        !plane.SurfaceRecycled()) {
      release_fence = fence;
    } else {
      release_fence = plane.GetOverlayLayer()->GetAcquireFence();
    }

    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      const OverlayLayer& overlay_layer =
          in_flight_layers_.at(layers.at(layer_index));
      HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
      layer->SetReleaseFence(dup(release_fence));
    }
  }
}

void DisplayQueue::HandleExit() {
  display_->Disable(previous_plane_state_);
  display_plane_manager_->ReleaseAllOffScreenTargets();

  compositor_.Reset();
  vblank_handler_->SetPowerMode(kOff);
  std::vector<OverlayLayer>().swap(in_flight_layers_);
  DisplayPlaneStateList().swap(previous_plane_state_);
  idle_tracker_.state_ = 0;
  idle_tracker_.idle_frames_ = 0;
  if (kms_fence_ > 0) {
    close(kms_fence_);
    kms_fence_ = 0;
  }
  use_layer_cache_ = false;
}

bool DisplayQueue::CheckPlaneFormat(uint32_t format) {
  return display_plane_manager_->CheckPlaneFormat(format);
}

void DisplayQueue::SetGamma(float red, float green, float blue) {
  gamma_.red = red;
  gamma_.green = green;
  gamma_.blue = blue;
  needs_color_correction_ = true;
}

void DisplayQueue::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;
  contrast_ = (red << 16) | (green << 8) | (blue);
  needs_color_correction_ = true;
}

void DisplayQueue::SetBrightness(uint32_t red, uint32_t green, uint32_t blue) {
  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;
  brightness_ = (red << 16) | (green << 8) | (blue);
  needs_color_correction_ = true;
}

void DisplayQueue::SetExplicitSyncSupport(bool disable_explicit_sync) {
  disable_overlay_usage_ = disable_explicit_sync;
}

int DisplayQueue::RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                        uint32_t display_id) {
  return vblank_handler_->RegisterCallback(callback, display_id);
}

void DisplayQueue::RegisterRefreshCallback(
    std::shared_ptr<RefreshCallback> callback, uint32_t display_id) {
  idle_tracker_.idle_lock_.lock();
  refresh_callback_ = callback;
  refrsh_display_id_ = display_id;
  idle_tracker_.idle_lock_.unlock();
}

void DisplayQueue::VSyncControl(bool enabled) {
  vblank_handler_->VSyncControl(enabled);
}

void DisplayQueue::HandleIdleCase() {
  idle_tracker_.idle_lock_.lock();

  if (idle_tracker_.state_ & FrameStateTracker::kPrepareComposition) {
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  if (idle_tracker_.idle_frames_ < 100) {
    idle_tracker_.idle_frames_++;
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  if (previous_plane_state_.size() <= 1 || idle_tracker_.idle_frames_ > 100) {
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  idle_tracker_.idle_frames_++;
  if (refresh_callback_) {
    refresh_callback_->Callback(refrsh_display_id_);
  }

  idle_tracker_.state_ |= FrameStateTracker::kRenderIdleDisplay;
  idle_tracker_.idle_lock_.unlock();
}

void DisplayQueue::DisplayConfigurationChanged() {
  // Mark it as needs modeset, so that in next queue update we do a modeset
  configuration_changed_ = true;
}

}  // namespace hwcomposer
