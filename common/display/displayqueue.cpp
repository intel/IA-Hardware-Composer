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
    : gpu_fd_(gpu_fd), display_(display) {
  if (disable_overlay) {
    state_ |= kDisableOverlayUsage;
  } else {
    state_ &= ~kDisableOverlayUsage;
  }

  vblank_handler_.reset(new VblankEventHandler(this));
  resource_manager_.reset(new ResourceManager(buffer_handler));

  /* use 0x80 as default brightness for all colors */
  brightness_ = 0x808080;
  /* use HWCColorTransform::kIdentical as default color transform hint */
  color_transform_hint_ = HWCColorTransform::kIdentical;
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
  if (!resource_manager_) {
    ETRACE("Failed to construct hwc layer buffer manager");
    return false;
  }

  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, plane_handler, resource_manager_.get()));
  if (!display_plane_manager_->Initialize(width, height)) {
    ETRACE("Failed to initialize DisplayPlane Manager.");
    return false;
  }

  ResetQueue();
  vblank_handler_->SetPowerMode(kOff);
  vblank_handler_->Init(gpu_fd_, pipe);
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
      state_ |= kPoweredOn;
      break;
    case kOn:
      state_ |= kPoweredOn | kConfigurationChanged | kNeedsColorCorrection;
      vblank_handler_->SetPowerMode(kOn);
      power_mode_lock_.lock();
      state_ &= ~kIgnoreIdleRefresh;
      compositor_.Init(resource_manager_.get(),
                       display_plane_manager_->GetGpuFd());
      power_mode_lock_.unlock();
      break;
    default:
      break;
  }

  return true;
}

