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
#include "renderer.h"
#include "scopedrendererstate.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, bool disable_overlay,
                           NativeBufferHandler* buffer_handler,
                           PhysicalDisplay* display)
    : frame_(0),
      gpu_fd_(gpu_fd),
      buffer_handler_(buffer_handler),
      display_(display) {
  compositor_.Init();
  if (disable_overlay) {
    state_ |= kDisableOverlayUsage;
  } else {
    state_ &= ~kDisableOverlayUsage;
  }

  vblank_handler_.reset(new VblankEventHandler(this));

  /* use 0x80 as default brightness for all colors */
  brightness_ = 0x808080;
  /* use 0x80 as default brightness for all colors */
  contrast_ = 0x808080;
  /* use 1 as default gamma value */
  gamma_.red = 1;
  gamma_.green = 1;
  gamma_.blue = 1;
  state_ |= kNeedsColorCorrection;
}

DisplayQueue::~DisplayQueue() {
}

bool DisplayQueue::Initialize(float refresh, uint32_t pipe, uint32_t width,
                              uint32_t height,
                              DisplayPlaneHandler* plane_handler) {
  frame_ = 0;
  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, buffer_handler_, plane_handler));
  if (!display_plane_manager_->Initialize(width, height)) {
    ETRACE("Failed to initialize DisplayPlane Manager.");
    return false;
  }

  vblank_handler_->Init(refresh, gpu_fd_, pipe);
  if (idle_tracker_.state_ & FrameStateTracker::kIgnoreUpdates) {
    hwc_lock_.reset(new HWCLock());
    if (!hwc_lock_->RegisterCallBack(this)) {
      idle_tracker_.state_ &= ~FrameStateTracker::kIgnoreUpdates;
      hwc_lock_.reset(nullptr);
    }
  }

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
      state_ |= kConfigurationChanged;
      state_ |= kNeedsColorCorrection;
      vblank_handler_->SetPowerMode(kOn);
      power_mode_lock_.lock();
      state_ &= ~kIgnoreIdleRefresh;
      power_mode_lock_.unlock();
      break;
    default:
      break;
  }

  return true;
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers,
                                   bool* can_ignore_commit) {
  CTRACE();
  bool needs_gpu_composition = false;
  bool ignore_commit = true;
  for (DisplayPlaneState& plane : previous_plane_state_) {
    composition->emplace_back(plane.plane());
    DisplayPlaneState& last_plane = composition->back();
    last_plane.AddLayers(plane.source_layers(), plane.GetDisplayFrame(),
                         plane.GetCompositionState());

    if (plane.GetCompositionState() == DisplayPlaneState::State::kRender ||
        plane.SurfaceRecycled()) {
      bool content_changed = false;
      bool region_changed = false;
      const std::vector<size_t>& source_layers = plane.source_layers();
      HwcRect<int> surface_damage = HwcRect<int>(0, 0, 0, 0);
      size_t layers_size = source_layers.size();

      for (size_t i = 0; i < layers_size; i++) {
        size_t source_index = source_layers.at(i);
        const OverlayLayer& layer = layers.at(source_index);
        if (layer.HasLayerContentChanged()) {
          content_changed = true;
          if (!region_changed) {
            const HwcRect<int>& damage = layer.GetSurfaceDamage();
            surface_damage.left = std::min(surface_damage.left, damage.left);
            surface_damage.top = std::min(surface_damage.top, damage.top);
            surface_damage.right = std::max(surface_damage.right, damage.right);
            surface_damage.bottom =
                std::max(surface_damage.bottom, damage.bottom);
          }
        }

        if (layer.HasDimensionsChanged()) {
          last_plane.UpdateDisplayFrame(layer.GetDisplayFrame());
          region_changed = true;
        }
      }

      plane.TransferSurfaces(last_plane, content_changed || region_changed);
      if (region_changed) {
        surface_damage = last_plane.GetDisplayFrame();
      }

      if (content_changed || region_changed) {
        if (last_plane.GetSurfaces().size() == 3) {
          NativeSurface* surface = last_plane.GetOffScreenTarget();
          surface->RecycleSurface(last_plane);
          surface->UpdateSurfaceDamage(
              surface_damage,
              plane.GetOffScreenTarget()->GetLastSurfaceDamage());
          if (!region_changed)
            last_plane.DisableClearSurface();
        } else {
          display_plane_manager_->SetOffScreenPlaneTarget(last_plane);
        }

        last_plane.ForceGPURendering();
        needs_gpu_composition = true;
      } else {
        NativeSurface* surface = plane.GetOffScreenTarget();
        surface->RecycleSurface(last_plane);
        last_plane.ReUseOffScreenTarget();
      }

      if (!region_changed) {
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
      if (layer->HasLayerContentChanged() || layer->HasDimensionsChanged()) {
        ignore_commit = false;
      }
    }
  }

  *render_layers = needs_gpu_composition;
  if (needs_gpu_composition)
    ignore_commit = false;

  *can_ignore_commit = ignore_commit;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence, bool idle_update) {
  CTRACE();
  ScopedIdleStateTracker tracker(idle_tracker_);
  if (tracker.IgnoreUpdate())
    return true;

  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  std::vector<OverlayLayer> layers;
  bool frame_changed = (size != previous_size);
  bool idle_frame = tracker.RenderIdleMode() || idle_update;
  if ((state_ & kConfigurationChanged) || idle_frame)
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
    if (scaling_tracker_.scaling_state_ == ScalingTracker::kNeedsScaling) {
      HwcRect<int> display_frame = layer->GetDisplayFrame();
      display_frame.left =
          display_frame.left +
          (display_frame.left * scaling_tracker_.scaling_width);
      display_frame.top = display_frame.top +
                          (display_frame.top * scaling_tracker_.scaling_height);
      display_frame.right =
          display_frame.right +
          (display_frame.right * scaling_tracker_.scaling_width);
      display_frame.bottom =
          display_frame.bottom +
          (display_frame.bottom * scaling_tracker_.scaling_height);

      overlay_layer.SetDisplayFrame(display_frame);
    } else {
      overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    }

    overlay_layer.SetLayerIndex(layer_index);
    overlay_layer.SetZorder(index);
    overlay_layer.SetBuffer(buffer_handler_, layer->GetNativeHandle(),
                            layer->GetAcquireFence());
    overlay_layer.ValidateForOverlayUsage();
    index++;

    if (frame_changed) {
      continue;
    }

    if (previous_size > layer_index) {
      overlay_layer.ValidatePreviousFrameState(
          in_flight_layers_.at(layer_index), layer);
    }

    if (overlay_layer.HasLayerAttributesChanged()) {
      layers_changed = true;
    }
  }

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  bool validate_layers = layers_changed || tracker.RevalidateLayers();
  bool composition_passed = true;
  bool disable_ovelays = state_ & kDisableOverlayUsage;

  // Validate Overlays and Layers usage.
  if (!validate_layers) {
    bool can_ignore_commit = false;
    // Before forcing layer validation, check if content has changed
    // if not continue showing the current buffer.
    GetCachedLayers(layers, &current_composition_planes, &render_layers,
                    &can_ignore_commit);

    if (can_ignore_commit) {
      HandleCommitIgnored(current_composition_planes);
      return true;
    }

  } else {
    tracker.ResetTrackerState();
    render_layers = display_plane_manager_->ValidateLayers(
        layers, state_ & kConfigurationChanged, idle_frame || disable_ovelays,
        current_composition_planes);
    state_ &= ~kConfigurationChanged;
  }

  DUMP_CURRENT_COMPOSITION_PLANES();

  if (render_layers) {
    if (!compositor_.BeginFrame(disable_ovelays)) {
      ETRACE("Failed to initialize compositor.");
      composition_passed = false;
    }

    if (composition_passed) {
      std::vector<HwcRect<int>> layers_rects;
      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        const OverlayLayer& layer = layers.at(layer_index);
        layers_rects.emplace_back(layer.GetDisplayFrame());
      }

      // Prepare for final composition.
      if (!compositor_.Draw(current_composition_planes, layers, layers_rects)) {
        ETRACE("Failed to prepare for the frame composition. ");
        composition_passed = false;
      }
    }
  }

  if (!composition_passed) {
    UpdateSurfaceInUse(false, current_composition_planes);
    UpdateSurfaceInUse(true, previous_plane_state_);
    ReleaseSurfaces();
    return false;
  }

  int32_t fence = 0;
