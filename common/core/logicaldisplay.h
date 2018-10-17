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

#ifndef WSI_LOGICALDISPLAY_H_
#define WSI_LOGICALDISPLAY_H_

#include <stdint.h>
#include <stdlib.h>

#include <memory>

#include <nativedisplay.h>

namespace hwcomposer {

class LogicalDisplayManager;

class LogicalDisplay : public NativeDisplay {
 public:
  LogicalDisplay(LogicalDisplayManager *display_manager,
                 NativeDisplay *physical_display, uint32_t total_divisions,
                 uint32_t index);
  ~LogicalDisplay() override;

  bool Initialize(NativeBufferHandler *buffer_handler,
                  FrameBufferManager * /*frame_buffer_manager*/) override;

  DisplayType Type() const override {
    return DisplayType::kLogical;
  }

  uint32_t Width() const override {
    return width_;
  }

  uint32_t Height() const override {
    return physical_display_->Height();
  }

  uint32_t PowerMode() const override;

  int GetDisplayPipe() override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers, int32_t *retire_fence,
               PixelUploaderCallback *call_back = NULL,
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
  void SetCanvasColor(uint16_t bpc, uint16_t red, uint16_t green, uint16_t blue,
                      uint16_t alpha) override;
  void RestoreVideoDefaultColor(HWCColorControl color) override;
  void SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                           HWCDeinterlaceControl mode) override;
  void RestoreVideoDefaultDeinterlace() override;

  bool IsConnected() const override;

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width,
                          uint32_t display_height) override;

  void CloneDisplay(NativeDisplay *source_display) override;

  bool PresentClone(NativeDisplay * /*display*/) override;

  bool GetDisplayAttribute(uint32_t /*config*/, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;

  uint32_t GetXTranslation() override {
    return (((physical_display_->Width()) / total_divisions_) * index_);
  }

  uint32_t GetLogicalIndex() const override {
    return index_;
  }

  bool EnableVSync() const {
    return enable_vsync_;
  }

  void VSyncUpdate(int64_t timestamp);

  void RefreshUpdate();

  void HotPlugUpdate(bool connected) override;

  void SetHDCPState(HWCContentProtection state,
                    HWCContentType content_type) override;
  void SetHDCPSRM(const int8_t *SRM, uint32_t SRMLength) override;

  bool ContainConnector(const uint32_t connector_id) override;

 private:
  LogicalDisplayManager *logical_display_manager_;
  NativeDisplay *physical_display_;
  std::shared_ptr<RefreshCallback> refresh_callback_ = NULL;
  std::shared_ptr<VsyncCallback> vsync_callback_ = NULL;
  std::shared_ptr<HotPlugCallback> hotplug_callback_ = NULL;
  uint32_t power_mode_ = kOff;
  uint32_t display_id_ = 0;
  uint32_t index_ = 0;
  uint32_t width_ = 0;
  uint32_t total_divisions_ = 1;
  bool enable_vsync_ = false;
};

}  // namespace hwcomposer
#endif  // WSI_LOGICALDISPLAY_H_
