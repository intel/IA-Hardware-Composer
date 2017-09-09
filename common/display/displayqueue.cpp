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

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, bool disable_overlay,
                           NativeBufferHandler* buffer_handler,
                           PhysicalDisplay* display)
    : frame_(0),
      gpu_fd_(gpu_fd),
      buffer_handler_(buffer_handler),
      display_(display) {
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

bool DisplayQueue::Initialize(uint32_t pipe, uint32_t width, uint32_t height,
                              DisplayPlaneHandler* plane_handler) {
  frame_ = 0;
  std::vector<OverlayLayer>().swap(in_flight_layers_);
  DisplayPlaneStateList().swap(previous_plane_state_);
  idle_tracker_.state_ = 0;
  idle_tracker_.idle_frames_ = 0;
  compositor_.Reset();

  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, buffer_handler_, plane_handler));
  if (!display_plane_manager_->Initialize(width, height)) {
    ETRACE("Failed to initialize DisplayPlane Manager.");
    return false;
  }

  vblank_handler_->SetPowerMode(kOff);
  vblank_handler_->Init(gpu_fd_, pipe);
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
      compositor_.Init(display_plane_manager_.get());
      power_mode_lock_.unlock();
      break;
    default:
      break;
  }

  return true;
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   bool cursor_layer_removed,
                                   bool cursor_layer_added,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers,
                                   bool* can_ignore_commit) {
  CTRACE();
  bool needs_gpu_composition = false;
  bool ignore_commit = true;
  for (DisplayPlaneState& plane : previous_plane_state_) {
    bool plane_state_render =
        plane.GetCompositionState() == DisplayPlaneState::State::kRender;
    if (cursor_layer_removed && plane.IsCursorPlane() && !plane_state_render) {
      ignore_commit = false;
      continue;
    }

    composition->emplace_back(plane.plane());
    DisplayPlaneState& last_plane = composition->back();
    if (plane.IsCursorPlane()) {
      last_plane.AddLayersForCursor(
          plane.source_layers(), plane.GetDisplayFrame(),
          plane.GetCompositionState(), plane.GetCursorLayer(),
          cursor_layer_removed);
    } else {
      last_plane.AddLayers(plane.source_layers(), plane.GetDisplayFrame(),
                           plane.GetCompositionState());
    }

    if (plane_state_render || plane.SurfaceRecycled()) {
      bool content_changed = false;
      bool region_changed = false;
      bool clear_surface = false;
      bool alpha_damaged = false;
      bool reset_regions = false;
      const std::vector<size_t>& source_layers = last_plane.source_layers();
      HwcRect<int> surface_damage = HwcRect<int>(0, 0, 0, 0);
      size_t layers_size = source_layers.size();

      for (size_t i = 0; i < layers_size; i++) {
        size_t source_index = source_layers.at(i);
        const OverlayLayer& layer = layers.at(source_index);
        if (layer.HasDimensionsChanged()) {
          last_plane.UpdateDisplayFrame(layer.GetDisplayFrame());
          region_changed = true;
          clear_surface = true;
        } else if (layer.NeedsToClearSurface()) {
          clear_surface = true;
        }

        if (!clear_surface && layer.HasLayerContentChanged()) {
          const HwcRect<int>& damage = layer.GetSurfaceDamage();
          if (content_changed) {
            surface_damage.left = std::min(surface_damage.left, damage.left);
            surface_damage.top = std::min(surface_damage.top, damage.top);
            surface_damage.right = std::max(surface_damage.right, damage.right);
            surface_damage.bottom =
                std::max(surface_damage.bottom, damage.bottom);
          } else {
            surface_damage = damage;
          }
          content_changed = true;
          // This is a work around for damage corruption when we have more than
          // one layer having alpha = 255 and damaged.
          if (layer.GetBlending() == HWCBlending::kBlendingPremult &&
              layer.GetAlpha() == 255) {
            if (alpha_damaged) {
              clear_surface = true;
            } else {
              alpha_damaged = true;
            }
          }
        }
      }

      if ((cursor_layer_added || cursor_layer_removed) &&
          previous_plane_state_.size() == composition->size()) {
        content_changed = true;
        clear_surface = true;
        reset_regions = true;
      }

      plane.TransferSurfaces(last_plane, content_changed || clear_surface);
      if (region_changed) {
        std::vector<NativeSurface*>& surfaces = last_plane.GetSurfaces();
        size_t size = surfaces.size();
        const HwcRect<int>& current_rect = last_plane.GetDisplayFrame();
        for (size_t i = 0; i < size; i++) {
          surfaces.at(i)->UpdateSurfaceDamage(current_rect, current_rect);
          surfaces.at(i)->UpdateDisplayFrame(current_rect);
        }

        content_changed = true;
      } else if (!reset_regions) {
        const std::vector<CompositionRegion>& comp_regions =
            plane.GetCompositionRegion();
        last_plane.GetCompositionRegion().assign(comp_regions.begin(),
                                                 comp_regions.end());
      }

      if (content_changed || reset_regions) {
        if (last_plane.GetSurfaces().size() == 3) {
          NativeSurface* surface = last_plane.GetOffScreenTarget();
          surface->RecycleSurface(last_plane);
          HwcRect<int> last_damage;
          std::vector<NativeSurface*>& surfaces = last_plane.GetSurfaces();

          if (!clear_surface) {
            last_plane.DisableClearSurface();
            // Calculate Surface damage for the current surface. This should
            // be always equal to current surface damage + damage of last
            // two surfaces.(We use tripple buffering for our internal surfaces)
            last_damage = surfaces.at(1)->GetLastSurfaceDamage();
            const HwcRect<int>& previous_damage =
                surfaces.at(2)->GetLastSurfaceDamage();
            last_damage.left = std::min(previous_damage.left, last_damage.left);
            last_damage.top = std::min(previous_damage.top, last_damage.top);
            last_damage.right =
                std::max(previous_damage.right, last_damage.right);
            last_damage.bottom =
                std::max(previous_damage.bottom, last_damage.bottom);
          } else {
            surface_damage = last_plane.GetDisplayFrame();
            last_damage = surface_damage;
          }

          surface->UpdateSurfaceDamage(surface_damage, last_damage);
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
  if (tracker.IgnoreUpdate()) {
    return true;
  }

  if (sync_) {
    compositor_.EnsureTasksAreDone();
    sync_ = false;
  }

  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  std::vector<OverlayLayer> layers;
  bool cursor_state_changed = false;
  bool idle_frame = tracker.RenderIdleMode() || idle_update;
  uint32_t previous_frame_cursor_state = cursor_state_;
  cursor_state_ = kNoCursorState;
  bool layers_changed = false;
  *retire_fence = -1;
  uint32_t z_order = 0;
  OverlayLayer* cursor_layer = NULL;
  if (previous_frame_cursor_state & kFrameHasCursor) {
    cursor_state_changed = true;
  }

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;

    layers.emplace_back();
    OverlayLayer& overlay_layer = layers.back();
    OverlayLayer* previous_layer = NULL;
    if (previous_size > z_order) {
      previous_layer = &(in_flight_layers_.at(z_order));
    }

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

      overlay_layer.InitializeFromHwcLayer(layer, buffer_handler_,
                                           previous_layer, z_order, layer_index,
                                           display_frame, true);
    } else {
      overlay_layer.InitializeFromHwcLayer(layer, buffer_handler_,
                                           previous_layer, z_order, layer_index,
                                           layer->GetDisplayFrame(), false);
    }

    if (overlay_layer.IsCursorLayer()) {
      cursor_layer = &overlay_layer;
      cursor_state_ |= kFrameHasCursor;
      if (previous_frame_cursor_state & kFrameHasCursor) {
        cursor_state_changed = false;
      } else {
        cursor_state_changed = true;
      }
    } else if (overlay_layer.HasLayerAttributesChanged()) {
      layers_changed = true;
    }

    z_order++;
  }
  // We may have skipped layers which are not visible.
  size = layers.size();
  bool frame_changed = (size != previous_size);
  bool add_cursor_layer = false;
  // Optimize cursor visibility state change.
  if (!layers_changed && !tracker.RevalidateLayers()) {
    if ((state_ & kConfigurationChanged) || idle_frame) {
      frame_changed = true;
    } else if (frame_changed) {
      if ((size == previous_size) ||
          ((previous_frame_cursor_state & kIgnoredCursorLayer) &&
           (size == previous_size - 1))) {
        frame_changed = false;
      }
    }

    // Let's avoid invalidating the whole cache when cursor state has changed
    // from visible to invisible.
    if (frame_changed && cursor_state_changed) {
      if (previous_size - size == 1) {
        cursor_state_ |= kIgnoredCursorLayer;
      } else if (size - previous_size == 1) {
        // Sometimes this is first frame and we only have cursor layer.
        if (cursor_state_changed && cursor_layer) {
          // Let's add cursor plane as cursor layer has been added.
          add_cursor_layer = true;
        } else {
          layers_changed = true;
        }
      } else {
        layers_changed = true;
      }
    } else if (frame_changed ||
               (cursor_state_changed &&
                !(previous_frame_cursor_state & kCursorIsGpuRendered))) {
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
    GetCachedLayers(layers, cursor_state_ & kIgnoredCursorLayer,
                    add_cursor_layer, &current_composition_planes,
                    &render_layers, &can_ignore_commit);
    if (add_cursor_layer) {
      bool render_cursor = display_plane_manager_->ValidateCursorLayer(
          cursor_layer, current_composition_planes);
      if (!render_layers)
        render_layers = render_cursor;

      if (cursor_layer->IsGpuRendered()) {
        cursor_state_ |= kCursorIsGpuRendered;
      }
    } else if (can_ignore_commit) {
      HandleCommitIgnored(current_composition_planes);
      return true;
    }
  } else {
    if (!idle_frame)
      tracker.ResetTrackerState();

    RecyclePreviousPlaneSurfaces();
    render_layers = display_plane_manager_->ValidateLayers(
        layers, state_ & kConfigurationChanged, idle_frame || disable_ovelays,
        current_composition_planes);
    state_ &= ~kConfigurationChanged;
    if (cursor_layer && cursor_layer->IsGpuRendered()) {
      cursor_state_ |= kCursorIsGpuRendered;
    }
  }

  DUMP_CURRENT_COMPOSITION_PLANES();
  // Handle any 3D Composition.
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

  if (idle_frame) {
    ReleaseSurfaces();
    state_ |= kLastFrameIdleUpdate;
    if (state_ & kClonedMode) {
      idle_tracker_.state_ |= FrameStateTracker::kRenderIdleDisplay;
    }
  } else {
    state_ &= ~kLastFrameIdleUpdate;
    ReleaseSurfacesAsNeeded(validate_layers);
  }

  if (fence > 0) {
    if (!(state_ & kClonedMode)) {
      *retire_fence = dup(fence);
    }
    kms_fence_ = fence;

    SetReleaseFenceToLayers(fence, source_layers);
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
  compositor_.FreeResources(false);
  sync_ = true;
  state_ &= ~kMarkSurfacesForRelease;
  state_ &= ~kReleaseSurfaces;
}

void DisplayQueue::ReleaseSurfacesAsNeeded(bool layers_validated) {
  if (state_ & kReleaseSurfaces) {
    ReleaseSurfaces();
  }

  if (state_ & kMarkSurfacesForRelease) {
    state_ |= kReleaseSurfaces;
    state_ &= ~kMarkSurfacesForRelease;
  }

  if (layers_validated) {
    state_ |= kMarkSurfacesForRelease;
    state_ &= ~kReleaseSurfaces;
  }
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

void DisplayQueue::RecyclePreviousPlaneSurfaces() {
  for (DisplayPlaneState& plane_state : previous_plane_state_) {
    std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    size_t size = surfaces.size();
    // Let's not mark the surface currently on screen as free.
    for (size_t i = 1; i < size; i++) {
      surfaces.at(i)->SetInUse(false);
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

  compositor_.Reset();
  sync_ = true;
  cursor_state_ = kNoCursorState;
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

  size_t size = previous_plane_state_.size();
  if (idle_tracker_.idle_reset_frames_counter == 5) {
    // If we are using more than one plane and have had
    // 5 continuous idle frames, lets reset our counter
    // to fallback to single plane composition when possible.
    if ((idle_tracker_.idle_frames_ > kidleframes) && size > 1)
      idle_tracker_.idle_frames_ = 0;
  } else {
    idle_tracker_.idle_reset_frames_counter++;
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  idle_tracker_.revalidate_frames_counter_ = 0;

  if (size <= 1 || (idle_tracker_.idle_frames_ > kidleframes)) {
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  if (idle_tracker_.idle_frames_ < kidleframes) {
    idle_tracker_.idle_frames_++;
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  idle_tracker_.idle_frames_++;
  power_mode_lock_.lock();
  if (!(state_ & kIgnoreIdleRefresh) && refresh_callback_) {
    refresh_callback_->Callback(refrsh_display_id_);
    idle_tracker_.state_ |= FrameStateTracker::kPrepareIdleComposition;
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
