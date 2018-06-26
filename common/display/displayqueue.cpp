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

#include <hwcdefs.h>
#include <hwclayer.h>
#include <math.h>

#include <vector>

#include "displayplanemanager.h"
#include "hwctrace.h"
#include "hwcutils.h"
#include "nativesurface.h"
#include "overlaylayer.h"
#include "vblankeventhandler.h"

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
  /* use 0x0 (black) as default canvas/background color for pipe */
  canvas_.bpc = 8;
  canvas_.red = 0x0;
  canvas_.green = 0x0;
  canvas_.blue = 0x0;
  canvas_.alpha = 0x0;
  state_ |= kNeedsColorCorrection;
}

DisplayQueue::~DisplayQueue() {
}

bool DisplayQueue::Initialize(uint32_t pipe, uint32_t width, uint32_t height,
                              DisplayPlaneHandler* plane_handler,
                              FrameBufferManager* frame_buffer_manager) {
  fb_manager_ = frame_buffer_manager;
  if (!resource_manager_) {
    ETRACE("Failed to construct hwc layer buffer manager");
    return false;
  }

  display_plane_manager_.reset(
      new DisplayPlaneManager(plane_handler, resource_manager_.get()));
  if (!display_plane_manager_->Initialize(width, height, fb_manager_)) {
    ETRACE("Failed to initialize DisplayPlane Manager.");
    return false;
  }

  display_plane_manager_->SetDisplayTransform(plane_transform_);
  display_plane_manager_->SetLastPlaneUsage(!enable_wa_);
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
      compositor_.Init(resource_manager_.get(), gpu_fd_, fb_manager_);
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

bool DisplayQueue::ForcePlaneValidation(int add_index, int remove_index,
                                        int total_layers_size,
                                        size_t total_planes) {
  // This is the case where cursor has changed z_order.
  if (remove_index == add_index)
    return false;

  int free_planes = display_plane_manager_->GetTotalOverlays() - total_planes;
  if (free_planes <= 0) {
    return false;
  }

  // Bail out in case we have new layers to fill the remaining planes.
  if ((add_index != -1) && (total_layers_size - add_index) > free_planes) {
    return false;
  }

  return true;
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   int remove_index,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers, bool* can_ignore_commit,
                                   bool* needs_plane_validation,
                                   bool* force_full_validation,
                                   int* add_index) {
  CTRACE();
  bool needs_gpu_composition = false;
  bool ignore_commit = true;
  bool check_to_squash = false;
  bool plane_validation = false;
  // If Scanout layers DisplayFrame rect has changed, we need
  // to re-calculate our Composition regions for planes using
  // GPU Composition.
  bool reset_composition_regions = false;
  bool reset_plane = remove_index != -1 ? true : false;
  bool removed_layers = false;
  bool pop_plane = false;
  size_t previous_size = 0;

  for (DisplayPlaneState& previous_plane : previous_plane_state_) {
    previous_size++;
    // Mark surfaces of all planes to be released once they are
    // offline.
    if (removed_layers) {
      if (previous_plane.NeedsOffScreenComposition()) {
        display_plane_manager_->MarkSurfacesForRecycling(
            &previous_plane, surfaces_not_inuse_, true);
      }

      previous_plane.GetDisplayPlane()->SetInUse(false);
      continue;
    }

    composition->emplace_back();
    DisplayPlaneState& last_plane = composition->back();
    last_plane.CopyState(previous_plane);
    if (reset_plane) {
      const std::vector<size_t>& source_layers = last_plane.GetSourceLayers();
      size_t source_layers_size = source_layers.size();
      size_t index = source_layers.at(source_layers_size - 1);
      size_t threshold = static_cast<size_t>(remove_index);
      bool needs_plane_validation = false;

      if (index >= threshold) {
        removed_layers = true;
#ifdef SURFACE_TRACING
        size_t original_size = source_layers_size;
#endif
        bool has_one_layer = source_layers_size == 1 ? true : false;
        if (!has_one_layer) {
          last_plane.ResetLayers(layers, threshold, &needs_plane_validation);
        }

        source_layers_size = source_layers.size();
#ifdef SURFACE_TRACING
        ISURFACETRACE(
            "Layers removed. Total old Layers: %d Total new Layers: %d "
            "Threshold: "
            "%d Plane Layer Index: %d Total Planes: %d previous_plane_state_ "
            "%d \n",
            original_size, source_layers_size, threshold, index,
            composition->size(), previous_plane_state_.size());
#endif
        // We need to force re-validation of commit to ensure we update any
        // Scalar usage with the new combination of layers.
        ignore_commit = false;

        if (last_plane.GetSourceLayers().empty() || has_one_layer) {
          display_plane_manager_->MarkSurfacesForRecycling(
              &last_plane, surfaces_not_inuse_, true);

          // On some platforms disabling primary disables
          // the whole pipe. Let's revalidate the new layers
          // and ensure primary has a buffer.
          if (last_plane.GetDisplayPlane() ==
              previous_plane_state_.begin()->GetDisplayPlane()) {
#ifdef SURFACE_TRACING
            ISURFACETRACE("Primary plane is empty forcing full validation. \n");
#endif
            *force_full_validation = true;
            *can_ignore_commit = false;
            return;
          }

          last_plane.GetDisplayPlane()->SetInUse(false);
          composition->pop_back();
          pop_plane = ForcePlaneValidation(*add_index, remove_index,
                                           layers.size(), composition->size());
#ifdef SURFACE_TRACING
          ISURFACETRACE(
              "Plane removed. Total old Layers: %d Total new Layers: %d "
              "Threshold: "
              "%d Plane Layer Index: %d Total Planes: %d previous_plane_state_ "
              "%d \n",
              original_size, source_layers_size, threshold, index,
              composition->size(), previous_plane_state_.size());
#endif
          continue;
        }

        std::vector<OverlayPlane> temp;
        while (display_plane_manager_->SquashPlanesAsNeeded(
            layers, *composition, temp, surfaces_not_inuse_,
            &plane_validation)) {
          continue;
        }
        DisplayPlaneState& squashed_plane = composition->back();

        squashed_plane.ValidateReValidation();

        if (squashed_plane.RevalidationType() &
            DisplayPlaneState::ReValidationType::kScanout) {
          const std::vector<size_t>& source_layers =
              squashed_plane.GetSourceLayers();
          const OverlayLayer* layer = &(layers.at(source_layers.at(0)));
          // Check if Actual & Supported Composition differ for this
          // layer. If so than let' mark it for validation.
          if (source_layers_size == 1) {
            if (layer->CanScanOut() &&
                squashed_plane.NeedsOffScreenComposition()) {
              plane_validation = true;
            } else {
              check_to_squash = true;
              squashed_plane.RevalidationDone(
                  DisplayPlaneState::ReValidationType::kScanout);
            }
          }
        }
      }

      if (needs_plane_validation)
        plane_validation = true;

      if (removed_layers) {
        pop_plane = ForcePlaneValidation(*add_index, remove_index,
                                         layers.size(), composition->size());
        if (pop_plane)
          continue;
      }
    }

    DisplayPlaneState& target_plane = composition->back();
    if (target_plane.NeedsOffScreenComposition()) {
      HwcRect<int> surface_damage = HwcRect<int>(0, 0, 0, 0);
      bool update_rect = reset_plane;
      bool refresh_surfaces = reset_composition_regions;
      bool force_partial_clear = false;

      const std::vector<size_t>& source_layers = target_plane.GetSourceLayers();
      size_t layers_size = source_layers.size();
      if (!removed_layers && !update_rect) {
        for (size_t i = 0; i < layers_size; i++) {
          const size_t& source_index = source_layers.at(i);
          const OverlayLayer& layer = layers.at(source_index);
          if (layer.HasDimensionsChanged()) {
            update_rect = true;
            break;
          }

          if (refresh_surfaces) {
            continue;
          }

          if (layer.NeedsPartialClear()) {
            force_partial_clear = true;
          }

          if (layer.HasLayerContentChanged()) {
            CalculateRect(layer.GetSurfaceDamage(), surface_damage);
          }
        }
      }

      if (layers_size == 1) {
        // If this is a video plane, ensure that buffer format
        // is still related to Video/Camera. Handle buffer format
        // changes in this case.
        if (target_plane.IsVideoPlane()) {
          const OverlayLayer& layer = layers.at(source_layers.at(0));
          if (!layer.IsVideoLayer()) {
            target_plane.SetVideoPlane(false);
            display_plane_manager_->MarkSurfacesForRecycling(
                &target_plane, surfaces_not_inuse_, true);
            refresh_surfaces = true;
          }
        } else if (target_plane.CanSupportVideo() &&
                   !target_plane.IsVideoPlane()) {
          const OverlayLayer& layer = layers.at(source_layers.at(0));
          if (layer.IsVideoLayer()) {
            target_plane.SetVideoPlane(true);
            display_plane_manager_->MarkSurfacesForRecycling(
                &target_plane, surfaces_not_inuse_, true);
            refresh_surfaces = true;
          }
        }
      }

      if (!removed_layers && update_rect) {
        target_plane.RefreshLayerRects(layers);
        surface_damage.reset();
      }

      // Let's check if we need to check this plane-layer combination.
      if (update_rect) {
        target_plane.ValidateReValidation();
        if (target_plane.RevalidationType() !=
            DisplayPlaneState::ReValidationType::kNone) {
          plane_validation = true;
        }
      }

      if (update_rect || refresh_surfaces || !surface_damage.empty() ||
          force_partial_clear) {
        needs_gpu_composition = true;
        if (target_plane.NeedsSurfaceAllocation()) {
          display_plane_manager_->SetOffScreenPlaneTarget(target_plane);
        } else if (refresh_surfaces || reset_plane) {
          target_plane.RefreshSurfaces(NativeSurface::kFullClear, true);
        } else if (!update_rect && !surface_damage.empty()) {
          if (force_partial_clear) {
            target_plane.RefreshSurfaces(NativeSurface::kPartialClear, true);
          }

          target_plane.UpdateDamage(surface_damage);
        }

        if (refresh_surfaces || reset_plane || update_rect) {
          std::vector<OverlayPlane> temp;
          bool squashed = display_plane_manager_->SquashPlanesAsNeeded(
              layers, *composition, temp, surfaces_not_inuse_,
              &plane_validation);
          if (squashed) {
            // We squashed planes and it's not the last one.
            // We might have messed up with plane order, let's
            // force full validation.
            if (previous_size != previous_plane_state_.size()) {
              *force_full_validation = true;
              *can_ignore_commit = false;
              return;
            } else {
              while (display_plane_manager_->SquashPlanesAsNeeded(
                  layers, *composition, temp, surfaces_not_inuse_,
                  &plane_validation)) {
                continue;
              }
            }
          }
        }
      } else {
        target_plane.UpdateDamage(surface_damage);
      }

      DisplayPlaneState& squashed_plane = composition->back();
      squashed_plane.RefreshSurfaces(NativeSurface::kFullClear);

      if (!needs_gpu_composition)
        needs_gpu_composition = !(squashed_plane.IsSurfaceRecycled());

      reset_composition_regions = false;
    } else {
      reset_composition_regions = false;
      const OverlayLayer* layer =
          &(layers.at(last_plane.GetSourceLayers().front()));

      OverlayBuffer* buffer = layer->GetBuffer();
      if (buffer->GetFb() == 0) {
        buffer->CreateFrameBuffer();

        // FB creation failed, we need to re-validate the
        // whole commit.
        if (buffer->GetFb() == 0) {
          *force_full_validation = true;
          *can_ignore_commit = false;
          return;
        }

        reset_composition_regions = true;
      }

      last_plane.SetOverlayLayer(layer);
      if (layer->HasLayerContentChanged()) {
        ignore_commit = false;
      }

      bool needs_revalidation = layer->NeedsRevalidation();
      if (layer->HasDimensionsChanged() || needs_revalidation) {
        ignore_commit = false;
        reset_composition_regions = true;
      }

      if (needs_revalidation) {
        plane_validation = true;
      }
    }
  }

  *render_layers = needs_gpu_composition;
  if (needs_gpu_composition)
    ignore_commit = false;

  *can_ignore_commit = ignore_commit;
  *needs_plane_validation = plane_validation;
  size_t total_planes = composition->size();
  // Check if we can squash the last overlay (Before Cursor Plane).
  // If we have freed planes, let's see if we can actually map
  // layers to planes for direct scanout.
  if (check_to_squash && !pop_plane) {
    if (composition->back().IsCursorPlane()) {
      // We cannot squash Cursor plane.
      total_planes -= 1;
    }

    if (total_planes > 2) {
      DisplayPlaneState& old_plane = composition->at(total_planes - 2);
      DisplayPlaneState& last_overlay = composition->at(total_planes - 1);
      const std::vector<size_t>& source_layers = last_overlay.GetSourceLayers();

      if (old_plane.CanSquash() && last_overlay.CanSquash() &&
          source_layers.size() == 1) {
#ifdef SURFACE_TRACING
        ISURFACETRACE(
            "Moving layer index %d from plane index: %d to plane idex: %d. \n",
            source_layers.at(0), total_planes - 1, total_planes - 2);
#endif
        const OverlayLayer* layer = &(layers.at(source_layers.at(0)));
        if (display_plane_manager_->ForceSeparatePlane(layers, old_plane,
                                                       layer)) {
          return;
        }

        old_plane.AddLayer(layer);
        old_plane.ResetCompositionRegion();

        // If overlay has offscreen surfaces, discard
        // them.
        if (last_overlay.GetOffScreenTarget()) {
          display_plane_manager_->MarkSurfacesForRecycling(
              &last_overlay, surfaces_not_inuse_, true);
        }

        // Let's allocate an offscreen surface if needed.
        display_plane_manager_->SetOffScreenPlaneTarget(old_plane);

        last_overlay.GetDisplayPlane()->SetInUse(false);
        composition->erase(composition->begin() + (total_planes - 1));
        *render_layers = true;
        *can_ignore_commit = false;
      }
    }
  } else if (pop_plane) {
    DisplayPlaneState& last_plane = composition->back();
    // Ignore in case we are already scanning out the layer.
    if (!last_plane.NeedsOffScreenComposition()) {
      return;
    }

    const std::vector<size_t>& source_layers = last_plane.GetSourceLayers();
    // Let's try to allocate more planes in case we have free planes and
    // layers to map.
    *add_index = source_layers.at(0);
    display_plane_manager_->MarkSurfacesForRecycling(&last_plane,
                                                     surfaces_not_inuse_, true);
    composition->erase(composition->begin() + (total_planes - 1));
  }
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence, bool* ignore_clone_update,
                               PixelUploaderCallback* call_back,
                               bool handle_constraints) {
  CTRACE();
  ScopedIdleStateTracker tracker(idle_tracker_, compositor_,
                                 resource_manager_.get(), this);
  if (tracker.IgnoreUpdate()) {
    return true;
  }

  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  std::vector<OverlayLayer> layers;
  int remove_index = -1;
  int add_index = -1;
  // If last commit failed, lets force full validation as
  // state might be all wrong in our side.
  bool idle_frame = tracker.RenderIdleMode();
  bool validate_layers =
      last_commit_failed_update_ || previous_plane_state_.empty();
  *retire_fence = -1;
  uint32_t z_order = 0;
  bool has_video_layer = false;
  bool re_validate_commit = false;
  needs_clone_validation_ = false;

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
          handle_constraints, fb_manager_);
    } else {
      overlay_layer->InitializeFromHwcLayer(
          layer, resource_manager_.get(), previous_layer, z_order, layer_index,
          display_plane_manager_->GetHeight(), plane_transform_,
          handle_constraints, fb_manager_);
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
    } else if (overlay_layer->HasLayerContentChanged()) {
      idle_frame = false;
    }

    if (overlay_layer->IsCursorLayer()) {
      tracker.FrameHasCursor();
    }

    z_order++;

    // Handle case where Media layer has been destroyed/created or has changed
    // z-order.
    if (add_index != 0 && overlay_layer->IsVideoLayer()) {
      if ((previous_layer && !previous_layer->IsVideoLayer()) ||
          !previous_layer) {
        add_index = 0;
#ifdef SURFACE_TRACING
        ISURFACETRACE(
            "Video layer has changed between frames: remove_index: %d "
            "add_index: "
            "%d \n",
            remove_index, add_index);
#endif
        continue;
      }
    }

    if (add_index == 0 || validate_layers ||
        ((add_index != -1) && (remove_index != -1))) {
      needs_clone_validation_ = true;
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

  if (idle_frame) {
    if ((add_index != -1) || (remove_index != -1) || re_validate_commit) {
      idle_frame = false;
    }
  }

  if (!validate_layers)
    validate_layers = idle_frame;
#ifdef SURFACE_TRACING
  if ((remove_index != -1) || (add_index != -1)) {
    ISURFACETRACE(
        "Remove index For this Frame: %d Add index For this Frame: %d Total "
        "Layers: %d previous_size %d size %d re_validate_commit %d \n",
        remove_index, add_index, layers.size(), previous_size, size,
        re_validate_commit);
  }

  if (validate_layers) {
    ISURFACETRACE(
        "Full Validation Forced: add_index: %d last_commit_failed_update_: %d "
        "tracker.RevalidateLayers(): %d  previous_plane_state_.empty(): %d "
        "tracker.RenderIdleMode():%d \n",
        add_index, last_commit_failed_update_, tracker.RevalidateLayers(),
        previous_plane_state_.empty(), tracker.RenderIdleMode());
  }
#endif

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  bool force_media_composition = false;
  bool requested_video_effect = false;
  if (has_video_layer) {
    video_lock_.lock();
    if (requested_video_effect_ != applied_video_effect_) {
      // Let's ensure Media planes take this into account.
      force_media_composition = true;
      applied_video_effect_ = requested_video_effect_;
      requested_video_effect = requested_video_effect_;
      idle_frame = false;
      validate_layers = true;
    }
    video_lock_.unlock();
  }

  bool composition_passed = true;
  bool disable_ovelays = state_ & kDisableOverlayUsage;
  if (!validate_layers && tracker.RevalidateLayers()) {
    validate_layers = true;
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
                    &validate_layers, &add_index);
    if (add_index == 0) {
      validate_layers = true;
    }

    if (!validate_layers && add_index > 0) {
      bool render_cursor = display_plane_manager_->ValidateLayers(
          layers, add_index, disable_ovelays, &commit_checked,
          &needs_plane_validation, current_composition_planes,
          previous_plane_state_, surfaces_not_inuse_);

      if (!render_layers)
        render_layers = render_cursor;
      can_ignore_commit = false;
      if (commit_checked) {
        re_validate_commit = false;
        needs_clone_validation_ = true;
      }
    }

    if (!validate_layers && (re_validate_commit || needs_plane_validation)) {
      bool render = display_plane_manager_->ReValidatePlanes(
          current_composition_planes, layers, surfaces_not_inuse_,
          &validate_layers, needs_plane_validation, re_validate_commit);
      can_ignore_commit = false;
      needs_clone_validation_ = true;
      if (!render_layers)
        render_layers = render;
    }

    if (!validate_layers) {
      if (force_media_composition) {
        SetMediaEffectsState(requested_video_effect, layers,
                             current_composition_planes);
        render_layers = true;
        can_ignore_commit = false;
      }

      if (can_ignore_commit) {
        *ignore_clone_update = true;
        // Free any surfaces.
        if (!mark_not_inuse_.empty()) {
          size_t size = mark_not_inuse_.size();
          for (uint32_t i = 0; i < size; i++) {
            mark_not_inuse_.at(i)->SetSurfaceAge(-1);
          }

          std::vector<NativeSurface*>().swap(mark_not_inuse_);
          tracker.ForceSurfaceRelease();
        }

        return true;
      }
    }
  }

  // Reset last commit failure state.
  last_commit_failed_update_ = false;

  if (validate_layers) {
    if (!idle_frame)
      tracker.ResetTrackerState();

    needs_clone_validation_ = true;

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

  // Ensure all pixel buffer uploads are done.
  if (call_back) {
    call_back->Synchronize();
  }
  // Handle any 3D Composition.
  if (render_layers) {
    compositor_.BeginFrame(disable_ovelays);

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

  if (!composition_passed) {
    HandleCommitFailure(current_composition_planes);
    last_commit_failed_update_ = true;
    return false;
  }

  if (state_ & kNeedsColorCorrection) {
    display_->SetColorCorrection(gamma_, contrast_, brightness_);
    display_->SetColorTransformMatrix(color_transform_matrix_,
                                      color_transform_hint_);
    state_ &= ~kNeedsColorCorrection;
  }

  display_->SetPipeCanvasColor(canvas_.bpc, canvas_.red, canvas_.green,
                               canvas_.blue, canvas_.alpha);

  int32_t fence = 0;
  bool fence_released = false;
  composition_passed =
      display_->Commit(current_composition_planes, previous_plane_state_,
                       disable_ovelays, kms_fence_, &fence, &fence_released);

  if (fence_released) {
    kms_fence_ = 0;
  }

  if (!composition_passed) {
    last_commit_failed_update_ = true;
    HandleCommitFailure(current_composition_planes);
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
      mark_not_inuse_.at(i)->SetSurfaceAge(-1);
    }

    std::vector<NativeSurface*>().swap(mark_not_inuse_);
    tracker.ForceSurfaceRelease();
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
      } else {
        mark_not_inuse_.emplace_back(surface);
      }
    }

    surfaces_not_inuse_.swap(temp);
  }

  if (fence > 0) {
    *retire_fence = dup(fence);
    kms_fence_ = fence;

    SetReleaseFenceToLayers(fence, source_layers);
  }

  // Let Display handle any lazy initalizations.
  if (handle_display_initializations_) {
    handle_display_initializations_ = false;
    display_->HandleLazyInitialization();
  }

  return true;
}

