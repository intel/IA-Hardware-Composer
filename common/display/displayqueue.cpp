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

  display_plane_manager_->SetDisplayTransform(plane_transform_);
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
  switch (rotation) {
    case kRotate90:
      plane_transform_ |= kTransform90;
      break;
    case kRotate270:
      plane_transform_ |= kTransform270;
      break;
    case kRotate180:
      plane_transform_ |= kTransform180;
      break;
    default:
      break;
  }

  display_plane_manager_->SetDisplayTransform(plane_transform_);
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   int remove_index,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers, bool* can_ignore_commit,
                                   bool* needs_plane_validation,
                                   bool* force_full_validation) {
  CTRACE();
  bool needs_gpu_composition = false;
  bool ignore_commit = true;
  bool check_to_squash = false;
  bool plane_validation = false;
  // If Scanout layers DisplayFrame rect has changed, we need
  // to re-calculate our Composition regions for planes using
  // GPU Composition.
  bool reset_composition_regions = false;

  for (DisplayPlaneState& previous_plane : previous_plane_state_) {
    bool clear_surface = false;
    composition->emplace_back();
    DisplayPlaneState& last_plane = composition->back();
    last_plane.CopyState(previous_plane);
    if (remove_index != -1) {
      const std::vector<size_t>& source_layers = last_plane.GetSourceLayers();
      const size_t& index = source_layers.at(source_layers.size() - 1);
      size_t threshold = static_cast<size_t>(remove_index);
      if (index >= threshold) {
        bool has_one_layer = source_layers.size() == 1 ? true : false;
        if (!has_one_layer) {
#ifdef SURFACE_TRACING
          ISURFACETRACE(
              "Plane has more than one layer, calling Reset Layers. Threshold: "
              "%d Plane Layer Index: %d Total Planes: %d \n",
              threshold, index, previous_plane_state_.size());
#endif
          last_plane.ResetLayers(layers, threshold);
          clear_surface = true;
        }
        // We need to force re-validation of commit to ensure we update any
        // Scalar usage with the new combination of layers.
        ignore_commit = false;

        if (last_plane.GetSourceLayers().empty() || has_one_layer) {
          display_plane_manager_->MarkSurfacesForRecycling(
              &last_plane, surfaces_not_inuse_, false);
          // On some platforms disabling primary disables
          // the whole pipe. Let's revalidate the new layers
          // and ensure primary has a buffer.
          if (last_plane.GetDisplayPlane() ==
              previous_plane_state_.begin()->GetDisplayPlane()) {
#ifdef SURFACE_TRACING
            ISURFACETRACE("Primary plane is empty forcing full validation. \n");
#endif
            *force_full_validation = true;
            return;
          }

          last_plane.GetDisplayPlane()->SetInUse(false);
          composition->pop_back();
          continue;
        }

        last_plane.ValidateReValidation();

        if (last_plane.RevalidationType() &
            DisplayPlaneState::ReValidationType::kScanout) {
          const std::vector<size_t>& source_layers =
              last_plane.GetSourceLayers();
          const OverlayLayer* layer = &(layers.at(source_layers.at(0)));
          // Check if Actual & Supported Composition differ for this
          // layer. If so than let' mark it for validation.
          if (layer->CanScanOut() && last_plane.NeedsOffScreenComposition()) {
            plane_validation = true;
          } else if (source_layers.size() == 1) {
            check_to_squash = true;
            last_plane.RevalidationDone(
                DisplayPlaneState::ReValidationType::kScanout);
          }
        }
      }
    }

    if (last_plane.NeedsOffScreenComposition()) {
      HwcRect<int> surface_damage = HwcRect<int>(0, 0, 0, 0);
      bool content_changed = false;
      bool update_rect = false;
      bool update_source_rect = false;

      if (!clear_surface) {
        const std::vector<size_t>& source_layers = last_plane.GetSourceLayers();
        size_t layers_size = source_layers.size();

        HwcRect<int> display_frame = last_plane.GetDisplayFrame();
        HwcRect<float> source_crop = last_plane.GetSourceCrop();

        for (size_t i = 0; i < layers_size; i++) {
          const size_t& source_index = source_layers.at(i);
          const OverlayLayer& layer = layers.at(source_index);
          if (layer.HasDimensionsChanged()) {
            last_plane.UpdateDisplayFrame(layer.GetDisplayFrame());
            update_rect = true;
          }

          if (layer.HasSourceRectChanged()) {
            last_plane.UpdateSourceCrop(layer.GetSourceCrop());
            update_source_rect = true;
          }

          if (layer.HasLayerContentChanged()) {
            const HwcRect<int>& damage = layer.GetSurfaceDamage();
            if (content_changed) {
              surface_damage.left = std::min(surface_damage.left, damage.left);
              surface_damage.top = std::min(surface_damage.top, damage.top);
              surface_damage.right =
                  std::max(surface_damage.right, damage.right);
              surface_damage.bottom =
                  std::max(surface_damage.bottom, damage.bottom);
            } else {
              surface_damage = damage;
            }
            content_changed = true;
          }
        }

        if (update_rect || update_source_rect) {
          content_changed = true;
          // Let's check if we need to check this plane-layer combination.
          last_plane.ValidateReValidation();
          if (last_plane.RevalidationType() !=
              DisplayPlaneState::ReValidationType::kNone) {
            plane_validation = true;
          }

          bool rect_updated = update_rect;
          bool source_rect_updated = update_source_rect;
          if (update_rect && (last_plane.GetDisplayFrame() == display_frame)) {
            rect_updated = false;
          }

          if (update_source_rect &&
              (last_plane.GetSourceCrop() == source_crop)) {
            source_rect_updated = false;
          }

          if (rect_updated) {
            last_plane.PlaneRectUpdated();
          }

          // If reset_composition_regions is true we will mark the whole
          // plane as damaged below. We can ignore setting the flag here.
          if (!reset_composition_regions &&
              (update_rect ||
               (update_source_rect && last_plane.IsUsingPlaneScalar()))) {
            last_plane.RefreshSurfaces(NativeSurface::kPartialClear, true);
            update_rect = true;
          }
        }
      }

      // If surfaces need to be cleared or rect is updated,
      // let's make sure all surfaces are refreshed.
      if (clear_surface) {
        content_changed = true;
        last_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
      }

      // Let's make sure we swap the surface in case content has changed.
      size_t total_surfaces = last_plane.GetSurfaces().size();
      if (content_changed && total_surfaces == 3) {
        last_plane.SwapSurfaceIfNeeded();
      }

      // Let's get the state from surface if it needs to be cleared.
      if (!clear_surface) {
        // If a rect intersects one of the dedicated layers, we need to remove
        // the layers from the composition region which appear *below* the
        // dedicated
        // layer. This effectively punches a hole through the composition layer
        // such that the dedicated layer can be placed below the composition and
        // not
        // be occluded. Rest composition regions in case
        // reset_composition_regions is true.
        if (reset_composition_regions) {
          surface_damage = last_plane.GetDisplayFrame();
          last_plane.RefreshSurfaces(NativeSurface::kPartialClear, true);
          if (!content_changed && total_surfaces == 3) {
            last_plane.SwapSurfaceIfNeeded();
          }

          content_changed = true;
        }
      }

      if (content_changed) {
        if (total_surfaces != 3) {
          display_plane_manager_->SetOffScreenPlaneTarget(last_plane);
        }

        // Make sure all rects are correct.
        if (!clear_surface && !surface_damage.empty()) {
          last_plane.UpdateDamage(surface_damage,
                                  reset_composition_regions || update_rect);
        }

        // We always call this here to handle case where we recycled
        // offscreen surface for last commit.
        last_plane.ForceGPURendering();
        needs_gpu_composition = true;
        reset_composition_regions = false;
      } else {
        last_plane.ReUseOffScreenTarget();
      }
    } else {
      const OverlayLayer* layer =
          &(layers.at(last_plane.GetSourceLayers().front()));
      OverlayBuffer* buffer = layer->GetBuffer();
      if (buffer->GetFb() == 0) {
        buffer->CreateFrameBuffer(gpu_fd_);

        // FB creation failed, we need to re-validate the
        // whole commit.
        if (buffer->GetFb() == 0) {
          *force_full_validation = true;
          ignore_commit = false;
          break;
        }
      }

      last_plane.SetOverlayLayer(layer);
      if (layer->HasLayerContentChanged()) {
        ignore_commit = false;
      }

      if (layer->HasDimensionsChanged()) {
        ignore_commit = false;
        reset_composition_regions = true;
      }
    }
  }

  *render_layers = needs_gpu_composition;
  if (needs_gpu_composition)
    ignore_commit = false;

  *can_ignore_commit = ignore_commit;
  *needs_plane_validation = plane_validation;

  // Check if we can squash the last overlay (Before Cursor Plane).
  if (check_to_squash) {
    size_t size = composition->size();
    if (composition->back().IsCursorPlane()) {
      // We cannot squash Cursor plane.
      size -= 1;
    }

    if (size > 2) {
      DisplayPlaneState& old_plane = composition->at(size - 2);
      DisplayPlaneState& last_overlay = composition->at(size - 1);
      const std::vector<size_t>& source_layers = last_overlay.GetSourceLayers();
      const OverlayLayer* layer = &(layers.at(source_layers.at(0)));

      if (old_plane.CanSquash() && last_overlay.CanSquash() &&
          source_layers.size() == 1) {
#ifdef SURFACE_TRACING
        ISURFACETRACE(
            "Moving layer index %d from plane index: %d to plane idex: %d. \n",
            source_layers.at(0), size - 1, size - 2);
#endif
        old_plane.AddLayer(layer);
        last_overlay.GetDisplayPlane()->SetInUse(false);
        composition->erase(composition->begin() + (size - 1));
      }
    }
  }
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence, bool idle_update,
                               bool handle_constraints) {
  CTRACE();
  ScopedIdleStateTracker tracker(idle_tracker_, compositor_,
                                 resource_manager_.get());
  if (tracker.IgnoreUpdate()) {
    return true;
  }

  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  std::vector<OverlayLayer> layers;
  int remove_index = -1;
  int add_index = -1;
  bool idle_frame = tracker.RenderIdleMode() || idle_update;
  // If last commit failed, lets force full validation as
  // state might be all wrong in our side.
  bool validate_layers =
      last_commit_failed_update_ || previous_plane_state_.empty() || idle_frame;
  *retire_fence = -1;
  uint32_t z_order = 0;
  bool has_video_layer = false;
  bool re_validate_commit = false;

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
    } else if (add_index == -1) {
      add_index = z_order;
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
          display_frame, display_plane_manager_->GetHeight(), plane_transform_,
          handle_constraints);
    } else {
      overlay_layer->InitializeFromHwcLayer(
          layer, resource_manager_.get(), previous_layer, z_order, layer_index,
          display_plane_manager_->GetHeight(), plane_transform_,
          handle_constraints);
    }

    if (!overlay_layer->IsVisible()) {
      layers.pop_back();
      continue;
    }

    if (overlay_layer->IsVideoLayer()) {
      has_video_layer = true;
    }

    if (overlay_layer->NeedsRevalidation()) {
      re_validate_commit = true;
    }

    z_order++;
    if (add_index == 0 || validate_layers ||
        ((add_index != -1) && (remove_index != -1))) {
      continue;
    }

    // Handle case where Cursor layer has been destroyed/created or has changed
    // z-order.
    if (previous_layer &&
        previous_layer->IsCursorLayer() != overlay_layer->IsCursorLayer()) {
      if (remove_index == -1)
        remove_index = previous_layer->GetZorder();

      // Treat this case as if a new layer has been created.
      if (add_index == -1)
        add_index = overlay_layer->GetZorder();

#ifdef SURFACE_TRACING
      ISURFACETRACE(
          "Cursor layer has changed between frames: remove_index: %d "
          "add_index: "
          "%d \n",
          remove_index, add_index);
#endif
    }

    // Handle case where Media layer has been destroyed/created or has changed
    // z-order.
    if (previous_layer &&
        previous_layer->IsVideoLayer() != overlay_layer->IsVideoLayer()) {
      if (remove_index == -1)
        remove_index = previous_layer->GetZorder();

      // Treat this case as if a new layer has been created.
      if (add_index == -1)
        add_index = overlay_layer->GetZorder();
#ifdef SURFACE_TRACING
      ISURFACETRACE(
          "Video layer has changed between frames: remove_index: %d add_index: "
          "%d \n",
          remove_index, add_index);
#endif
    }
  }

  // We may have skipped layers which are not visible.
  size = layers.size();
  if ((add_index == 0) || validate_layers) {
    // If index is zero, no point trying for incremental validation.
    validate_layers = true;
  } else if (previous_size > size) {
    if (remove_index == -1) {
      remove_index = size;
    } else if (add_index != -1) {
      remove_index = std::min(add_index, remove_index);
    }
  }

