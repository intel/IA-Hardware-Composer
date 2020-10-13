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

#ifndef WSI_PHYSICALDISPLAY_H_
#define WSI_PHYSICALDISPLAY_H_

#ifndef DEFAULT_CONFIG_ID
#define DEFAULT_CONFIG_ID 0
#endif

#include <stdint.h>
#include <stdlib.h>

#include <nativedisplay.h>

#include <memory>
#include <vector>

#include <spinlock.h>
#include "displayplanehandler.h"
#include "displayplanestate.h"
#include "platformdefines.h"

namespace hwcomposer {
class DisplayPlaneState;
class DisplayPlaneManager;
class DisplayQueue;
class NativeBufferHandler;
class GpuDevice;
struct HwcLayer;

class PhysicalDisplay : public NativeDisplay, public DisplayPlaneHandler {
 public:
  PhysicalDisplay(uint32_t gpu_fd, uint32_t pipe_id);
  ~PhysicalDisplay() override;

  bool Initialize(NativeBufferHandler *buffer_handler) override;

  DisplayType Type() const override {
    return DisplayType::kInternal;
  }

  uint32_t Width() const override {
    return width_;
  }

  uint32_t Height() const override {
    return height_;
  }

  uint32_t PowerMode() const override;

  int GetDisplayPipe() override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetCustomResolution(const HwcRect<int32_t> &) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers, int32_t *retire_fence,
               PixelUploaderCallback *call_back = NULL,
               bool handle_constraints = false) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  int RegisterVsyncPeriodCallback(std::shared_ptr<VsyncPeriodCallback> callback,
                                  uint32_t display_id) override;

  void RegisterRefreshCallback(std::shared_ptr<RefreshCallback> callback,
                               uint32_t display_id) override;

  void RegisterHotPlugCallback(std::shared_ptr<HotPlugCallback> callback,
                               uint32_t display_id) override;