void DisplayQueue::PresentClonedCommit(DisplayQueue* queue) {
  ScopedCloneStateTracker tracker(compositor_, resource_manager_.get(), this);
  const DisplayPlaneStateList& source_planes =
      queue->GetCurrentCompositionPlanes();
  if (source_planes.empty()) {
    // Mark any surfaces as not in use. These surfaces
    // where not marked earlier as they where onscreen.
    // Doing it here also ensures that if this surface
    // is still in use than it will be marked in use
    // below.
    if (!mark_not_inuse_.empty()) {
      size_t size = mark_not_inuse_.size();
      for (uint32_t i = 0; i < size; i++) {
        mark_not_inuse_.at(i)->SetSurfaceAge(-1);
      }

      std::vector<NativeSurface*>().swap(mark_not_inuse_);
      tracker.ForceSurfaceRelease();
    }

    return;
  }

  std::vector<OverlayLayer> layers;
  int add_index = -1;
  for (const DisplayPlaneState& previous_plane : source_planes) {
    layers.emplace_back();
    OverlayLayer& layer = layers.back();
    HwcRect<int> display_frame =
        previous_plane.GetOverlayLayer()->GetDisplayFrame();
    if (scaling_tracker_.scaling_state_ == ScalingTracker::kNeedsScaling) {
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
    }

    layer.CloneLayer(previous_plane.GetOverlayLayer(), display_frame,
                     resource_manager_.get(), layers.size() - 1, fb_manager_);
  }

  bool test_commit = false;
  bool render_layers = false;
  bool validate_layers = last_commit_failed_update_ ||
                         queue->needs_clone_validation_ ||
                         previous_plane_state_.empty();
  if (previous_plane_state_.size() != source_planes.size())
    validate_layers = true;

  DisplayPlaneStateList current_composition_planes;
  // Validate Overlays and Layers usage.
  if (!validate_layers) {
    bool can_ignore_commit = false;
    // Before forcing layer validation, check if content has changed
    // if not continue showing the current buffer.
    bool commit_checked = false;
    bool needs_plane_validation = false;
    GetCachedLayers(layers, -1, &current_composition_planes, &render_layers,
                    &can_ignore_commit, &needs_plane_validation,
                    &validate_layers, &add_index);
    if (add_index == 0) {
      validate_layers = true;
    }

    if (!validate_layers && add_index > 0) {
      bool render_cursor = display_plane_manager_->ValidateLayers(
          layers, add_index, false, &commit_checked, &needs_plane_validation,
          current_composition_planes, previous_plane_state_,
          surfaces_not_inuse_);

      if (!render_layers)
        render_layers = render_cursor;
      can_ignore_commit = false;
    }

    if (!validate_layers && needs_plane_validation) {
      bool render = display_plane_manager_->ReValidatePlanes(
          current_composition_planes, layers, surfaces_not_inuse_,
          &validate_layers, needs_plane_validation, false);
      can_ignore_commit = false;
      if (!render_layers)
        render_layers = render;
    }

    if (!validate_layers) {
      if (can_ignore_commit) {
        // Free any surfaces.
        if (!mark_not_inuse_.empty()) {
          size_t size = mark_not_inuse_.size();
          for (uint32_t i = 0; i < size; i++) {
            mark_not_inuse_.at(i)->SetSurfaceAge(-1);
          }

          std::vector<NativeSurface*>().swap(mark_not_inuse_);
          tracker.ForceSurfaceRelease();
        }

        return;
      }
    }
  }

  // Reset last commit failure state.
  last_commit_failed_update_ = false;

  if (validate_layers) {
    render_layers = display_plane_manager_->ValidateLayers(
        layers, 0, false, &test_commit, &test_commit,
        current_composition_planes, previous_plane_state_, surfaces_not_inuse_);
  }

  DUMP_CURRENT_COMPOSITION_PLANES();
  DUMP_CURRENT_LAYER_PLANE_COMBINATIONS();
  DUMP_CURRENT_DUPLICATE_LAYER_COMBINATIONS();

  bool composition_passed = true;
  clone_rendered_ = false;
  // Handle any 3D Composition.
  if (render_layers) {
    clone_rendered_ = true;
    compositor_.BeginFrame(false);

    std::vector<HwcRect<int>> layers_rects;
    size_t size = layers.size();
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

  if (!composition_passed) {
    HandleCommitFailure(current_composition_planes);
    last_commit_failed_update_ = true;
    return;
  }

  int32_t fence = 0;
  bool fence_released = false;
  composition_passed =
      display_->Commit(current_composition_planes, previous_plane_state_, false,
                       kms_fence_, &fence, &fence_released);

  if (fence_released) {
    kms_fence_ = 0;
  }

  if (!composition_passed) {
    last_commit_failed_update_ = true;
    HandleCommitFailure(current_composition_planes);
    return;
  }

  // Mark any surfaces as not in use. These surfaces
  // where not marked earlier as they where onscreen.
  // Doing it here also ensures that if this surface
  // is still in use than it will be marked in use
  // below.
  if (!mark_not_inuse_.empty()) {
    size_t size = mark_not_inuse_.size();
    for (uint32_t i = 0; i < size; i++) {
      mark_not_inuse_.at(i)->SetSurfaceAge(-1);
    }

    std::vector<NativeSurface*>().swap(mark_not_inuse_);
  }

  in_flight_layers_.swap(layers);
  current_composition_planes.swap(previous_plane_state_);

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
      } else {
        mark_not_inuse_.emplace_back(surface);
      }
    }

    surfaces_not_inuse_.swap(temp);
  }

  if (fence > 0) {
    kms_fence_ = fence;
  }
}