#ifndef ENABLE_DOUBLE_BUFFERING
  if (kms_fence_ > 0) {
    HWCPoll(kms_fence_, -1);
    close(kms_fence_);
    kms_fence_ = 0;
  }
#endif
  if (state_ & kNeedsColorCorrection) {
    display_->SetColorCorrection(gamma_, contrast_, brightness_);
    state_ &= ~kNeedsColorCorrection;
  }

  composition_passed =
      display_->Commit(current_composition_planes, previous_plane_state_,
                       disable_ovelays, &fence);

  if (!composition_passed) {
    UpdateSurfaceInUse(false, current_composition_planes);
    UpdateSurfaceInUse(true, previous_plane_state_);
    ReleaseSurfaces();
    return false;
  }

  in_flight_layers_.swap(layers);
  UpdateSurfaceInUse(false, previous_plane_state_);
  previous_plane_state_.swap(current_composition_planes);
  UpdateSurfaceInUse(true, previous_plane_state_);
  if (state_ & kReleaseSurfaces) {
    ReleaseSurfaces();
    state_ &= ~kReleaseSurfaces;
  }

  if (validate_layers) {
    state_ |= kReleaseSurfaces;
  }

  if (idle_frame) {
    ReleaseSurfaces();
    state_ |= kLastFrameIdleUpdate;
    if (state_ & kClonedMode) {
      idle_tracker_.state_ |= FrameStateTracker::kRenderIdleDisplay;
    }
  } else {
    state_ &= ~kLastFrameIdleUpdate;
  }

  if (fence > 0) {
    if (!(state_ & kClonedMode)) {
      if (render_layers)
        compositor_.InsertFence(dup(fence));

      *retire_fence = dup(fence);
    }
    kms_fence_ = fence;

    SetReleaseFenceToLayers(fence, source_layers);
  } else {
    if (render_layers)
      compositor_.InsertFence(0);
  }

  if (hwc_lock_.get()) {
    hwc_lock_->DisableWatch();
    hwc_lock_.reset(nullptr);
  }

