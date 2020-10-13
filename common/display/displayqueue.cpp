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
#include <sys/time.h>
#include <vector>

#include "displayplanemanager.h"
#include "gpudevice.h"
#include "hwctrace.h"
#include "hwcutils.h"
#include "nativesurface.h"
#include "overlaylayer.h"
#include "vblankeventhandler.h"

#include "physicaldisplay.h"
#include "renderer.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, bool disable_explictsync,
                           NativeBufferHandler* buffer_handler,
                           PhysicalDisplay* display)
    : gpu_fd_(gpu_fd), display_(display) {
  if (disable_explictsync) {
    state_ |= kDisableExplictSync;
  } else {
    state_ &= ~kDisableExplictSync;
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
  state_ |= kCanvasColorChanged;
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
      new DisplayPlaneManager(plane_handler, resource_manager_.get()));
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
      state_ |= kPoweredOn | kConfigurationChanged | kNeedsColorCorrection |
                kCanvasColorChanged;
      vblank_handler_->SetPowerMode(kOn);
      power_mode_lock_.lock();
      state_ &= ~kIgnoreIdleRefresh;
      compositor_.Init(resource_manager_.get(), gpu_fd_);
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

void DisplayQueue::ReleaseUnreservedPlanes(
    std::vector<uint32_t>& reserved_planes) {
  display_plane_manager_->ReleaseUnreservedPlanes(reserved_planes);
}

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   int& re_validate_begin,
                                   DisplayPlaneStateList& composition) {
  CTRACE();
  size_t previous_size = 0;
  int new_re_validate = 0;
  bool rects_updated = false;

  for (DisplayPlaneState& previous_plane : previous_plane_state_) {
    previous_size += previous_plane.GetSourceLayers().size();
    if ((int)previous_size > re_validate_begin) {
      // Mark surfaces of all planes to be released once they are
      // offline.
      if (previous_plane.NeedsOffScreenComposition()) {
        display_plane_manager_->MarkSurfacesForRecycling(
            &previous_plane, surfaces_not_inuse_, true);
      }
      previous_plane.GetDisplayPlane()->SetInUse(false);
    } else {
      composition.emplace_back();
      DisplayPlaneState& last_plane = composition.back();
      last_plane.CopyState(previous_plane);
      last_plane.GetDisplayPlane()->SetInUse(true);
      if (last_plane.NeedsOffScreenComposition()) {
        if (last_plane.NeedsSurfaceAllocation()) {
          last_plane.ForceGPURendering();
          last_plane.ResetLayers(layers, &rects_updated);
        } else
          last_plane.RefreshLayerRects(layers);
      } else {
        const OverlayLayer* layer =
            &(layers.at(last_plane.GetSourceLayers().front()));
        last_plane.SetOverlayLayer(layer);
        last_plane.RefreshLayerRects(layers);
      }
      // last_plane.ResetLayers(layers, &rects_updated);
      new_re_validate = previous_size;
    }
  }
  re_validate_begin = new_re_validate;
}