#ifdef SURFACE_TRACING
  if ((remove_index != -1) || (add_index != -1)) {
    ISURFACETRACE(
        "Remove index For this Frame: %d Add index For this Frame: %d Total "
        "Layers: %d \n",
        remove_index, add_index, layers.size());
  }

  if (validate_layers) {
    ISURFACETRACE(
        "Full Validation Forced: add_index: %d last_commit_failed_update_: %d "
        "tracker.RevalidateLayers(): %d  previous_plane_state_.empty(): %d "
        "tracker.RenderIdleMode():%d idle_update:%d \n",
        add_index, last_commit_failed_update_, tracker.RevalidateLayers(),
        previous_plane_state_.empty(), tracker.RenderIdleMode(), idle_update);
  }
#endif

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
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
  // If we are in idle mode, we can postpone re-validation
  // in case we are only removing layers and no new layers
  // are being added.
  if (tracker.RevalidateLayers()) {
    if (!validate_layers && tracker.RevalidateLayers() &&
        (add_index != -1 || re_validate_commit)) {
      validate_layers = true;
    } else {
      tracker.PostponeRevalidation();
    }
  }

  // Validate Overlays and Layers usage.
  if (!validate_layers) {
    bool can_ignore_commit = false;
    // Before forcing layer validation, check if content has changed
    // if not continue showing the current buffer.
    bool commit_checked = false;
    bool needs_plane_validation = false;
    GetCachedLayers(layers, remove_index, &current_composition_planes,
                    &render_layers, &can_ignore_commit, &needs_plane_validation,
                    &validate_layers);

    if (!validate_layers && add_index > 0) {
      bool render_cursor = display_plane_manager_->ValidateLayers(
          layers, add_index, disable_ovelays, &commit_checked,
          &needs_plane_validation, current_composition_planes,
          previous_plane_state_, surfaces_not_inuse_);

      if (!render_layers)
        render_layers = render_cursor;
      can_ignore_commit = false;
      if (commit_checked)
        re_validate_commit = false;
    }

    if (!validate_layers && (re_validate_commit || needs_plane_validation)) {
      bool render = display_plane_manager_->ReValidatePlanes(
          current_composition_planes, layers, surfaces_not_inuse_,
          &validate_layers, needs_plane_validation, re_validate_commit);
      can_ignore_commit = false;
      if (!render_layers)
        render_layers = render;
    }

    if (!validate_layers) {
      if (force_media_composition) {
        SetMediaEffectsState(requested_video_effect, layers,
                             current_composition_planes);
        if (requested_video_effect) {
          render_layers = true;
        }

        can_ignore_commit = false;
      }

      if (can_ignore_commit) {
        in_flight_layers_.swap(layers);
        return true;
      }
    }
  }

  // Reset last commit failure state.
  last_commit_failed_update_ = false;

  if (validate_layers) {
    if (!idle_frame)
      tracker.ResetTrackerState();

    // We are doing a full re-validation.
    add_index = 0;
    bool force_gpu = disable_ovelays || idle_frame ||
                     ((state_ & kConfigurationChanged) && (layers.size() > 1));
    bool test_commit = false;
    render_layers = display_plane_manager_->ValidateLayers(
        layers, add_index, force_gpu, &test_commit, &test_commit,
        current_composition_planes, previous_plane_state_, surfaces_not_inuse_);
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
  DUMP_CURRENT_LAYER_PLANE_COMBINATIONS();
  DUMP_CURRENT_DUPLICATE_LAYER_COMBINATIONS();
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
    last_commit_failed_update_ = true;
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
    display_->SetColorTransformMatrix(color_transform_matrix_,
                                      color_transform_hint_);
    state_ &= ~kNeedsColorCorrection;
  }

  composition_passed =
      display_->Commit(current_composition_planes, previous_plane_state_,
                       disable_ovelays, &fence);

  if (!composition_passed) {
    last_commit_failed_update_ = true;
    return false;
  }

  // Mark any surfaces as not in use. These surfaces
  // where not marked earlier as they where onscreen.
  // Doing it here also ensures that if this surface
  // is still in use than it will be marked in use
  // below.
  if (!mark_not_inuse_.empty()) {
    size_t size = mark_not_inuse_.size();
    for (uint32_t i = 0; i < size; i++) {
      mark_not_inuse_.at(i)->SetInUse(false);
    }

    std::vector<NativeSurface*>().swap(mark_not_inuse_);
  }

  in_flight_layers_.swap(layers);

  // Swap current and previous composition results.
  previous_plane_state_.swap(current_composition_planes);

  // Set Age for all offscreen surfaces.
  UpdateOnScreenSurfaces();

  // Swap any surfaces which are to be marked as not in
  // use next frame.
  if (!surfaces_not_inuse_.empty()) {
    size_t size = surfaces_not_inuse_.size();
    std::vector<NativeSurface*> temp;
    for (uint32_t i = 0; i < size; i++) {
      NativeSurface* surface = surfaces_not_inuse_.at(i);
      uint32_t age = surface->GetSurfaceAge();
      if (age > 0) {
        temp.emplace_back(surface);
        surface->SetSurfaceAge(surface->GetSurfaceAge() - 1);
        surface->SetInUse(true);
      } else {
        mark_not_inuse_.emplace_back(surface);
      }
    }

    surfaces_not_inuse_.swap(temp);
  }

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
  if (!layers_validated && (state_ & kReleaseSurfaces)) {
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
    DisplayPlaneStateList& current_composition_planes) {
  for (DisplayPlaneState& plane : current_composition_planes) {
    if (!plane.IsVideoPlane()) {
      continue;
    }

    plane.SetApplyEffects(apply_effects);
    const std::vector<NativeSurface*>& surfaces = plane.GetSurfaces();
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
      display_plane_manager_->MarkSurfacesForRecycling(
          &plane, surfaces_not_inuse_, false);
      const std::vector<size_t>& source = plane.GetSourceLayers();
      plane.SetOverlayLayer(&(layers.at(source.at(0))));
    }

    plane.SwapSurfaceIfNeeded();
  }
}

