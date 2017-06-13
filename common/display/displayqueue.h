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

#include <drmscopedtypes.h>
#include <spinlock.h>

#include <stdlib.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <queue>
#include <memory>
#include <vector>

#include "compositor.h"
#include "hwcthread.h"
#include "vblankeventhandler.h"
#include "platformdefines.h"

namespace hwcomposer {
struct gamma_colors {
  float red;
  float green;
  float blue;
};

class DisplayPlaneManager;
struct HwcLayer;
class OverlayBufferManager;

class DisplayQueue {
 public:
  DisplayQueue(uint32_t gpu_fd, uint32_t crtc_id,
               OverlayBufferManager* buffer_manager);
  ~DisplayQueue();

  bool Initialize(float refresh, uint32_t width, uint32_t height, uint32_t pipe,
                  uint32_t connector, const drmModeModeInfo& mode_info);

  bool QueueUpdate(std::vector<HwcLayer*>& source_layers,
                   int32_t* retire_fence);
  bool SetPowerMode(uint32_t power_mode);
  bool CheckPlaneFormat(uint32_t format);
  void SetGamma(float red, float green, float blue);
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue);
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue);
  bool SetBroadcastRGB(const char* range_property);
  void SetExplicitSyncSupport(bool disable_explicit_sync);

  void HandleExit();

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id);

  void VSyncControl(bool enabled);

  void HandleIdleCase();

  bool SetActiveConfig(drmModeModeInfo& mode_info);

 private:
  struct IdleFrameTracker {
    uint32_t idle_frames_ = 0;
    SpinLock idle_lock_;
    bool preparing_composition_ = true;
  };

  struct ScopedIdleStateTracker {
    ScopedIdleStateTracker(struct IdleFrameTracker& tracker)
        : tracker_(tracker) {
      tracker_.idle_lock_.lock();
      tracker_.idle_frames_ = 0;
      tracker_.preparing_composition_ = true;
      tracker_.idle_lock_.unlock();
    }

    ~ScopedIdleStateTracker() {
      tracker_.idle_lock_.lock();
      tracker_.preparing_composition_ = false;
      tracker_.idle_lock_.unlock();
    }

   private:
    struct IdleFrameTracker& tracker_;
  };

  bool ApplyPendingModeset(drmModeAtomicReqPtr property_set);
  void GetCachedLayers(const std::vector<OverlayLayer>& layers,
                       DisplayPlaneStateList* composition, bool* render_layers);
  bool GetFence(drmModeAtomicReqPtr property_set, int32_t* out_fence);
  void GetDrmObjectProperty(const char* name,
                            const ScopedDrmObjectPropertyPtr& props,
                            uint32_t* id) const;
  void ApplyPendingLUT(struct drm_color_lut* lut) const;
  void GetDrmObjectPropertyValue(const char* name,
                                 const ScopedDrmObjectPropertyPtr& props,
                                 uint64_t* value) const;

  void SetColorCorrection(struct gamma_colors gamma, uint32_t contrast,
                          uint32_t brightness) const;
  float TransformGamma(float value, float gamma) const;
  float TransformContrastBrightness(float value, float brightness,
                                    float contrast) const;

  Compositor compositor_;
  drmModeModeInfo mode_;
  uint32_t frame_;
  uint32_t dpms_prop_;
  uint32_t dpms_mode_ = DRM_MODE_DPMS_ON;
  uint32_t out_fence_ptr_prop_;
  uint32_t active_prop_;
  uint32_t mode_id_prop_;
  uint32_t lut_id_prop_;
  uint32_t crtc_id_;
  uint32_t connector_;
  uint32_t crtc_prop_;
  uint32_t blob_id_ = 0;
  uint32_t old_blob_id_ = 0;
  uint32_t gpu_fd_;
  uint32_t brightness_;
  uint32_t contrast_;
  uint32_t flags_ = DRM_MODE_ATOMIC_ALLOW_MODESET;
  int32_t kms_fence_ = 0;
  struct gamma_colors gamma_;
  uint64_t lut_size_;
  uint32_t broadcastrgb_id_;
  int64_t broadcastrgb_full_;
  int64_t broadcastrgb_automatic_;
  bool needs_color_correction_ = false;
  bool use_layer_cache_ = false;
  bool needs_modeset_ = true;
  bool disable_overlay_usage_ = false;
  std::unique_ptr<VblankEventHandler> vblank_handler_;
  std::unique_ptr<DisplayPlaneManager> display_plane_manager_;
  std::vector<OverlayLayer> in_flight_layers_;
  std::vector<OverlayLayer> previous_layers_;
  std::vector<HwcRect<int>> previous_layers_rects_;
  DisplayPlaneStateList previous_plane_state_;
  OverlayBufferManager* buffer_manager_;
  std::vector<NativeSurface*> in_flight_surfaces_;
  std::vector<NativeSurface*> previous_surfaces_;
  IdleFrameTracker idle_tracker_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYQUEUE_H_
