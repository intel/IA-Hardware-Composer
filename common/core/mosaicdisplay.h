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

#ifndef WSI_MOSAICDISPLAY_H_
#define WSI_MOSAICDISPLAY_H_

#include <stdlib.h>
#include <stdint.h>

#include <memory>

#include <nativedisplay.h>
#include <spinlock.h>

namespace hwcomposer {

class MosaicDisplay : public NativeDisplay {
 public:
  MosaicDisplay(const std::vector<NativeDisplay *> displays);
  ~MosaicDisplay() override;

  bool Initialize(NativeBufferHandler *buffer_handler) override;

  DisplayType Type() const override {
    return DisplayType::kMosaic;
  }

  uint32_t Width() const override;

  uint32_t Height() const override;

  uint32_t PowerMode() const override;

  int GetDisplayPipe() override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers, int32_t *retire_fence,
               bool handle_constraints = false) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void RegisterRefreshCallback(std::shared_ptr<RefreshCallback> callback,
                               uint32_t display_id) override;

  void RegisterHotPlugCallback(std::shared_ptr<HotPlugCallback> callback,
                               uint32_t display_id) override;

  void VSyncControl(bool enabled) override;
  bool CheckPlaneFormat(uint32_t format) override;
  void SetGamma(float red, float green, float blue) override;
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue) override;
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue) override;
  void SetExplicitSyncSupport(bool disable_explicit_sync) override;
  void SetVideoScalingMode(uint32_t mode) override;
  void SetVideoColor(HWCColorControl color, float value) override;
  void GetVideoColor(HWCColorControl color, float *value, float *start,
                     float *end) override;
  void RestoreVideoDefaultColor(HWCColorControl color) override;
  void SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                           HWCDeinterlaceControl mode) override;
  void RestoreVideoDefaultDeinterlace() override;

  bool IsConnected() const override;

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width,
                          uint32_t display_height) override;

  void CloneDisplay(NativeDisplay *source_display) override;

  bool PresentClone(std::vector<HwcLayer *> &source_layers,
                    int32_t *retire_fence, bool idle_frame) override;

  bool GetDisplayAttribute(uint32_t /*config*/, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;

  uint32_t GetXTranslation() override {
    return 0;
  }

  bool EnableVSync() const {
    return enable_vsync_;
  }

  void VSyncUpdate(int64_t timestamp);

  void RefreshUpdate();

  void HotPlugUpdate(bool connected);

  void SetHDCPState(HWCContentProtection state) override;

 private:
  std::vector<NativeDisplay *> physical_displays_;
  std::vector<NativeDisplay *> connected_displays_;
  std::shared_ptr<RefreshCallback> refresh_callback_ = NULL;
  std::shared_ptr<VsyncCallback> vsync_callback_ = NULL;
  std::shared_ptr<HotPlugCallback> hotplug_callback_ = NULL;
  int32_t dpix_;
  int32_t dpiy_;
  uint32_t refresh_ = 0;
  uint32_t power_mode_ = kOff;
  uint32_t display_id_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t config_ = 0;
  uint32_t vsync_counter_ = 0;
  uint32_t vsync_divisor_ = 0;
  uint32_t preferred_display_index_ = 0;
  int64_t vsync_timestamp_ = 0;
  bool enable_vsync_ = false;
  bool connected_ = false;
  bool pending_vsync_ = false;
  bool update_connected_displays_ = true;
  SpinLock lock_;
};

}  // namespace hwcomposer
#endif  // WSI_MosaicDisplay_H_