void DisplayQueue::UpdateOnScreenSurfaces() {
  for (DisplayPlaneState& plane_state : previous_plane_state_) {
    const std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    if (surfaces.empty())
      continue;

    size_t size = surfaces.size();
    if (size == 3) {
      NativeSurface* surface = surfaces.at(1);
      surface->SetSurfaceAge(0);
      surface->SetInUse(true);

      surface = surfaces.at(0);
      surface->SetSurfaceAge(2);
      surface->SetInUse(true);

      surface = surfaces.at(2);
      surface->SetSurfaceAge(1);
      surface->SetInUse(true);
    } else {
      for (uint32_t i = 0; i < size; i++) {
        NativeSurface* surface = surfaces.at(i);
        surface->SetSurfaceAge(2 - i);
        surface->SetInUse(true);
      }
    }
#ifdef COMPOSITOR_TRACING
    // Swap any surfaces which are to be marked as not in
    // use next frame.
    if (!surfaces_not_inuse_.empty()) {
      size_t n_size = surfaces_not_inuse_.size();
      for (uint32_t j = 0; j < n_size; j++) {
        NativeSurface* temp = surfaces_not_inuse_.at(j);
        bool found = false;
        for (uint32_t k = 0; k < size; k++) {
          NativeSurface* surface = surfaces.at(k);
          if (temp == surface) {
            found = true;
            ICOMPOSITORTRACE(
                "ALERT: Found a surface in re-cycling queue being used by "
                "current surface. \n");
            break;
          }
        }
      }
    }
#endif
  }
}