void DisplayQueue::NotifyDisplayWA(bool enable_wa) {
  if (enable_wa_ == enable_wa)
    return;

  enable_wa_ = enable_wa;
  if (display_plane_manager_)
    display_plane_manager_->SetLastPlaneUsage(!enable_wa_);
}

void DisplayQueue::SetCloneMode(bool cloned) {
  if (clone_mode_ == cloned)
    return;

  if (vblank_handler_) {
    if (cloned) {
      vblank_handler_->SetPowerMode(kOff);
    } else {
      vblank_handler_->SetPowerMode(kOn);
    }
  }

  if (display_plane_manager_) {
    // Set Age for all offscreen surfaces.
    UpdateOnScreenSurfaces();

    for (DisplayPlaneState& previous_plane : previous_plane_state_) {
      display_plane_manager_->MarkSurfacesForRecycling(
          &previous_plane, surfaces_not_inuse_, true);
    }

    DisplayPlaneStateList().swap(previous_plane_state_);
  }

  clone_mode_ = cloned;
  clone_rendered_ = false;
}

void DisplayQueue::IgnoreUpdates() {
  idle_tracker_.idle_frames_ = 0;
  idle_tracker_.state_ = FrameStateTracker::kIgnoreUpdates;
  idle_tracker_.revalidate_frames_counter_ = 0;
}

