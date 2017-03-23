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
#include <scopedfd.h>
#include <spinlock.h>

#include <stdlib.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <queue>
#include <memory>
#include <vector>

#include "compositor.h"
#include "hwcthread.h"
#include "kmsfencehandler.h"
#include "nativesync.h"
#include "platformdefines.h"

namespace hwcomposer {
class DisplayPlaneManager;
struct HwcLayer;
class NativeBufferHandler;
class PageFlipEventHandler;

class DisplayQueue {
 public:
  DisplayQueue(uint32_t gpu_fd, uint32_t crtc_id);
  ~DisplayQueue();

  bool Initialize(uint32_t width, uint32_t height, uint32_t pipe,
                  uint32_t connector, const drmModeModeInfo& mode_info,
                  NativeBufferHandler* buffer_handler);

  bool QueueUpdate(std::vector<HwcLayer*>& source_layers);
  bool SetPowerMode(uint32_t power_mode);

  void HandleExit();

 private:
  bool ApplyPendingModeset(drmModeAtomicReqPtr property_set);
  bool GetFence(drmModeAtomicReqPtr property_set, uint64_t* out_fence);
  void GetDrmObjectProperty(const char* name,
                            const ScopedDrmObjectPropertyPtr& props,
                            uint32_t* id) const;

  Compositor compositor_;
  drmModeModeInfo mode_;
  uint32_t frame_;
  uint32_t dpms_prop_;
  uint32_t dpms_mode_ = DRM_MODE_DPMS_ON;
  uint32_t out_fence_ptr_prop_;
  uint32_t active_prop_;
  uint32_t mode_id_prop_;
  uint32_t crtc_id_;
  uint32_t connector_;
  uint32_t crtc_prop_;
  uint32_t blob_id_ = 0;
  uint32_t old_blob_id_ = 0;
  uint32_t gpu_fd_;
  uint64_t fence_ = 0;
  bool needs_modeset_ = false;
  std::unique_ptr<PageFlipEventHandler> flip_handler_;
  std::unique_ptr<KMSFenceEventHandler> kms_fence_handler_;
  std::unique_ptr<DisplayPlaneManager> display_plane_manager_;
  std::vector<OverlayLayer> previous_layers_;
  DisplayPlaneStateList previous_plane_state_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYQUEUE_H_
