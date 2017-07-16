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

#include <stdlib.h>
#include <stdint.h>

#include <nativedisplay.h>

#include <memory>
#include <vector>

#include "platformdefines.h"
#include "displayplanestate.h"
#include "displayplanehandler.h"
#include <spinlock.h>

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

  uint32_t Pipe() const override {
    return pipe_;
  }

  uint32_t Width() const override {
    return width_;
  }

  uint32_t Height() const override {
    return height_;
  }

  int32_t GetRefreshRate() const override {
    return refresh_;
  }

  uint32_t PowerMode() const override;

  int GetDisplayPipe() override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers,
               int32_t *retire_fence) override;

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

  void DisConnect() override;

  void Connect() override;

  bool IsConnected() const override {
    return !(display_state_ & kDisconnectionInProgress);
  }

  bool TestCommit(
      const std::vector<OverlayPlane> &commit_planes) const override;

  bool PopulatePlanes(
      std::unique_ptr<DisplayPlane> &primary_plane,
      std::unique_ptr<DisplayPlane> &cursor_plane,
      std::vector<std::unique_ptr<DisplayPlane>> &overlay_planes) override;

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width,
                          uint32_t display_height) override;

  void CloneDisplay(NativeDisplay *source_display) override;

  bool PresentClone(std::vector<HwcLayer *> &source_layers,
                    int32_t *retire_fence) override;

  void OwnPresentation(NativeDisplay *clone);

  void DisOwnPresentation(NativeDisplay *clone);

  /**
  * API for setting color correction for display.
  */
  virtual void SetColorCorrection(struct gamma_colors gamma, uint32_t contrast,
                                  uint32_t brightness) const = 0;

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
                      bool disable_explicit_fence, int32_t *commit_fence) = 0;

  /**
  * API is called if current active display configuration has changed.
  * Implementations need to reset any state in this case.
  */
  virtual void UpdateDisplayConfig() = 0;

  /**
  * API for powering on the display
  */
  virtual void PowerOn() = 0;

  /**
  * API for initializing display. Implementation needs to handle all things
  * needed to set up the physical display.
  */
  virtual bool InitializeDisplay() = 0;

 private:
  bool UpdatePowerMode();
  void RefreshClones();
  void HandleClonedDisplays(std::vector<HwcLayer *> &source_layers);

 protected:
  enum DisplayState {
    kConnected = 1 << 0,
    kNeedsModeset = 1 << 1,
    kPendingPowerMode = 1 << 2,
    kUpdateDisplay = 1 << 3,
    kDisconnectionInProgress = 1 << 4,
    kInitialized = 1 << 5,  // Display Queue is initialized.
    kRefreshClonedDisplays = 1 << 6
  };

  uint32_t pipe_;
  uint32_t config_ = 0;
  int32_t width_;
  int32_t height_;
  int32_t dpix_;
  int32_t dpiy_;
  uint32_t gpu_fd_;
  uint32_t power_mode_ = kOn;
  float refresh_;
  uint32_t display_state_ = 0;
  uint32_t hot_plug_display_id_ = 0;
  SpinLock modeset_lock_;
  SpinLock cloned_displays_lock_;
  std::unique_ptr<DisplayQueue> display_queue_;
  std::shared_ptr<HotPlugCallback> hotplug_callback_ = NULL;
  NativeDisplay *source_display_ = NULL;
  std::vector<NativeDisplay *> cloned_displays_;
  std::vector<NativeDisplay *> clones_;
};

}  // namespace hwcomposer
#endif  // WSI_PHYSICALDISPLAY_H_