bool DisplayQueue::IsIgnoreUpdates() {
  return idle_tracker_.state_ & FrameStateTracker::kIgnoreUpdates;
}

void DisplayQueue::HandleCommitFailure(
    DisplayPlaneStateList& current_composition_planes) {
  for (DisplayPlaneState& plane : current_composition_planes) {
    if (plane.GetSurfaces().empty()) {
      continue;
    }

    display_plane_manager_->MarkSurfacesForRecycling(
        &plane, surfaces_not_inuse_, false, false);
  }

  // Let's mark all previous planes as in use.
  for (DisplayPlaneState& previous_plane : previous_plane_state_) {
    previous_plane.GetDisplayPlane()->SetInUse(true);
    previous_plane.HandleCommitFailure();
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
          &plane, surfaces_not_inuse_, true);
      const std::vector<size_t>& source = plane.GetSourceLayers();
      plane.SetOverlayLayer(&(layers.at(source.at(0))));
    }
  }
}

void DisplayQueue::UpdateOnScreenSurfaces() {
  for (DisplayPlaneState& plane_state : previous_plane_state_) {
    const std::vector<NativeSurface*>& surfaces = plane_state.GetSurfaces();
    if (surfaces.empty())
      continue;

    size_t size = surfaces.size();
    for (uint32_t i = 0; i < size; i++) {
      NativeSurface* surface = surfaces.at(i);
      surface->SetSurfaceAge(2 - i);
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
    if (plane.IsSurfaceRecycled())
      continue;

    const std::vector<size_t>& layers = plane.GetSourceLayers();
    size_t size = layers.size();
    int32_t release_fence = -1;
    if (plane.Scanout()) {
      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        OverlayLayer& overlay_layer =
            in_flight_layers_.at(layers.at(layer_index));
        HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
        layer->SetReleaseFence(dup(fence));
        overlay_layer.SetLayerComposition(OverlayLayer::kDisplay);
      }
    } else {
      release_fence = plane.GetOverlayLayer()->GetAcquireFence();

      for (size_t layer_index = 0; layer_index < size; layer_index++) {
        OverlayLayer& overlay_layer =
            in_flight_layers_.at(layers.at(layer_index));
        overlay_layer.SetLayerComposition(OverlayLayer::kGpu);
        HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
        if (release_fence > 0) {
          layer->SetReleaseFence(dup(release_fence));
        } else {
          int32_t temp = overlay_layer.GetAcquireFence();
          if (temp > 0) {
            layer->SetReleaseFence(dup(temp));
          }
        }
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

  state_ = kConfigurationChanged;
  if (disable_overlay) {
    state_ |= kDisableOverlayUsage;
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

void DisplayQueue::SetColorTransform(const float* matrix,
                                     HWCColorTransform hint) {
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

void DisplayQueue::SetVideoScalingMode(uint32_t mode) {
  video_lock_.lock();
  // requested_video_effect_ = true;
  compositor_.SetVideoScalingMode(mode);
  video_lock_.unlock();
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

void DisplayQueue::SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                                       HWCDeinterlaceControl mode) {
  video_lock_.lock();
  requested_video_effect_ = true;
  compositor_.SetVideoDeinterlace(flag, mode);
  video_lock_.unlock();
}

void DisplayQueue::RestoreVideoDefaultDeinterlace() {
  video_lock_.lock();
  requested_video_effect_ = false;
  compositor_.RestoreVideoDefaultDeinterlace();
  video_lock_.unlock();
}

void DisplayQueue::SetCanvasColor(uint16_t bpc, uint16_t red, uint16_t green,
                                  uint16_t blue, uint16_t alpha) {
  canvas_.bpc = bpc;
  canvas_.red = red;
  canvas_.green = green;
  canvas_.blue = blue;
  canvas_.alpha = alpha;
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

  if (idle_tracker_.total_planes_ <= 1 ||
      (idle_tracker_.state_ & FrameStateTracker::kTrackingFrames) ||
      (idle_tracker_.state_ & FrameStateTracker::kRevalidateLayers) ||
      idle_tracker_.has_cursor_layer_) {
    idle_tracker_.idle_lock_.unlock();
    return;
  }

  if (idle_tracker_.idle_frames_ > kidleframes) {
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
  if (display_plane_manager_.get() && display_plane_manager_->HasSurfaces())
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
  clone_rendered_ = false;
}

}  // namespace hwcomposer