  void VSyncControl(bool enabled) override;
  bool CheckPlaneFormat(uint32_t format) override;
  void SetGamma(float red, float green, float blue) override;
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue) override;
  void SetColorTransform(const float *matrix, HWCColorTransform hint) override;
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue) override;
  void SetDisableExplicitSync(bool disable_explicit_sync) override;
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

  void Connect() override;

  bool IsConnected() const override;

  bool TestCommit(const DisplayPlaneStateList &commit_planes) const override;

  bool PopulatePlanes(
      std::vector<std::unique_ptr<DisplayPlane>> &overlay_planes) override;

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width,
                          uint32_t display_height) override;

  void CloneDisplay(NativeDisplay *source_display) override;

  bool PresentClone(NativeDisplay * /*display*/) override;

  bool GetDisplayAttribute(uint32_t /*config*/, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;

  bool IsBypassClientCTM() const;
  void GetDisplayCapabilities(uint32_t *outNumCapabilities,
                              uint32_t *outCapabilities) override;

  bool EnableDRMCommit(bool enable) override;

  /**
   * API for composition to non-zero X coordinate.
   * This is applicable when float mode is enabled.
   * Parameters are read from config file.
   */
  uint32_t GetXTranslation() override {
    return rect_.left;
  }

  /**
   * API for composition to non-zero Y coordinate.
   * This is applicable when float mode is enabled.
   * Parameters are read from config file.
   */
  uint32_t GetYTranslation() override {
    return rect_.top;
  }

  void OwnPresentation(NativeDisplay *clone) override;

  void DisOwnPresentation(NativeDisplay *clone) override;

  void SetDisplayOrder(uint32_t display_order) override;

  void RotateDisplay(HWCRotation rotation) override;

  const NativeBufferHandler *GetNativeBufferHandler() const override;

  void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                            uint32_t pavp_instance_id) override;

  /**
   * API for setting color correction for display.
   */
  virtual void SetColorCorrection(struct gamma_colors gamma, uint32_t contrast,
                                  uint32_t brightness) const = 0;
  /**
   * API for setting color transform matrix.
   */
  virtual void SetColorTransformMatrix(
      const float *color_transform_matrix,
      HWCColorTransform color_transform_hint) const = 0;

  /**
   * API is called when display needs to be disabled.
   * @param composition_planes contains list of planes enabled last
   * frame.
   */
  virtual void Disable(const DisplayPlaneStateList &composition_planes) = 0;

  /**
   * API for showing content on display
   * @param composition_planes contains list of layers which need to displayed.
   * @param previous_composition_planes contains list of planes enabled last
   * frame.
   * @param disable_explicit_fence is set to true if we want a hardware fence
   *        associated with this commit request set to commit_fence.
   * @param commit_fence hardware fence associated with this commit request.
   */
  virtual bool Commit(const DisplayPlaneStateList &composition_planes,
                      const DisplayPlaneStateList &previous_composition_planes,
                      bool disable_explicit_fence, int32_t previous_fence,
                      int32_t *commit_fence, bool *previous_fence_released) = 0;

  /**
   * API is called if current active display configuration has changed.
   * Implementations need to reset any state in this case.
   */
  virtual void UpdateDisplayConfig() = 0;

  virtual bool GetDisplayVsyncPeriod(uint32_t *outVsyncPeriod) = 0;

  /**
   * API for powering on the display
   */
  virtual void PowerOn() = 0;

  /**
   * API for initializing display. Implementation needs to handle all things
   * needed to set up the physical display.
   */
  virtual bool InitializeDisplay() = 0;

  virtual void NotifyClientsOfDisplayChangeStatus() = 0;

  /**
   * API for setting the color of the pipe canvas.
   */
  virtual void SetPipeCanvasColor(uint16_t bpc, uint16_t red, uint16_t green,
                                  uint16_t blue, uint16_t alpha) const = 0;

  /**
   * API for setting the colordepth of the pipe.
   */
  virtual bool SetPipeMaxBpc(uint16_t max_bpc) const = 0;

  /**
   * API for informing the display that it might be disconnected in near
   * future.
   */
  void MarkForDisconnect();

  /**
   * API for informing the clients resgistered via RegisterHotPlugCallback
   * that this display had been disconnected.
   */
  void NotifyClientOfConnectedState();

  /**
   * API for informing the clients resgistered via RegisterHotPlugCallback
   * that this display had been connected.
   */
  void NotifyClientOfDisConnectedState();

  /**
  * API to disconnect the display. This is called when this display
  * is physically disconnected.
  */
  virtual void DisConnect();

  /**
   * API to handle any lazy initializations which need to be handled
   * during first present call.
   */
  virtual void HandleLazyInitialization() {
  }

  bool IsFakeConnected() {
    return connection_state_ & kFakeConnected;
  }

  int GetTotalOverlays() const override;

 private:
  bool UpdatePowerMode();
  void RefreshClones();
  void HandleClonedDisplays(NativeDisplay *display);

 protected:
  enum DisplayConnectionStatus {
    kDisconnected = 1 << 0,
    kConnected = 1 << 1,
    kDisconnectionInProgress = 1 << 2,
    kFakeConnected = 1 << 3
  };

  enum DisplayState {
    kNone = 1 << 0,
    kNeedsModeset = 1 << 1,
    kPendingPowerMode = 1 << 2,
    kUpdateDisplay = 1 << 3,
    kInitialized = 1 << 4,  // Display Queue is initialized.
    kRefreshClonedDisplays = 1 << 5,
    kHandlePendingHotPlugNotifications = 1 << 6,
    kNotifyClient = 1 << 7,  // Notify client as display connection physical
                             // status has changed.
    kUpdateConfig = 1 << 8
  };

  uint32_t pipe_;
  int32_t width_;
  int32_t height_;
  HwcRect<int32_t> rect_;
  int32_t custom_resolution_;
  uint32_t gpu_fd_;
  uint32_t power_mode_ = kOn;
  int display_state_ = kNone;
  int connection_state_ = kDisconnected;
  uint32_t hot_plug_display_id_ = 0;
  // This differs from pipe_ as upper layers can
  // change order of physical display from setting
  // file. This may or may not be same as pipe_.
  uint32_t ordered_display_id_ = 0;
  SpinLock modeset_lock_;
  std::unique_ptr<DisplayQueue> display_queue_;
  std::shared_ptr<HotPlugCallback> hotplug_callback_ = NULL;
  NativeDisplay *source_display_ = NULL;
  std::vector<NativeDisplay *> cloned_displays_;
  std::vector<NativeDisplay *> clones_;
  uint32_t config_ = DEFAULT_CONFIG_ID;
  bool bypassClientCTM_ = false;
};

}  // namespace hwcomposer
#endif  // WSI_PHYSICALDISPLAY_H_