void DisplayQueue::RotateDisplay(HWCRotation rotation) {
  rotation_ = rotation;
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   bool cursor_layer_removed,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers, bool* can_ignore_commit,
                                   bool* re_validate_commit) {
  CTRACE();
  bool needs_gpu_composition = false;
  bool ignore_commit = !cursor_layer_removed;
  bool needs_revalidation = false;
  for (DisplayPlaneState& plane : previous_plane_state_) {
    if (cursor_layer_removed && plane.IsCursorPlane()) {
      plane.plane()->SetInUse(false);
      continue;
    }

    bool clear_surface = cursor_layer_removed;
    composition->emplace_back();
    DisplayPlaneState& last_plane = composition->back();
    last_plane.CopyState(plane);
    last_plane.AddLayers(plane.source_layers(), layers, cursor_layer_removed);

    if (plane.NeedsOffScreenComposition()) {
      bool content_changed = false;
      bool update_source_rect = false;
      const std::vector<size_t>& source_layers = last_plane.source_layers();
      HwcRect<int> surface_damage = HwcRect<int>(0, 0, 0, 0);
      size_t layers_size = source_layers.size();

      for (size_t i = 0; i < layers_size; i++) {
        size_t source_index = source_layers.at(i);
        const OverlayLayer& layer = layers.at(source_index);
        if (!needs_revalidation)
          needs_revalidation = layer.NeedsRevalidation();

        if (layer.HasDimensionsChanged()) {
          last_plane.UpdateDisplayFrame(layer.GetDisplayFrame());
          clear_surface = true;
        } else if (layer.NeedsToClearSurface()) {
          clear_surface = true;
        }

        if (layer.HasSourceRectChanged() && last_plane.IsUsingPlaneScalar()) {
          last_plane.SetSourceCrop(layer.GetSourceCrop());
          update_source_rect = true;
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
        }
      }

      // Let's make sure we swap the surface if content has changed or
      // we need to clear the surface.
      last_plane.TransferSurfaces(plane.GetSurfaces(),
				  content_changed || clear_surface || update_source_rect);
      std::vector<NativeSurface*>& surfaces = last_plane.GetSurfaces();
      size_t size = surfaces.size();
      if (update_source_rect) {
        const HwcRect<float>& current_rect = last_plane.GetSourceCrop();
        content_changed = true;
        for (size_t i = 0; i < size; i++) {
          surfaces.at(i)->ResetSourceCrop(current_rect);
        }
      }

      if (clear_surface) {
        content_changed = true;
        const HwcRect<int>& current_rect = last_plane.GetDisplayFrame();
        const HwcRect<float>& source_crop = last_plane.GetSourceCrop();
        for (size_t i = 0; i < size; i++) {
          surfaces.at(i)->ResetDisplayFrame(current_rect);
          surfaces.at(i)->ResetSourceCrop(source_crop);
        }
      } else {
        clear_surface = true;
        NativeSurface* surface = last_plane.GetOffScreenTarget();
        if (surface) {
          clear_surface = surface->ClearSurface();
        }

        if (!clear_surface) {
          const std::vector<CompositionRegion>& comp_regions =
              plane.GetCompositionRegion();
          last_plane.GetCompositionRegion().assign(comp_regions.begin(),
                                                   comp_regions.end());
        } else {
          content_changed = true;
        }
      }

      if (content_changed) {
        if (last_plane.GetSurfaces().size() == 3) {
          if (!clear_surface) {
            HwcRect<int> last_damage;
            std::vector<NativeSurface*>& surfaces = last_plane.GetSurfaces();
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
            surfaces.at(0)->UpdateSurfaceDamage(surface_damage, last_damage);
          }
        } else {
          display_plane_manager_->SetOffScreenPlaneTarget(last_plane);
        }

        // We always call this here to handle case where we recycled
        // offscreen surface for last commit.
        last_plane.ForceGPURendering();
        needs_gpu_composition = true;
      } else {
        last_plane.ReUseOffScreenTarget();
      }
    } else {
      const OverlayLayer* layer =
          &(*(layers.begin() + last_plane.source_layers().front()));
      if (layer->GetBuffer()->GetFb() == 0) {
        layer->GetBuffer()->CreateFrameBuffer(gpu_fd_);
      }

      last_plane.SetOverlayLayer(layer);
      if (layer->HasLayerContentChanged() || layer->HasDimensionsChanged()) {
        ignore_commit = false;
      }

      if (!needs_revalidation) {
        needs_revalidation = layer->NeedsRevalidation();
      }
    }
  }

  *render_layers = needs_gpu_composition;
  if (needs_gpu_composition)
    ignore_commit = false;

  *can_ignore_commit = ignore_commit;
  *re_validate_commit = needs_revalidation;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence, bool idle_update,
                               bool handle_constraints) {
  CTRACE();
  ScopedIdleStateTracker tracker(idle_tracker_, compositor_);
  if (tracker.IgnoreUpdate()) {
    return true;
  }

  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  std::vector<OverlayLayer> layers;
  std::vector<OverlayLayer*> cursor_layers;
  uint32_t new_layers = 0;
  bool idle_frame = tracker.RenderIdleMode() || idle_update;
  uint32_t previous_cursor_layers = total_cursor_layers_;
  bool layers_changed = tracker.RevalidateLayers();
  *retire_fence = -1;
  uint32_t z_order = 0;
  bool has_video_layer = false;

  resource_manager_->RefreshBufferCache();
  total_cursor_layers_ = 0;

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;

    layers.emplace_back();
    OverlayLayer* overlay_layer = &(layers.back());
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

      overlay_layer->InitializeFromScaledHwcLayer(
          layer, resource_manager_.get(), previous_layer, z_order, layer_index,
          display_frame, display_plane_manager_->GetHeight(), rotation_,
          handle_constraints);
    } else {
      overlay_layer->InitializeFromHwcLayer(
          layer, resource_manager_.get(), previous_layer, z_order, layer_index,
          display_plane_manager_->GetHeight(), rotation_, handle_constraints);
    }

    if (!overlay_layer->IsVisible()) {
      layers.pop_back();
      continue;
    }

    if (overlay_layer->IsVideoLayer()) {
      has_video_layer = true;
    } else if (overlay_layer->IsCursorLayer()) {
      total_cursor_layers_++;
      cursor_layers.emplace_back(overlay_layer);
    } else if (!previous_layer ||
               (previous_layer && previous_layer->IsCursorLayer())) {
      // Cursor layer is replaced by another layer or a new layer has
      // been created.
      new_layers++;
    }

    z_order++;
  }

  // We may have skipped layers which are not visible.
  size = layers.size();
  bool add_cursor_layer = false;
  bool removed_cursor_layer = false;

  if (!layers_changed) {
    // New non cursor layers have been added. Force
    // a re-validation.
    if (new_layers > 0) {
      layers_changed = true;
    } else if (total_cursor_layers_ == previous_cursor_layers) {
      // This case is when non-cursor layer has been removed and we
      // don't have any cursor layers or we have cursor layers and
      // non cursor layer has been removed.
      // TODO: We should be able to optimize without forcing a full
      // validation.
      if (previous_size != size) {
        layers_changed = true;
      }
    } else if (previous_cursor_layers > total_cursor_layers_) {
      // Cursor layer has been removed.
      if ((previous_size - size) !=
          (previous_cursor_layers - total_cursor_layers_)) {
        // TODO: We should be able to optimize without forcing a full
        // validation.
        layers_changed = true;
      } else {
        // We should have "new_layers" set above in case cursor layer was
        // removed
        // and a new layer added.
        removed_cursor_layer = true;
      }
    } else if (total_cursor_layers_ > previous_cursor_layers) {
      // If here, no new layers apart from cursor have been added for this
      // frame.
      add_cursor_layer = true;
    }
  }

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  bool validate_layers = layers_changed;
  bool force_media_composition = false;
  bool requested_video_effect = false;
  video_lock_.lock();
  if (requested_video_effect_ != applied_video_effect_) {
    // Let's ensure Media planes take this into account.
    force_media_composition = true;
    applied_video_effect_ = requested_video_effect_;
    requested_video_effect = requested_video_effect_;
  }
  video_lock_.unlock();

  bool composition_passed = true;
  bool disable_ovelays = state_ & kDisableOverlayUsage;

  // Validate Overlays and Layers usage.
  if (!validate_layers) {
    bool can_ignore_commit = false;
    bool request_full_validation = false;
    bool re_validate_commit = false;
    // Before forcing layer validation, check if content has changed
    // if not continue showing the current buffer.
    GetCachedLayers(layers, removed_cursor_layer, &current_composition_planes,
                    &render_layers, &can_ignore_commit, &re_validate_commit);

    // Let's re-validate if their was a request for this.
    if (re_validate_commit) {
      display_plane_manager_->ReValidateLayers(
          layers, current_composition_planes, &request_full_validation);
    }

    if (!request_full_validation && force_media_composition) {
      SetMediaEffectsState(requested_video_effect, layers,
                           current_composition_planes);
      if (requested_video_effect) {
        render_layers = true;
      }

      can_ignore_commit = false;
    }

    if (request_full_validation) {
      validate_layers = true;
      UpdateSurfaceInUse(false, current_composition_planes);
      UpdateSurfaceInUse(true, previous_plane_state_);
      DisplayPlaneStateList().swap(current_composition_planes);
    } else if (add_cursor_layer) {
      bool render_cursor = display_plane_manager_->ValidateCursorLayer(
          cursor_layers, current_composition_planes);
      if (!render_layers)
        render_layers = render_cursor;
    } else if (can_ignore_commit) {
      IgnoreCompositionResults(current_composition_planes);
      return true;
    }
  }

  if (validate_layers) {
    if (!idle_frame)
      tracker.ResetTrackerState();

    RecyclePreviousPlaneSurfaces();
    render_layers = display_plane_manager_->ValidateLayers(
        layers, cursor_layers, state_ & kConfigurationChanged,
        idle_frame || disable_ovelays, current_composition_planes);
    // If Video effects need to be applied, let's make sure
    // we go through the composition pass for Video Layers.
    if (force_media_composition && requested_video_effect) {
      SetMediaEffectsState(requested_video_effect, layers,
                           current_composition_planes);
      render_layers = true;
    }
    state_ &= ~kConfigurationChanged;
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
    IgnoreCompositionResults(current_composition_planes);
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
    display_->SetColorTransformMatrix(color_transform_matrix_, color_transform_hint_);
    state_ &= ~kNeedsColorCorrection;
  }

  composition_passed =
      display_->Commit(current_composition_planes, previous_plane_state_,
                       disable_ovelays, &fence);

  if (!composition_passed) {
    IgnoreCompositionResults(current_composition_planes);
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

#ifdef ENABLE_DOUBLE_BUFFERING
  if (kms_fence_ > 0) {
    HWCPoll(kms_fence_, -1);
    close(kms_fence_);
    kms_fence_ = 0;
  }
#endif

  // Let Display handle any lazy initalizations.
  if (handle_display_initializations_) {
    handle_display_initializations_ = false;
    display_->HandleLazyInitialization();
  }

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

void DisplayQueue::IgnoreUpdates() {
  idle_tracker_.idle_frames_ = 0;
  idle_tracker_.state_ = FrameStateTracker::kIgnoreUpdates;
  idle_tracker_.revalidate_frames_counter_ = 0;
  idle_tracker_.idle_reset_frames_counter_ = 0;
}

void DisplayQueue::ReleaseSurfaces() {
  display_plane_manager_->ReleaseFreeOffScreenTargets();
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

void DisplayQueue::SetMediaEffectsState(
    bool apply_effects, const std::vector<OverlayLayer>& layers,
    DisplayPlaneStateList& current_composition_planes) const {
  for (DisplayPlaneState& plane : current_composition_planes) {
    if (!plane.IsVideoPlane()) {
      continue;
    }

    plane.SetApplyEffects(apply_effects);
    std::vector<NativeSurface*>& surfaces = plane.GetSurfaces();
    // Handle case where we enable effects but video plane is currently
    // scanned out directly. In this case we will need to ensure we
    // have a offscreen surface to render to.
    if (apply_effects && surfaces.empty()) {
      display_plane_manager_->SetOffScreenPlaneTarget(plane);
    } else if (!apply_effects && !surfaces.empty() && plane.Scanout()) {
      // Handle case where we disable effects but video plane can be
      // scanned out directly. In this case we will need to delete all
      // offscreen surfaces and set the right overlayer layer to the
      // plane.
      for (NativeSurface* surface : surfaces) {
        surface->SetInUse(false);
      }

      std::vector<NativeSurface*>().swap(plane.GetSurfaces());
      const std::vector<size_t>& source = plane.source_layers();
      plane.SetOverlayLayer(&(layers.at(source.at(0))));
    }
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

void DisplayQueue::IgnoreCompositionResults(
    DisplayPlaneStateList& current_composition_planes) {
  UpdateSurfaceInUse(false, current_composition_planes);
  UpdateSurfaceInUse(true, previous_plane_state_);
  ReleaseSurfaces();
}

void DisplayQueue::SetReleaseFenceToLayers(
    int32_t fence, std::vector<HwcLayer*>& source_layers) const {
  for (const DisplayPlaneState& plane : previous_plane_state_) {
    const std::vector<size_t>& layers = plane.source_layers();
    size_t size = layers.size();
    int32_t release_fence = -1;
    if (plane.Scanout() && !plane.SurfaceRecycled()) {
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
  IHOTPLUGEVENTTRACE("HandleExit Called: %p \n", this);
  power_mode_lock_.lock();
  state_ |= kIgnoreIdleRefresh;
  power_mode_lock_.unlock();
  vblank_handler_->SetPowerMode(kOff);
  if (!previous_plane_state_.empty()) {
    display_->Disable(previous_plane_state_);
  }

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

  ResetQueue();
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

void DisplayQueue::SetColorTransform(const float *matrix, HWCColorTransform hint) {
  color_transform_hint_ = hint;

  if (hint == HWCColorTransform::kArbitraryMatrix) {
    memcpy(color_transform_matrix_, matrix, sizeof(color_transform_matrix_));
  }

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

void DisplayQueue::SetVideoColor(HWCColorControl color, float value) {
  video_lock_.lock();
  requested_video_effect_ = true;
  compositor_.SetVideoColor(color, value);
  video_lock_.unlock();
}

void DisplayQueue::GetVideoColor(HWCColorControl color, float* value,
                                 float* start, float* end) {
  compositor_.GetVideoColor(color, value, start, end);
}

void DisplayQueue::RestoreVideoDefaultColor(HWCColorControl color) {
  video_lock_.lock();
  requested_video_effect_ = false;
  compositor_.RestoreVideoDefaultColor(color);
  video_lock_.unlock();
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
  if (idle_tracker_.idle_reset_frames_counter_ == 5) {
    // If we are using more than one plane and have had
    // 5 continuous idle frames, lets reset our counter
    // to fallback to single plane composition when possible.
    if ((idle_tracker_.idle_frames_ > kidleframes) && size > 1)
      idle_tracker_.idle_frames_ = 0;
  } else {
    idle_tracker_.idle_reset_frames_counter_++;
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
  if (!(state_ & kIgnoreIdleRefresh) && refresh_callback_ &&
      (state_ & kPoweredOn)) {
    refresh_callback_->Callback(refrsh_display_id_);
    idle_tracker_.state_ |= FrameStateTracker::kPrepareIdleComposition;
  }
  power_mode_lock_.unlock();
  idle_tracker_.idle_lock_.unlock();
}
void DisplayQueue::ForceRefresh() {
  idle_tracker_.idle_lock_.lock();
  idle_tracker_.state_ &= ~FrameStateTracker::kIgnoreUpdates;
  idle_tracker_.state_ |= FrameStateTracker::kRevalidateLayers;
  idle_tracker_.idle_lock_.unlock();
  power_mode_lock_.lock();
  if (!(state_ & kIgnoreIdleRefresh) && refresh_callback_ &&
      (state_ & kPoweredOn)) {
    refresh_callback_->Callback(refrsh_display_id_);
  }
  power_mode_lock_.unlock();
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

void DisplayQueue::ResetQueue() {
  total_cursor_layers_ = 0;
  applied_video_effect_ = false;
  std::vector<OverlayLayer>().swap(in_flight_layers_);
  DisplayPlaneStateList().swap(previous_plane_state_);
  if (display_plane_manager_->HasSurfaces())
    display_plane_manager_->ReleaseAllOffScreenTargets();

  resource_manager_->PurgeBuffer();
  bool ignore_updates = false;
  if (idle_tracker_.state_ & FrameStateTracker::kIgnoreUpdates) {
    ignore_updates = true;
  }

  idle_tracker_.state_ = 0;
  idle_tracker_.idle_frames_ = 0;
  if (ignore_updates) {
    idle_tracker_.state_ |= FrameStateTracker::kIgnoreUpdates;
  }
  compositor_.Reset();
}

}  // namespace hwcomposer