#ifdef ENABLE_DOUBLE_BUFFERING
  if (kms_fence_ > 0) {
    HWCPoll(kms_fence_, -1);
    close(kms_fence_);
    kms_fence_ = 0;
  }
#endif

  return true;
}

void DisplayQueue::SetCloneMode(bool cloned) {
  if (cloned) {
    if (!(state_ & kClonedMode)) {
      state_ |= kClonedMode;
      vblank_handler_->SetPowerMode(kOff);
    }
  } else if (state_ & kClonedMode) {
    state_ &= ~kClonedMode;
    state_ |= kConfigurationChanged;
    vblank_handler_->SetPowerMode(kOn);
  }
}

void DisplayQueue::ReleaseSurfaces() {
  if (!compositor_.BeginFrame(state_ & kDisableOverlayUsage)) {
    ETRACE("Failed to initialize compositor.");
    return;
  }

  ScopedRendererState state(compositor_.GetRenderer());
  if (!state.IsValid()) {
    ETRACE("Failed to make context current.");
    return;
  }

  display_plane_manager_->ReleaseFreeOffScreenTargets();
}

void DisplayQueue::UpdateSurfaceInUse(
    bool in_use, DisplayPlaneStateList& current_composition_planes) {
  for (DisplayPlaneState& plane_state : current_composition_planes) {
    std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    for (NativeSurface* surface : surfaces) {
      surface->SetInUse(in_use);
    }
  }
}

void DisplayQueue::HandleCommitIgnored(
    DisplayPlaneStateList& current_composition_planes) {
  for (DisplayPlaneState& plane_state : current_composition_planes) {
    std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    for (NativeSurface* surface : surfaces) {
      surface->SetInUse(false);
    }
  }

  for (DisplayPlaneState& plane_state : previous_plane_state_) {
    std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    for (NativeSurface* surface : surfaces) {
      surface->SetInUse(true);
    }
  }

  ReleaseSurfaces();
}

void DisplayQueue::SetReleaseFenceToLayers(
    int32_t fence, std::vector<HwcLayer*>& source_layers) const {
  for (const DisplayPlaneState& plane : previous_plane_state_) {
    const std::vector<size_t>& layers = plane.source_layers();
    size_t size = layers.size();
    int32_t release_fence = -1;
    if (plane.GetCompositionState() == DisplayPlaneState::State::kScanout &&
        !plane.SurfaceRecycled()) {
      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        const OverlayLayer& overlay_layer =
            in_flight_layers_.at(layers.at(layer_index));
        HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
        layer->SetReleaseFence(dup(fence));
      }
    } else {
      release_fence = plane.GetOverlayLayer()->ReleaseAcquireFence();

      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        const OverlayLayer& overlay_layer =
            in_flight_layers_.at(layers.at(layer_index));
        HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
        if (release_fence > 0) {
          layer->SetReleaseFence(dup(release_fence));
        } else {
          int32_t temp = overlay_layer.ReleaseAcquireFence();
          if (temp > 0)
            layer->SetReleaseFence(temp);
        }
      }

      if (release_fence > 0) {
        close(release_fence);
        release_fence = -1;
      }
    }
  }
}