void DisplayQueue::InitializeOverlayLayers(
    std::vector<HwcLayer*>& source_layers, bool handle_constraints,
    std::vector<OverlayLayer>& layers, bool& has_video_layer,
    bool& has_cursor_layer, int& re_validate_begin, bool& idle_frame) {
  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  uint32_t z_order = 0;
  re_validate_begin = size;

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;

    // Discard protected video for tear down
    if (state_ & kVideoDiscardProtected) {
      if (layer->GetNativeHandle() != NULL &&
          IsBufferProtected(layer->GetNativeHandle()))
        continue;
    }

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
          display_frame, display_plane_manager_->GetHeight(),
          display_plane_manager_->GetWidth(), plane_transform_,
          handle_constraints);
    } else {
      overlay_layer->InitializeFromHwcLayer(
          layer, resource_manager_.get(), previous_layer, z_order, layer_index,
          display_plane_manager_->GetHeight(),
          display_plane_manager_->GetWidth(), plane_transform_,
          handle_constraints);
    }

    if (!overlay_layer->IsVisible()) {
      layers.pop_back();
      continue;
    }

    if (overlay_layer->IsVideoLayer()) {
      has_video_layer = true;
    }
    if (previous_layer) {
      if (overlay_layer->IsVideoLayer() != previous_layer->IsVideoLayer()) {
        re_validate_begin = 0;
      }
      if (re_validate_begin == size) {
        bool need_revalidate =
            overlay_layer->IsSolidColor() != previous_layer->IsSolidColor();
        if (!need_revalidate) {
          if (!(overlay_layer->IsCursorLayer() &&
                previous_layer->IsCursorLayer())) {
            if (IsLayerAlphaBlendingCommitted(overlay_layer) ^
                IsLayerAlphaBlendingCommitted(previous_layer))
              need_revalidate = true;
            if (!need_revalidate) {
              if (overlay_layer->NeedsRevalidation())
                need_revalidate = true;
            }
          }
        }
        if (need_revalidate)
          re_validate_begin = layer_index;
      }
    } else if (overlay_layer->IsVideoLayer()) {
      re_validate_begin = 0;
    } else if (re_validate_begin == size) {
      re_validate_begin = layer_index;
    }

    if (overlay_layer->HasLayerContentChanged()) {
      idle_frame = false;
    }

    if (overlay_layer->IsCursorLayer()) {
      has_cursor_layer = true;
    }

    z_order++;
  }
}

void DisplayQueue::DumpCurrentDisplayPlaneList(
    DisplayPlaneStateList& composition) {
  ETRACE("Dumping DisplayPlaneState size %d", composition.size());
  for (auto& state : composition) {
    state.Dump();
  }
  ETRACE("End DisplayPlaneState Dump");
}

