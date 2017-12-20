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

#include <stdlib.h>
#include <stdint.h>

#include <queue>
#include <memory>
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

class PhysicalDisplay;
class DisplayPlaneHandler;
struct HwcLayer;
class NativeBufferHandler;

static uint32_t kidleframes = 250;
class DisplayQueue {
 public:
  DisplayQueue(uint32_t gpu_fd, bool disable_overlay,
               NativeBufferHandler* buffer_handler, PhysicalDisplay* display);
  ~DisplayQueue();

  bool Initialize(uint32_t pipe, uint32_t width, uint32_t height,
                  DisplayPlaneHandler* plane_manager);

  bool QueueUpdate(std::vector<HwcLayer*>& source_layers, int32_t* retire_fence,
                   bool idle_update, bool handle_constraints);
  bool SetPowerMode(uint32_t power_mode);
  bool CheckPlaneFormat(uint32_t format);
  void SetGamma(float red, float green, float blue);
  void SetColorTransform(const float *matrix, HWCColorTransform hint);
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue);
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue);
  void SetExplicitSyncSupport(bool disable_explicit_sync);
  void SetVideoColor(HWCColorControl color, float value);
  void GetVideoColor(HWCColorControl color,
                     float* value, float* start, float* end);
  void RestoreVideoDefaultColor(HWCColorControl color);

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id);

  void RegisterRefreshCallback(std::shared_ptr<RefreshCallback> callback,
                               uint32_t display_id);

  void VSyncControl(bool enabled);

  void HandleIdleCase();

  void DisplayConfigurationChanged();

  void ForceRefresh();

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width, uint32_t display_height);

  void SetCloneMode(bool cloned);

  bool WasLastFrameIdleUpdate() {
    return state_ & kLastFrameIdleUpdate;
  }

  void RotateDisplay(HWCRotation rotation);

  void IgnoreUpdates();
 private:
  enum QueueState {
    kNeedsColorCorrection = 1 << 0,  // Needs Color correction.
    kConfigurationChanged = 1 << 1,  // Layers need to be re-validated.
    kPoweredOn = 1 << 2,
    kDisableOverlayUsage = 1 << 3,     // Disable Overlays.
    kMarkSurfacesForRelease = 1 << 4,  // Mark surfaces to be released.
    kReleaseSurfaces = 1 << 5,         // Release Native Surfaces.
    kIgnoreIdleRefresh =
        1 << 6,            // Ignore refresh request during idle callback.
    kClonedMode = 1 << 7,  // We are in cloned mode.
    kLastFrameIdleUpdate = 1 << 8  // Last frame was a refresh for Idle state.
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
          1 << 4,              // Tracking frames to see when layers need to be
                               // revalidated after
                               // disabling overlays for idle case scenario.
      kIgnoreUpdates = 1 << 5  // Ignore present display calls.
    };

    uint32_t idle_frames_ = 0;
    SpinLock idle_lock_;
    int state_ = kPrepareComposition;
    uint32_t revalidate_frames_counter_ = 0;
    uint32_t idle_reset_frames_counter_ = 0;
  };

  struct ScopedIdleStateTracker {
    ScopedIdleStateTracker(struct FrameStateTracker& tracker,
                           Compositor& compositor)
        : tracker_(tracker), compositor_(compositor) {
      tracker_.idle_lock_.lock();
      tracker_.state_ |= FrameStateTracker::kPrepareComposition;
      if (tracker_.state_ & FrameStateTracker::kPrepareIdleComposition) {
        tracker_.state_ |= FrameStateTracker::kRenderIdleDisplay;
        tracker_.state_ &= ~FrameStateTracker::kPrepareIdleComposition;
      }

      tracker_.idle_lock_.unlock();
    }

    bool RenderIdleMode() const {
      return tracker_.state_ & FrameStateTracker::kRenderIdleDisplay;
    }

    bool RevalidateLayers() const {
      return tracker_.state_ & FrameStateTracker::kRevalidateLayers;
    }

    void ResetTrackerState() {
      if (tracker_.state_ & FrameStateTracker::kIgnoreUpdates) {
	tracker_.state_ = FrameStateTracker::kIgnoreUpdates;
      } else {
	tracker_.state_ = 0;
      }

      tracker_.revalidate_frames_counter_ = 0;
    }

    bool IgnoreUpdate() const {
      return tracker_.state_ & FrameStateTracker::kIgnoreUpdates;
    }

    ~ScopedIdleStateTracker() {
      tracker_.idle_lock_.lock();
      tracker_.idle_reset_frames_counter_ = 0;
      // Reset idle frame count if it's less than
      // kidleframes. We want that idle frames
      // are continuous to detect idle mode scenario.
      if (tracker_.idle_frames_ < kidleframes)
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

      tracker_.idle_lock_.unlock();

      compositor_.FreeResources();
    }

   private:
    struct FrameStateTracker& tracker_;
    Compositor& compositor_;
  };

  void HandleExit();
  void GetCachedLayers(const std::vector<OverlayLayer>& layers,
                       bool cursor_layer_removed,
                       DisplayPlaneStateList* composition, bool* render_layers,
                       bool* can_ignore_commit, bool* re_validate_commit,
                       bool* force_full_validation);
  void SetReleaseFenceToLayers(int32_t fence,
                               std::vector<HwcLayer*>& source_layers) const;

  void SetMediaEffectsState(
      bool apply_effects, const std::vector<OverlayLayer>& layers,
      DisplayPlaneStateList& current_composition_planes) const;

  void UpdateSurfaceInUse(bool in_use,
                          DisplayPlaneStateList& current_composition_planes);
  void RecyclePreviousPlaneSurfaces();

  void IgnoreCompositionResults(
      DisplayPlaneStateList& current_composition_planes);

  void ReleaseSurfaces();
  void ReleaseSurfacesAsNeeded(bool layers_validated);

  // Re-initialize all state. When we are hearing this means the
  // queue is teraing down or re-started for some reason.
  void ResetQueue();

  Compositor compositor_;
  uint32_t gpu_fd_;
  uint32_t brightness_;
  float color_transform_matrix_[16];
  HWCColorTransform color_transform_hint_;
  uint32_t contrast_;
  int32_t kms_fence_ = 0;
  struct gamma_colors gamma_;
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
  uint32_t total_cursor_layers_ = 0;
  PhysicalDisplay* display_ = NULL;
  SpinLock power_mode_lock_;
  bool handle_display_initializations_ = true;  // to disable hwclock thread.
  HWCRotation rotation_ = kRotateNone;
  SpinLock video_lock_;
  bool requested_video_effect_ = false;
  bool applied_video_effect_ = false;
  // Set to true when layers are validated and commit fails.
  bool last_commit_failed_update_ = false;
  // Surfaces to be marked as not in use. These
  // are surfaces which are added to surfaces_not_inuse_
  // below.
  std::vector<NativeSurface*> mark_not_inuse_;
  // Surfaces which are currently on screen and
  // need to be marked as not in use during next
  // frame.
  std::vector<NativeSurface*> surfaces_not_inuse_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYQUEUE_H_
