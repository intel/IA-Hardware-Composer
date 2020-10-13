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

#ifndef COMMON_DISPLAY_DISPLAYQUEUE_H_
#define COMMON_DISPLAY_DISPLAYQUEUE_H_

#include <spinlock.h>

#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <queue>
#include <vector>

#include "compositor.h"
#include "displayplanemanager.h"
#include "hwcthread.h"
#include "platformdefines.h"
#include "resourcemanager.h"
#include "vblankeventhandler.h"

namespace hwcomposer {
struct gamma_colors {
  float red;
  float green;
  float blue;
};

struct canvas_color_comps {
  uint16_t bpc;
  uint16_t red;
  uint16_t green;
  uint16_t blue;
  uint16_t alpha;
};

class FrameBufferManager;
class PhysicalDisplay;
class DisplayPlaneHandler;
struct HwcLayer;
class NativeBufferHandler;

static uint32_t kidleframes = 250;
class DisplayQueue {
 public:
  DisplayQueue(uint32_t gpu_fd, bool disable_explictsync,
               NativeBufferHandler* buffer_handler, PhysicalDisplay* display);
  ~DisplayQueue();

  bool Initialize(uint32_t pipe, uint32_t width, uint32_t height,
                  DisplayPlaneHandler* plane_manager);

  bool QueueUpdate(std::vector<HwcLayer*>& source_layers, int32_t* retire_fence,
                   bool* ignore_clone_update, PixelUploaderCallback* call_back,
                   bool handle_constraints);
  bool SetPowerMode(uint32_t power_mode);
  bool CheckPlaneFormat(uint32_t format);
  void SetGamma(float red, float green, float blue);
  void SetColorTransform(const float* matrix, HWCColorTransform hint);
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue);
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue);
  void SetDisableExplicitSync(bool disable_explicit_sync);
  void SetVideoScalingMode(uint32_t mode);
  void SetVideoColor(HWCColorControl color, float value);
  void GetVideoColor(HWCColorControl color, float* value, float* start,
                     float* end);
  void SetCanvasColor(uint16_t bpc, uint16_t red, uint16_t green, uint16_t blue,
                      uint16_t alpha);
  void RestoreVideoDefaultColor(HWCColorControl color);
  void SetVideoDeinterlace(HWCDeinterlaceFlag flag, HWCDeinterlaceControl mode);
  void RestoreVideoDefaultDeinterlace();
  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id);
  int RegisterVsyncPeriodCallback(std::shared_ptr<VsyncPeriodCallback> callback,
                                  uint32_t display_id);
  void RegisterRefreshCallback(std::shared_ptr<RefreshCallback> callback,
                               uint32_t display_id);

  void VSyncControl(bool enabled);

  void HandleIdleCase();

  void DisplayConfigurationChanged();

  bool IsIgnoreUpdates();

  void ForceRefresh();

  void ForceIgnoreUpdates(bool force);

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width, uint32_t display_height);

  void SetCloneMode(bool cloned);

  void RotateDisplay(HWCRotation rotation);

  void IgnoreUpdates();

  void ResetPlanes(drmModeAtomicReqPtr pset);

  void PresentClonedCommit(DisplayQueue* queue);

  const DisplayPlaneStateList& GetCurrentCompositionPlanes() const {
    return previous_plane_state_;
  }

  bool NeedsCloneValidation() const {
    return needs_clone_validation_;
  }

  const NativeBufferHandler* GetNativeBufferHandler() const {
    if (resource_manager_) {
      return resource_manager_->GetNativeBufferHandler();
    }

    return NULL;
  }

  std::vector<HwcLayer*>* GetSourceLayers() {
    return source_layers_;
  }

  void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                            uint32_t pavp_instance_id) {
    if (enabled) {
      state_ &= ~kVideoDiscardProtected;
    } else {
      state_ |= kVideoDiscardProtected;
    }
  }

  int GetTotalOverlays() const {
    if (display_plane_manager_)
      return display_plane_manager_->GetTotalOverlays();
    else
      return 0;
  }

  void ReleaseUnreservedPlanes(std::vector<uint32_t>& reserved_planes);
  void DumpCurrentDisplayPlaneList(DisplayPlaneStateList& composition);

 private:
  enum QueueState {
    kNeedsColorCorrection = 1 << 0,  // Needs Color correction.
    kConfigurationChanged = 1 << 1,  // Layers need to be re-validated.
    kPoweredOn = 1 << 2,
    kDisableExplictSync = 1 << 3,  // Disable explict sync.
    kIgnoreIdleRefresh =
        1 << 4,  // Ignore refresh request during idle callback.
    kCanvasColorChanged = 1 << 5,  // Update color change
    kVideoDiscardProtected =
        1 << 6,  // Need to discard protected video due to tearing down
    kDisableOverlay = 1 << 7,  // Disable HW overlay
  };

  struct ScalingTracker {
    enum ScalingState {
      kNeeedsNoSclaing = 0,  // Needs no scaling.
      kNeedsScaling = 1,     // Needs scaling.
    };
    float scaling_height = 1.0;
    float scaling_width = 1.0;
    uint32_t scaling_state_ = ScalingTracker::kNeeedsNoSclaing;
  };

  struct FrameStateTracker {
    enum FrameState {
      kPrepareComposition = 1 << 0,  // Preparing for current frame composition.
      kPrepareIdleComposition =
          1 << 1,  // Next frame should be composited using Single plane.
      kRenderIdleDisplay = 1 << 2,  // We are in idle mode, disable all overlays
                                    // and use only one plane.
      kRevalidateLayers = 1 << 3,   // We disabled overlay usage for idle mode,
                                    // if we are continously updating
      // frames, revalidate layers to use planes.
      kTrackingFrames =
          1 << 4,               // Tracking frames to see when layers need to be
                                // revalidated after
                                // disabling overlays for idle case scenario.
      kIgnoreUpdates = 1 << 5,  // Ignore present display calls.
      kForceIgnoreUpdates = 1 << 6  // Ignore all commits/updates.
    };

    uint32_t idle_frames_ = 0;
    bool has_cursor_layer_ = false;
    SpinLock idle_lock_;
    int state_ = kPrepareComposition;
    uint32_t revalidate_frames_counter_ = 0;
    size_t total_planes_ = 1;
  };

  struct ScopedStateTracker {
    void ForceSurfaceRelease() {
      forced_ = true;
    }

    bool forced_ = false;
  };

  struct ScopedIdleStateTracker : public ScopedStateTracker {
    ScopedIdleStateTracker(struct FrameStateTracker& tracker,
                           Compositor& compositor,
                           ResourceManager* resource_manager,
                           DisplayQueue* queue)
        : tracker_(tracker),
          compositor_(compositor),
          resource_manager_(resource_manager),
          queue_(queue) {
      tracker_.idle_lock_.lock();
      tracker_.state_ |= FrameStateTracker::kPrepareComposition;
      tracker_.has_cursor_layer_ = false;
      if (tracker_.state_ & FrameStateTracker::kPrepareIdleComposition) {
        tracker_.state_ |= FrameStateTracker::kRenderIdleDisplay;
        tracker_.state_ &= ~FrameStateTracker::kPrepareIdleComposition;
      }

      resource_manager_->RefreshBufferCache();
      tracker_.idle_lock_.unlock();
    }

    bool RenderIdleMode() const {
      return tracker_.state_ & FrameStateTracker::kRenderIdleDisplay;
    }

    bool RevalidateLayers() const {
      return tracker_.state_ & FrameStateTracker::kRevalidateLayers;
    }

    bool TrackingFrames() const {
      return tracker_.state_ & FrameStateTracker::kTrackingFrames;
    }

    void ResetTrackerState() {
      if (tracker_.state_ & FrameStateTracker::kIgnoreUpdates) {
        if (tracker_.state_ & FrameStateTracker::kForceIgnoreUpdates) {
          tracker_.state_ = FrameStateTracker::kIgnoreUpdates;
          tracker_.state_ |= FrameStateTracker::kForceIgnoreUpdates;
        } else
          tracker_.state_ = FrameStateTracker::kIgnoreUpdates;
      } else {
        tracker_.state_ = 0;
      }

      tracker_.revalidate_frames_counter_ = 0;
    }

    bool IgnoreUpdate() const {
      return tracker_.state_ & FrameStateTracker::kIgnoreUpdates;
    }

    void FrameHasCursor() {
      tracker_.has_cursor_layer_ = true;
    }

    ~ScopedIdleStateTracker() {
      tracker_.idle_lock_.lock();
      // Reset idle frame count. We want that idle frames
      // are continuous to detect idle mode scenario.
      tracker_.idle_frames_ = 0;

      tracker_.state_ &= ~FrameStateTracker::kPrepareComposition;
      if (tracker_.state_ & FrameStateTracker::kRenderIdleDisplay) {
        tracker_.state_ &= ~FrameStateTracker::kRenderIdleDisplay;
        tracker_.state_ |= FrameStateTracker::kTrackingFrames;
        tracker_.revalidate_frames_counter_ = 0;
      } else if (tracker_.state_ & FrameStateTracker::kTrackingFrames) {
        if (tracker_.revalidate_frames_counter_ > 3) {
          tracker_.state_ &= ~FrameStateTracker::kTrackingFrames;
          tracker_.state_ |= FrameStateTracker::kRevalidateLayers;
          tracker_.revalidate_frames_counter_ = 0;
        } else {
          tracker_.revalidate_frames_counter_++;
        }
      } else if (tracker_.state_ & FrameStateTracker::kRevalidateLayers) {
        tracker_.state_ &= ~FrameStateTracker::kRevalidateLayers;
        tracker_.revalidate_frames_counter_ = 0;
      }

      tracker_.total_planes_ = queue_->previous_plane_state_.size();
      tracker_.idle_lock_.unlock();

      // Free any surfaces.
      queue_->display_plane_manager_->ReleaseFreeOffScreenTargets(forced_);

      if (resource_manager_->PreparePurgedResources())
        compositor_.FreeResources();
    }

   private:
    struct FrameStateTracker& tracker_;
    Compositor& compositor_;
    ResourceManager* resource_manager_;
    DisplayQueue* queue_;
  };

  // State trackers for cloned display.
  struct ScopedCloneStateTracker : public ScopedStateTracker {
    ScopedCloneStateTracker(Compositor& compositor,
                            ResourceManager* resource_manager,
                            DisplayQueue* queue)
        : compositor_(compositor),
          resource_manager_(resource_manager),
          queue_(queue) {
      resource_manager_->RefreshBufferCache();
    }

    ~ScopedCloneStateTracker() {
      // Free any surfaces.
      queue_->display_plane_manager_->ReleaseFreeOffScreenTargets(forced_);

      if (resource_manager_->PreparePurgedResources())
        compositor_.FreeResources();
    }

   private:
    Compositor& compositor_;
    ResourceManager* resource_manager_;
    DisplayQueue* queue_;
  };

  void HandleExit();
  bool ForcePlaneValidation(int add_index, int remove_index,
                            int total_layers_size, size_t total_planes);
  void GetCachedLayers(const std::vector<OverlayLayer>& layers,
                       int& re_validate_begin,
                       DisplayPlaneStateList& composition);

  void SetReleaseFenceToLayers(int32_t fence,
                               std::vector<HwcLayer*>& source_layers);

  void SetMediaEffectsState(bool apply_effects,
                            const std::vector<OverlayLayer>& layers,
                            DisplayPlaneStateList& current_composition_planes);

  void UpdateOnScreenSurfaces();

  // Re-initialize all state. When we are hearing this means the
  // queue is teraing down or re-started for some reason.
  void ResetQueue();

  void HandleCommitFailure(DisplayPlaneStateList& current_composition_planes);

  void InitializeOverlayLayers(std::vector<HwcLayer*>& source_layers,
                               bool handle_constraints,
                               std::vector<OverlayLayer>& layers,
                               bool& has_video_layer, bool& has_cursor_layer,
                               int& re_validate_begin, bool& idle_frame);

  bool AssignAndCommitPlanes(std::vector<OverlayLayer>& layers,
                             std::vector<HwcLayer*>* source_layers,
                             bool validate_layers, int re_validate_begin,
                             bool setMediaEffect, int32_t* retire_fence,
                             ScopedStateTracker* tracker);

  Compositor compositor_;
  uint32_t gpu_fd_;
  uint32_t brightness_;
  float color_transform_matrix_[16];
  HWCColorTransform color_transform_hint_;
  uint32_t contrast_;
  int32_t kms_fence_ = 0;
  struct gamma_colors gamma_;
  struct canvas_color_comps canvas_;
  std::unique_ptr<VblankEventHandler> vblank_handler_;
  std::unique_ptr<DisplayPlaneManager> display_plane_manager_;
  std::unique_ptr<ResourceManager> resource_manager_;
  std::vector<OverlayLayer> in_flight_layers_;
  DisplayPlaneStateList previous_plane_state_;
  FrameStateTracker idle_tracker_;
  ScalingTracker scaling_tracker_;
  // shared_ptr since we need to use this outside of the thread lock (to
  // actually call the hook) and we don't want the memory freed until we're
  // done
  std::shared_ptr<RefreshCallback> refresh_callback_ = NULL;
  uint32_t refrsh_display_id_ = 0;
  int state_ = kConfigurationChanged;
  PhysicalDisplay* display_ = NULL;
  SpinLock power_mode_lock_;
  // to disable hwclock monitoring.
  bool handle_display_initializations_ = true;
  uint32_t plane_transform_ = kIdentity;
  SpinLock video_lock_;
  bool requested_video_effect_ = false;
  bool video_effect_changed_ = false;
  // Set to true when layers are validated and commit fails.
  bool last_commit_failed_update_ = false;
  // Set to true if cloned display needs to be validated.
  bool needs_clone_validation_ = false;
  bool clone_mode_ = false;
  // Set to true if this queue needs to render the offscreen surfaces.
  bool clone_rendered_ = false;
  // Surfaces to be marked as not in use. These
  // are surfaces which are added to surfaces_not_inuse_
  // below.
  std::vector<NativeSurface*> mark_not_inuse_;
  // Surfaces which are currently on screen and
  // need to be marked as not in use during next
  // frame.
  std::vector<NativeSurface*> surfaces_not_inuse_;
  std::vector<HwcLayer*>* source_layers_ = NULL;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYQUEUE_H_