bool DisplayQueue::AssignAndCommitPlanes(
    std::vector<OverlayLayer>& layers, std::vector<HwcLayer*>* source_layers,
    bool validate_layers, int re_validate_begin, bool setMediaEffect,
    int32_t* retire_fence, ScopedStateTracker* tracker) {
  DisplayPlaneStateList current_composition_planes;
  bool render_layers = false;
  bool composition_passed = true;
  bool disable_overlays = state_ & kDisableOverlay;
  bool disable_explictsync = state_ & kDisableExplictSync;

  GetCachedLayers(layers, re_validate_begin, current_composition_planes);
  // We need to verify the rest layers and planes
  if (re_validate_begin < (int)layers.size()) {
    validate_layers = true;
  }

  if (validate_layers) {
    display_plane_manager_->ValidateLayers(
        layers, re_validate_begin, disable_overlays, current_composition_planes,
        previous_plane_state_, surfaces_not_inuse_);

    if (setMediaEffect) {
      SetMediaEffectsState(requested_video_effect_, layers,
                           current_composition_planes);
    }
    needs_clone_validation_ = true;
  }

  for (auto& composition : current_composition_planes) {
    if (composition.NeedsOffScreenComposition()) {
      render_layers = true;
      break;
    }
  }

  // Reset last commit failure state.
  last_commit_failed_update_ = false;

  DUMP_CURRENT_COMPOSITION_PLANES();
  DUMP_CURRENT_LAYER_PLANE_COMBINATIONS();
  DUMP_CURRENT_DUPLICATE_LAYER_COMBINATIONS();

  // Ensure all pixel buffer uploads are done.
  bool compsition_passed = false;

  // Handle any 3D Composition.
  if (render_layers) {
    compositor_.BeginFrame(disable_explictsync);
    // Prepare for final composition.
    if (!compositor_.Draw(current_composition_planes, layers)) {
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

  if (state_ & kCanvasColorChanged) {
    display_->SetPipeCanvasColor(canvas_.bpc, canvas_.red, canvas_.green,
                                 canvas_.blue, canvas_.alpha);
    state_ &= ~kCanvasColorChanged;
  }

  int32_t fence = 0;
  bool fence_released = false;
  if (!IsIgnoreUpdates()) {
    composition_passed = display_->Commit(
        current_composition_planes, previous_plane_state_, disable_explictsync,
        kms_fence_, &fence, &fence_released);
  }

  if (fence_released) {
    kms_fence_ = 0;
  }

  if (!composition_passed) {
    DumpCurrentDisplayPlaneList(current_composition_planes);
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
    if (tracker)
      tracker->ForceSurfaceRelease();
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
    if (retire_fence)
      *retire_fence = dup(fence);
    kms_fence_ = fence;
    if (source_layers)
      SetReleaseFenceToLayers(fence, *source_layers);
  }

  // Let Display handle any lazy initalizations.
  if (handle_display_initializations_) {
    handle_display_initializations_ = false;
    display_->HandleLazyInitialization();
  }

  return true;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence, bool* ignore_clone_update,
                               PixelUploaderCallback* call_back,
                               bool handle_constraints) {
  CTRACE();
  if (!GpuDevice::getInstance().IsDrmMaster()) {
    ITRACE("DRM master is not set, ignore queue update.");
    return true;
  }

  ScopedIdleStateTracker tracker(idle_tracker_, compositor_,
                                 resource_manager_.get(), this);
  if (tracker.IgnoreUpdate()) {
    return true;
  }
  source_layers_ = &source_layers;
  std::vector<OverlayLayer> layers;
  int re_validate_begin = -1;
  bool idle_frame = true;
  // If last commit failed, lets force full validation as
  // state might be all wrong in our side.
  bool validate_layers =
      last_commit_failed_update_ || previous_plane_state_.empty();
  *retire_fence = -1;

  bool has_video_layer = false;
  bool has_cursor_layer = false;
  needs_clone_validation_ = false;

  InitializeOverlayLayers(source_layers, handle_constraints, layers,
                          has_video_layer, has_cursor_layer, re_validate_begin,
                          idle_frame);

  if (validate_layers || re_validate_begin != source_layers.size()) {
    needs_clone_validation_ = true;
  }

  if (has_cursor_layer)
    tracker.FrameHasCursor();

  // We are going to force GPU and validate all
  if (re_validate_begin == 0)
    validate_layers = true;
  if (validate_layers)
    re_validate_begin = 0;

  bool force_media_composition = false;
  bool requested_video_effect = false;

  if (has_video_layer) {
    video_lock_.lock();
    if (video_effect_changed_) {
      idle_frame = false;
      video_effect_changed_ = false;
      validate_layers = true;
    }
    if (requested_video_effect_) {
      // Let's ensure Media planes take this into account.
      force_media_composition = true;
      requested_video_effect = requested_video_effect_;
      idle_frame = false;
    }
    video_lock_.unlock();
  }

  if (!validate_layers && tracker.RevalidateLayers()) {
    validate_layers = true;
  }

  // Validate Overlays and Layers usage.
  bool can_ignore_commit = idle_frame && !validate_layers &&
                           source_layers_->size() == in_flight_layers_.size();

  if (can_ignore_commit) {
    *ignore_clone_update = true;
    if (!mark_not_inuse_.empty()) {
      size_t size = mark_not_inuse_.size();
      for (size_t i = 0; i < size; i++) {
        mark_not_inuse_[i]->SetSurfaceAge(-1);
      }
      std::vector<NativeSurface*>().swap(mark_not_inuse_);
      tracker.ForceSurfaceRelease();
    }
    return true;
  }

  if (call_back) {
    call_back->Synchronize();
  }

  return AssignAndCommitPlanes(
      layers, &source_layers, validate_layers, re_validate_begin,
      force_media_composition && requested_video_effect, retire_fence,
      &tracker);
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
  size_t layers_size = layers.size();
  int add_index = layers_size;
  size_t z_order = 0;
  size_t previous_size = in_flight_layers_.size();
  for (const DisplayPlaneState& previous_plane : source_planes) {
    layers.emplace_back();

    OverlayLayer& layer = layers.back();
    OverlayLayer* previous_layer = NULL;

    if (previous_size > z_order) {
      previous_layer = &(in_flight_layers_.at(z_order));
    } else if (add_index == -1) {
      add_index = z_order;
    }
    z_order++;

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
                     resource_manager_.get(), layers.size() - 1);
#ifndef FORCE_ALL_DEVICE_TYPE
    // the blending of Client layer is None. need to set as Premult for
    // composing
    if (display_->GetTotalOverlays() < z_order)
      layer.SetBlending(hwcomposer::HWCBlending::kBlendingPremult);
#endif

    // force full validate when video layer changes
    if (add_index != 0 && layer.IsVideoLayer()) {
      if ((previous_layer && !previous_layer->IsVideoLayer()) ||
          !previous_layer) {
        add_index = 0;
        continue;
      }
    }
    if (previous_layer &&
        previous_layer->IsCursorLayer() != layer.IsCursorLayer()) {
      if (add_index == layers_size)
        add_index = layer.GetZorder();
    }
  }

  bool validate_layers = last_commit_failed_update_ ||
                         queue->needs_clone_validation_ ||
                         previous_plane_state_.empty() || (add_index == 0);
  if (previous_plane_state_.size() != source_planes.size())
    validate_layers = true;

  AssignAndCommitPlanes(layers, queue->GetSourceLayers(), validate_layers,
                        add_index, false, NULL, &tracker);
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

void DisplayQueue::ResetPlanes(drmModeAtomicReqPtr pset) {
  display_plane_manager_->ResetPlanes(pset);
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
    if (!apply_effects && !surfaces.empty() && plane.Scanout()) {
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
          } else {
            // [WA] set commit fence for video buffer as release fence
            if (layer->IsVideoLayer()) {
              layer->SetReleaseFence(dup(fence));
            }
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

  bool disable_explictsync = false;
  if (state_ & kDisableExplictSync) {
    disable_explictsync = true;
  }

  state_ = kConfigurationChanged;
  if (disable_explictsync) {
    state_ |= kDisableExplictSync;
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

void DisplayQueue::SetDisableExplicitSync(bool disable_explicit_sync) {
  if (disable_explicit_sync) {
    state_ |= kDisableExplictSync;
  } else {
    state_ &= ~kDisableExplictSync;
  }
}

void DisplayQueue::SetVideoScalingMode(uint32_t mode) {
  video_lock_.lock();
  requested_video_effect_ = true;
  video_effect_changed_ = true;
  compositor_.SetVideoScalingMode(mode);
  video_lock_.unlock();
}

void DisplayQueue::SetVideoColor(HWCColorControl color, float value) {
  video_lock_.lock();
  requested_video_effect_ = true;
  video_effect_changed_ = true;
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
  video_effect_changed_ = true;
  compositor_.RestoreVideoDefaultColor(color);
  video_lock_.unlock();
}

void DisplayQueue::SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                                       HWCDeinterlaceControl mode) {
  video_lock_.lock();
  requested_video_effect_ = true;
  video_effect_changed_ = true;
  compositor_.SetVideoDeinterlace(flag, mode);
  video_lock_.unlock();
}

void DisplayQueue::RestoreVideoDefaultDeinterlace() {
  video_lock_.lock();
  requested_video_effect_ = false;
  video_effect_changed_ = true;
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
  state_ |= kCanvasColorChanged;
}

int DisplayQueue::RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                        uint32_t display_id) {
  return vblank_handler_->RegisterCallback(callback, display_id);
}

int DisplayQueue::RegisterVsyncPeriodCallback(
    std::shared_ptr<VsyncPeriodCallback> callback, uint32_t display_id) {
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
  if (idle_tracker_.state_ & FrameStateTracker::kForceIgnoreUpdates)
    return;
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

void DisplayQueue::ForceIgnoreUpdates(bool force) {
  if (force) {
    idle_tracker_.state_ |= FrameStateTracker::kForceIgnoreUpdates;
    IgnoreUpdates();
  } else {
    idle_tracker_.state_ &= ~FrameStateTracker::kForceIgnoreUpdates;
    ForceRefresh();
  }
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