void DisplayQueue::SetReleaseFenceToLayers(
    int32_t fence, std::vector<HwcLayer*>& source_layers) {
  for (const DisplayPlaneState& plane : previous_plane_state_) {
    const std::vector<size_t>& layers = plane.GetSourceLayers();
    size_t size = layers.size();
    int32_t release_fence = -1;
    if (plane.Scanout() && !plane.SurfaceRecycled()) {
      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        OverlayLayer& overlay_layer =
            in_flight_layers_.at(layers.at(layer_index));
        HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
        layer->SetReleaseFence(dup(fence));
        overlay_layer.SetLayerComposition(OverlayLayer::kDisplay);
      }
    } else {
      release_fence = plane.GetOverlayLayer()->ReleaseAcquireFence();

      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        OverlayLayer& overlay_layer =
            in_flight_layers_.at(layers.at(layer_index));
        overlay_layer.SetLayerComposition(OverlayLayer::kGpu);
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
  applied_video_effect_ = false;
  last_commit_failed_update_ = false;
  std::vector<OverlayLayer>().swap(in_flight_layers_);
  DisplayPlaneStateList().swap(previous_plane_state_);
  std::vector<NativeSurface*>().swap(mark_not_inuse_);
  std::vector<NativeSurface*>().swap(surfaces_not_inuse_);
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
