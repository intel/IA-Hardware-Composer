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
#include "vblankeventhandler.h"
#include "platformdefines.h"

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

class DisplayQueue {
 public:
  DisplayQueue(uint32_t gpu_fd, bool disable_overlay,
               NativeBufferHandler* buffer_handler, PhysicalDisplay* display);
  ~DisplayQueue();

  bool Initialize(float refresh, uint32_t pipe, uint32_t width, uint32_t height,
                  DisplayPlaneHandler* plane_manager);

  bool QueueUpdate(std::vector<HwcLayer*>& source_layers,
                   int32_t* retire_fence);
  bool SetPowerMode(uint32_t power_mode);
  bool CheckPlaneFormat(uint32_t format);
  void SetGamma(float red, float green, float blue);
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue);
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue);
  void SetExplicitSyncSupport(bool disable_explicit_sync);

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id);

  void RegisterRefreshCallback(std::shared_ptr<RefreshCallback> callback,
                               uint32_t display_id);

  void VSyncControl(bool enabled);

  void HandleIdleCase();

  void DisplayConfigurationChanged();

 private:
  void HandleExit();
  struct FrameStateTracker {
    enum FrameState {
      kPrepareComposition = 1 << 0,  // Preparing for current frame composition.
      kRenderIdleDisplay = 1 << 1,  // We are in idle mode, disable all overlays
                                    // and use only one plane.
      kRevalidateLayers = 1 << 3,   // We disabled overlay usage for idle mode,
                                    // if we are continously updating
      // frames, revalidate layers to use planes.
      kTrackingFrames = 1 << 4  // Tracking frames to see when layers need to be
                                // revalidated after
                                // disabling overlays for idle case scenario.
    };

    uint32_t idle_frames_ = 0;
    SpinLock idle_lock_;
    uint32_t state_;
    uint32_t continuous_frames_ = 0;
  };

  struct ScopedIdleStateTracker {
    ScopedIdleStateTracker(struct FrameStateTracker& tracker)
        : tracker_(tracker) {
      tracker_.idle_lock_.lock();
      tracker_.idle_frames_ = 0;
      tracker_.state_ |= FrameStateTracker::kPrepareComposition;
      tracker_.idle_lock_.unlock();
    }

    bool RenderIdleMode() const {
      return tracker_.state_ & FrameStateTracker::kRenderIdleDisplay;
    }

    bool RevalidateLayers() const {
      return tracker_.state_ & FrameStateTracker::kRevalidateLayers;
    }

    void ResetTrackerState() {
      if (!(tracker_.state_ & FrameStateTracker::kRenderIdleDisplay)) {
        tracker_.state_ = 0;
	tracker_.continuous_frames_ = 0;
      }
    }

    ~ScopedIdleStateTracker() {
      tracker_.idle_lock_.lock();
      tracker_.state_ &= ~FrameStateTracker::kPrepareComposition;
      if (tracker_.state_ & FrameStateTracker::kRenderIdleDisplay) {
        tracker_.state_ &= ~FrameStateTracker::kRenderIdleDisplay;
	tracker_.state_ |= FrameStateTracker::kTrackingFrames;
        tracker_.continuous_frames_ = 0;
      } else if (tracker_.state_ & FrameStateTracker::kTrackingFrames) {
        if (tracker_.continuous_frames_ > 10) {
          tracker_.state_ &= ~FrameStateTracker::kTrackingFrames;
          tracker_.state_ |= FrameStateTracker::kRevalidateLayers;
          tracker_.continuous_frames_ = 0;
        } else {
          tracker_.continuous_frames_++;
        }
      } else if (tracker_.state_ & FrameStateTracker::kRevalidateLayers) {
        tracker_.state_ &= ~FrameStateTracker::kRevalidateLayers;
	tracker_.continuous_frames_ = 0;
      }

      tracker_.idle_lock_.unlock();
    }

   private:
    struct FrameStateTracker& tracker_;
  };

  void GetCachedLayers(const std::vector<OverlayLayer>& layers,
                       DisplayPlaneStateList* composition, bool* render_layers);
  void SetReleaseFenceToLayers(int32_t fence,
                               std::vector<HwcLayer*>& source_layers) const;
  void UpdateSurfaceInUse();

  Compositor compositor_;
  uint32_t frame_;
  uint32_t gpu_fd_;
  uint32_t brightness_;
  uint32_t contrast_;
  int32_t kms_fence_ = 0;
  struct gamma_colors gamma_;
  bool needs_color_correction_ = false;
  bool configuration_changed_ = true;
  bool use_layer_cache_ = false;
  bool disable_overlay_usage_ = false;
  std::unique_ptr<VblankEventHandler> vblank_handler_;
  std::unique_ptr<DisplayPlaneManager> display_plane_manager_;
  std::vector<OverlayLayer> in_flight_layers_;
  std::vector<NativeSurface*> in_flight_surfaces_;
  std::vector<NativeSurface*> previous_surfaces_;
  DisplayPlaneStateList previous_plane_state_;
  NativeBufferHandler* buffer_handler_;
  FrameStateTracker idle_tracker_;
  // shared_ptr since we need to use this outside of the thread lock (to
  // actually call the hook) and we don't want the memory freed until we're
  // done
  std::shared_ptr<RefreshCallback> refresh_callback_ = NULL;
  uint32_t refrsh_display_id_ = 0;
  PhysicalDisplay* display_ = NULL;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYQUEUE_H_
