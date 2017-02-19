/*
// Copyright (c) 2016 Intel Corporation
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

#ifndef INTERNAL_DISPLAY_H_
#define INTERNAL_DISPLAY_H_

#include "platformdefines.h"

#include <mutex>
#include <stdint.h>
#include <xf86drmMode.h>

#include <drmscopedtypes.h>
#include <nativedisplay.h>
#include <nativebufferhandler.h>

#include "compositor.h"
#include "pageflipeventhandler.h"
#include "scopedfd.h"
#include "spinlock.h"

namespace hwcomposer {
class DisplayPlaneState;
class DisplayPlaneManager;
class GpuDevice;
class NativeSync;
struct HwcLayer;

class Display : public NativeDisplay {
 public:
  Display(uint32_t gpu_fd, NativeBufferHandler &handler,
                  uint32_t pipe_id, uint32_t crtc_id);
  ~Display();

  bool Initialize() override;

  DisplayType Type() const override {
    return DisplayType::kInternal;
  }

  uint32_t Pipe() const override {
    return pipe_;
  }

  int32_t Width() const override {
    return width_;
  }

  int32_t Height() const override {
    return height_;
  }

  int32_t GetRefreshRate() const override {
    return refresh_;
  }

  bool GetDisplayAttribute(uint32_t config, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetDpmsMode(uint32_t dpms_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void VSyncControl(bool enabled) override;

 protected:
  uint32_t CrtcId() const override {
    return crtc_id_;
  }

  bool Connect(const drmModeModeInfo &mode_info,
               const drmModeConnector *connector) override;

  bool IsConnected() const override {
    return is_connected_;
  }

  void DisConnect() override;

  void ShutDown() override;

 private:
  enum PendingModeset { kNone = 0, kModeset = 1 << 0 };

  void ShutDownPipe();
  void InitializeResources();
  bool ApplyPendingModeset(drmModeAtomicReqPtr property_set, NativeSync *sync,
                           uint64_t *out_fence);

  void GetDrmObjectProperty(const char *name,
                            const ScopedDrmObjectPropertyPtr &props,
                            uint32_t *id) const;

  NativeBufferHandler &buffer_handler_;
  Compositor compositor_;
  drmModeModeInfo mode_;
  uint32_t frame_;
  uint32_t dpms_prop_;
  uint32_t crtc_prop_;
  uint32_t active_prop_;
  uint32_t mode_id_prop_;
  uint32_t out_fence_ptr_prop_;
  uint32_t crtc_id_;
  uint32_t pipe_;
  uint32_t dpms_mode_ = DRM_MODE_DPMS_ON;
  uint32_t connector_;
  uint32_t pending_operations_ = kNone;
  uint32_t blob_id_ = 0;
  uint32_t old_blob_id_ = 0;
  int32_t width_;
  int32_t height_;
  int32_t dpix_;
  int32_t dpiy_;
  uint32_t gpu_fd_;
  bool is_connected_;
  bool is_powered_off_;
  float refresh_;
  ScopedFd out_fence_ = -1;
  std::unique_ptr<PageFlipEventHandler> flip_handler_;
  std::unique_ptr<DisplayPlaneManager> display_plane_manager_;
  SpinLock spin_lock_;
  std::vector<OverlayLayer> previous_layers_;
  DisplayPlaneStateList previous_plane_state_;
};

}  // namespace hwcomposer
#endif  // INTERNAL_DISPLAY_H_
