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

#ifndef PUBLIC_NATIVEDISPLAY_H_
#define PUBLIC_NATIVEDISPLAY_H_

#include <hwcdefs.h>
#include <platformdefines.h>

#include <stdint.h>

#include <memory>
#include <vector>

typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeModeInfo drmModeModeInfo;

namespace hwcomposer {
struct HwcLayer;
class GpuDevice;
class OverlayBufferManager;

class VsyncCallback {
 public:
  virtual ~VsyncCallback() {
  }
  virtual void Callback(uint32_t display, int64_t timestamp) = 0;
};

class NativeDisplay {
 public:
  virtual ~NativeDisplay() {
  }

  virtual bool Initialize(OverlayBufferManager *buffer_manager) = 0;

  virtual DisplayType Type() const = 0;

  virtual uint32_t Pipe() const = 0;

  virtual uint32_t Width() const = 0;

  virtual uint32_t Height() const = 0;

  virtual int32_t GetRefreshRate() const = 0;

  virtual uint32_t PowerMode() const = 0;

  virtual bool GetDisplayAttribute(uint32_t config,
                                   HWCDisplayAttribute attribute,
                                   int32_t *value) = 0;

  virtual bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) = 0;
  virtual bool GetDisplayName(uint32_t *size, char *name) = 0;
  /**
  * API for getting connected display's pipe id.
  * @return "-1" for unconnected display, valid values are 0 ~ 2.
  */
  virtual int GetDisplayPipe() = 0;
  virtual bool SetActiveConfig(uint32_t config) = 0;
  virtual bool GetActiveConfig(uint32_t *config) = 0;

  virtual bool SetPowerMode(uint32_t power_mode) = 0;

  /**
   * API for showing content on screen.
   * @param source_layers, are the layers to be shown on screen for this frame.
   * @param retire_fence will be populated with Native Fence object provided
   *        we are able to display source_layers. When retire_fence is
   *        signalled source_layers are shown on the output and any previous
   *        frame composition results can be invalidated.
   */
  virtual bool Present(std::vector<HwcLayer *> &source_layers,
                       int32_t *retire_fence) = 0;

  virtual int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                    uint32_t display_id) = 0;
  virtual void VSyncControl(bool enabled) = 0;

  // Color Correction related APIS.
  /**
  * API for setting color gamma value of display in HWC, which be used to remap
  * original color brightness to new one by gamma color correction. HWC uses
  * default gamma value 2.2 which popular display is using, and allow users to
  * change gamma value for RGB colors by this API, e.g. 0 will remap all
  * gradient brightness of the color to brightest value (solid color).
  *
  * @param red red color gamma value
  * @param green blue color gamma value
  * @param blue blue color gamma value
  */
  virtual void SetGamma(float /*red*/, float /*green*/, float /*blue*/) {
  }
  /**
  * API for setting display color contrast in HWC
  * @param red valid value is 0 ~ 255, bigger value with stronger contrast
  * @param green valid value is 0 ~ 255, bigger value with stronger contrast
  * @param blue valid value is 0 ~ 255, bigger value with stronger contrast
  */
  virtual void SetContrast(uint32_t /*red*/, uint32_t /*green*/,
                           uint32_t /*blue*/) {
  }
  /**
  * API for setting display color brightness in HWC
  * @param red valid value is 0 ~ 255, bigger value with stronger brightness
  * @param green valid value is 0 ~ 255, bigger value with stronger brightness
  * @param blue valid value is 0 ~ 255, bigger value with stronger brightness
  */
  virtual void SetBrightness(uint32_t /*red*/, uint32_t /*green*/,
                             uint32_t /*blue*/) {
  }
  /**
  * API for setting display Broadcast RGB range property
  * @param range_property supported property string, e.g. "Full", "Automatic"
  */
  virtual bool SetBroadcastRGB(const char * /*range_property*/) {
    return false;
  }

  // Virtual display related.
  virtual void InitVirtualDisplay(uint32_t /*width*/, uint32_t /*height*/) {
  }
  virtual void SetOutputBuffer(HWCNativeHandle /*buffer*/,
                               int32_t /*acquire_fence*/) {
  }
  /**
  * API to check the format support on the device
  * @param format valid DRM formats found in drm_fourcc.h.
  */
  virtual bool CheckPlaneFormat(uint32_t format) = 0;

  virtual void SetExplicitSyncSupport(bool /*explicit_sync_enabled*/) {
  }
 protected:
  virtual uint32_t CrtcId() const = 0;
  virtual bool Connect(const drmModeModeInfo &mode_info,
                       const drmModeConnector *connector) = 0;

  virtual bool IsConnected() const = 0;

  virtual void DisConnect() = 0;

  virtual void ShutDown() = 0;

  friend class GpuDevice;
};
}  // namespace hwcomposer
#endif  // PUBLIC_NATIVEDISPLAY_H_