void DisplayQueue::HandleExit() {
  power_mode_lock_.lock();
  state_ |= kIgnoreIdleRefresh;
  power_mode_lock_.unlock();
  vblank_handler_->SetPowerMode(kOff);
  display_->Disable(previous_plane_state_);
  std::vector<OverlayLayer>().swap(in_flight_layers_);
  DisplayPlaneStateList().swap(previous_plane_state_);
  idle_tracker_.state_ = 0;
  idle_tracker_.idle_frames_ = 0;
  if (kms_fence_ > 0) {
    close(kms_fence_);
    kms_fence_ = 0;
  }

  bool disable_overlay = false;
  if (state_ & kDisableOverlayUsage) {
    disable_overlay = true;
  }

  bool cloned_mode = false;
  if (state_ & kClonedMode) {
    cloned_mode = true;
  }

  state_ = kConfigurationChanged;
  if (disable_overlay) {
    state_ |= kDisableOverlayUsage;
  }

  if (cloned_mode) {
    state_ |= kClonedMode;
  }

  if (display_plane_manager_->HasSurfaces()) {
    compositor_.BeginFrame(true);
    compositor_.GetRenderer()->MakeCurrent();
    display_plane_manager_->ReleaseAllOffScreenTargets();
    compositor_.GetRenderer()->RestoreState();
  }

  compositor_.Reset();
}

bool DisplayQueue::CheckPlaneFormat(uint32_t format) {
  return display_plane_manager_->CheckPlaneFormat(format);
}

void DisplayQueue::SetGamma(float red, float green, float blue) {
  gamma_.red = red;
  gamma_.green = green;
  gamma_.blue = blue;
  state_ |= kNeedsColorCorrection;
}

void DisplayQueue::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;
  contrast_ = (red << 16) | (green << 8) | (blue);
  state_ |= kNeedsColorCorrection;
}

void DisplayQueue::SetBrightness(uint32_t red, uint32_t green, uint32_t blue) {
  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;
  brightness_ = (red << 16) | (green << 8) | (blue);
  state_ |= kNeedsColorCorrection;
}

void DisplayQueue::SetExplicitSyncSupport(bool disable_explicit_sync) {
  if (disable_explicit_sync) {
    state_ |= kDisableOverlayUsage;
  } else {
    state_ &= ~kDisableOverlayUsage;
  }
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

  idle_tracker_.continuous_frames_ = 0;

  if (idle_tracker_.idle_frames_ < 50) {
    idle_tracker_.idle_frames_++;
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  if (previous_plane_state_.size() <= 1 || idle_tracker_.idle_frames_ > 50) {
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  idle_tracker_.idle_frames_++;
  power_mode_lock_.lock();
  if (!(state_ & kIgnoreIdleRefresh) && refresh_callback_) {
    refresh_callback_->Callback(refrsh_display_id_);
    idle_tracker_.state_ |= FrameStateTracker::kRenderIdleDisplay;
  }
  power_mode_lock_.unlock();
  idle_tracker_.idle_lock_.unlock();
}

void DisplayQueue::ForceRefresh() {
  idle_tracker_.idle_lock_.lock();
  idle_tracker_.state_ &= ~FrameStateTracker::kIgnoreUpdates;
  power_mode_lock_.lock();
  if (!(state_ & kIgnoreIdleRefresh) && refresh_callback_) {
    refresh_callback_->Callback(refrsh_display_id_);
  }
  power_mode_lock_.unlock();
  idle_tracker_.idle_lock_.unlock();
}

void DisplayQueue::DisplayConfigurationChanged() {
  // Mark it as needs modeset, so that in next queue update we do a modeset
  state_ |= kConfigurationChanged;
}

void DisplayQueue::UpdateScalingRatio(uint32_t primary_width,
                                      uint32_t primary_height,
                                      uint32_t display_width,
                                      uint32_t display_height) {
  scaling_tracker_.scaling_state_ = ScalingTracker::kNeeedsNoSclaing;
  uint32_t primary_area = primary_width * primary_height;
  uint32_t display_area = display_width * display_height;
  if (primary_area != display_area) {
    scaling_tracker_.scaling_state_ = ScalingTracker::kNeedsScaling;
    scaling_tracker_.scaling_width =
        float(display_width - primary_width) / float(primary_width);
    scaling_tracker_.scaling_height =
        float(display_height - primary_height) / float(primary_height);
  }

  state_ |= kConfigurationChanged;
}

}  // namespace hwcomposer
