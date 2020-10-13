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
class NativeBufferHandler;
class FrameBufferManager;

class VsyncCallback {
 public:
  virtual ~VsyncCallback() {
  }
  virtual void Callback(uint32_t display, int64_t timestamp) = 0;
};

class VsyncPeriodCallback {
 public:
  virtual ~VsyncPeriodCallback() {
  }
  virtual void Callback(uint32_t display, int64_t timestamp,
                        uint32_t vsyncPeriodNanos) = 0;
};

class RefreshCallback {
 public:
  virtual ~RefreshCallback() {
  }
  virtual void Callback(uint32_t display) = 0;
};

class HotPlugCallback {
 public:
  virtual ~HotPlugCallback() {
  }
  virtual void Callback(uint32_t display, bool connected) = 0;
};

class PixelUploaderCallback {
 public:
  virtual ~PixelUploaderCallback() {
  }
  virtual void Synchronize() = 0;
};

class NativeDisplay {
 public:
  virtual ~NativeDisplay() {
  }

  NativeDisplay() = default;

  NativeDisplay(const NativeDisplay &rhs) = delete;
  NativeDisplay &operator=(const NativeDisplay &rhs) = delete;

  virtual bool Initialize(NativeBufferHandler *buffer_handler) = 0;

  virtual DisplayType Type() const = 0;

  virtual uint32_t Width() const = 0;

  virtual uint32_t Height() const = 0;

  virtual uint32_t PowerMode() const = 0;

  virtual bool GetDisplayAttribute(uint32_t config,
                                   HWCDisplayAttribute attribute,
                                   int32_t *value) = 0;

  virtual bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) = 0;
  virtual bool GetDisplayName(uint32_t *size, char *name) = 0;

  virtual bool GetDisplayIdentificationData(uint8_t *outPort,
                                            uint32_t *outDataSize,
                                            uint8_t *outData) = 0;

  virtual void GetDisplayCapabilities(uint32_t *outNumCapabilities,
                                      uint32_t *outCapabilities) = 0;
  virtual bool GetDisplayVsyncPeriod(uint32_t *outVsyncPeriod) {
    return false;
  };

  /**
   * API for getting connected display's pipe id.
   * @return "-1" for unconnected display, valid values are 0 ~ 2.
   */
  virtual int GetDisplayPipe() = 0;
  virtual bool SetActiveConfig(uint32_t config) = 0;
  virtual bool GetActiveConfig(uint32_t *config) = 0;

  /**
   * API for setting custom resolution. By default the resolution is set as per
   * the display hardware capability. This API sets the resolution from
   * the configuration file.
   * @param rectangle as per the resolution required
   * @return true if set passes, false otherwise
   */
  virtual bool SetCustomResolution(const HwcRect<int32_t> &) {
    return false;
  }

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
                       int32_t *retire_fence,
                       PixelUploaderCallback *call_back = NULL,
                       bool handle_constraints = false) = 0;

  virtual int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                    uint32_t display_id) = 0;

  virtual int RegisterVsyncPeriodCallback(
      std::shared_ptr<VsyncPeriodCallback> callback, uint32_t display_id) {
    return false;
  };

  virtual void VSyncControl(bool enabled) = 0;

  /**
   * API for registering for refresh callback requests.
   * @param callback, function which will be used by HWC to request client to
   *        trigger a screen refresh. This will be used to optimize scenarios
   *        like idle mode.
   * @param display_id will be populated with id of the display.
   */
  virtual void RegisterRefreshCallback(
      std::shared_ptr<RefreshCallback> /*callback*/, uint32_t /*display_id*/) {
  }

  /**
   * API for registering for hotplug callback requests.
   * @param callback, function which will be used by HWC to notify client of
   *        a hot plug event.
   * @param display_id will be populated with id of the display.
   */
  virtual void RegisterHotPlugCallback(
      std::shared_ptr<HotPlugCallback> /*callback*/, uint32_t /*display_id*/) {
  }

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
   * API for setting a color transform which will be applied after composition.
   *
   * The matrix provided is an affine color transformation of the following
   * form:
   *
   * |r.r r.g r.b 0|
   * |g.r g.g g.b 0|
   * |b.r b.g b.b 0|
   * |Tr  Tg  Tb  1|
   *
   * This matrix will be provided in row-major form: {r.r, r.g, r.b, 0, g.r,
   * ...}.
   *
   * Given a matrix of this form and an input color [R_in, G_in, B_in], the
   * output
   * color [R_out, G_out, B_out] will be:
   *
   * R_out = R_in * r.r + G_in * g.r + B_in * b.r + Tr
   * G_out = R_in * r.g + G_in * g.g + B_in * b.g + Tg
   * B_out = R_in * r.b + G_in * g.b + B_in * b.b + Tb
   *
   * @param matrix a 4x4 transform matrix (16 floats) as described above
   * @param hint a hint value to specify the transform type, applying no
   * transform
   *        or applying transform defined by given matrix
   */
  virtual void SetColorTransform(const float * /*matrix*/,
                                 HWCColorTransform /*hint*/) {
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
   * API for setting video color in HWC
   */
  virtual void SetVideoColor(HWCColorControl /*color*/, float /*value*/) {
  }

  /**
   * API for getting video color in HWC
   */
  virtual void GetVideoColor(HWCColorControl /*color*/, float * /*value*/,
                             float * /*start*/, float * /*end*/) {
  }

  /**
   * API for restoring video default color in HWC
   */
  virtual void RestoreVideoDefaultColor(HWCColorControl /*color*/) {
  }

  /**
   * API for setting video scaling mode in HWC
   */
  virtual void SetVideoScalingMode(uint32_t /*mode*/) {
  }

  /**
   * API for setting video deinterlace in HWC
   */
  virtual void SetVideoDeinterlace(HWCDeinterlaceFlag /*flags*/,
                                   HWCDeinterlaceControl /*mode*/) {
  }

  /**
   * API for restoring video default deinterlace in HWC
   */
  virtual void RestoreVideoDefaultDeinterlace() {
  }

  /**
   * API for setting display Broadcast RGB range property
   * @param range_property supported property string, e.g. "Full", "Automatic"
   */
  virtual bool SetBroadcastRGB(const char * /*range_property*/) {
    return false;
  }

  /**
   * API for setting the color of the pipe canvas.
   * The caller of this function must pass the individual color component
   * values which will all be used to convert into a format expected by
   * the kernel driver.
   *
   * @param bpc specifies how many bits are used per color componenet
   * @param red bits representing the red color componenet
   * @param green bits representing the green color componenet
   * @param blue bits representing the blue color componenet
   * @param alpha bits representing the alpha color componenet
   */
  virtual void SetCanvasColor(uint16_t /*bpc*/, uint16_t /*red*/,
                              uint16_t /*green*/, uint16_t /*blue*/,
                              uint16_t /*alpha*/) {
  }

  // Virtual display related.
  virtual void InitVirtualDisplay(uint32_t /*width*/, uint32_t /*height*/) {
  }

  /**
   * API for setting output buffer for virtual display.
   * @param buffer ownership is taken by display.
   */
  virtual void SetOutputBuffer(HWCNativeHandle /*buffer*/,
                               int32_t /*acquire_fence*/) {
  }
  /**
   * API to check the format support on the device
   * @param format valid DRM formats found in drm_fourcc.h.
   */
  virtual bool CheckPlaneFormat(uint32_t format) = 0;

  virtual void SetDisableExplicitSync(bool /*explicit_sync_enabled*/) {
  }

  /**
   * API to connect the display. Note that this doesn't necessarily
   * mean display is turned on. Implementation is free to reset any display
   * state which they seem appropriate for this state. Any subsequent calls
   * to Present after this call will show content on screen provided
   * Powermode is kon.
   */
  virtual void Connect() {
  }

  /**
   * API to check if display is connected.
   */
  virtual bool IsConnected() const {
    return false;
  }

  /* Returns capability to bypass client-enabled CTM for this display */
  virtual bool IsBypassClientCTM() const {
    return false;
  }

  /**
   * Scales layers of display to match it's resolutions in case
   * this display is in cloned mode and resolution doesn't match
   * with Source Display.
   */
  virtual void UpdateScalingRatio(uint32_t /*primary_width*/,
                                  uint32_t /*primary_height*/,
                                  uint32_t /*display_width*/,
                                  uint32_t /*display_height*/) {
  }

  /**
   * This display needs to clone source_display.
   * We cannot have a display in cloned mode and extended mode at
   * same time or clone more than one source_display at same time.
   */
  virtual void CloneDisplay(NativeDisplay * /*source_display*/) {
  }

  virtual uint32_t GetXTranslation() {
    return 0;
  }

  /**
   * This is to position the composition to non-zero coordinates in display.
   * When using float mode, the composition resolution is custom configured
   * and it can be positioned as required.
   */
  virtual uint32_t GetYTranslation() {
    return 0;
  }

  virtual uint32_t GetLogicalIndex() const {
    return 0;
  }

  virtual void HotPlugUpdate(bool /*connected*/) {
  }

  /**
   * Use this method to initalize the display's pool of
   * layer ids. The argument to the method is the
   * initial size of the pool
   */
  virtual int InitializeLayerHashGenerator(int size) {
    LayerIds_.clear();
    for (int i = size; i >= 0; i--) {
      LayerIds_.push_back(i);
    }

    current_max_layer_ids_ = size;
    return 0;
  }

  /**
   * Once the id pool is initialzed, use this to acquire an
   * unused id for the layer.
   */
  virtual uint64_t AcquireId() {
    if (LayerIds_.empty())
      return ++current_max_layer_ids_;

    uint64_t id = LayerIds_.back();
    LayerIds_.pop_back();

    return id;
  }

  /**
   * Method to return a destroyed layer's id back into the pool
   */
  virtual void ReleaseId(uint64_t id) {
    LayerIds_.push_back(id);
  }

  /**
   * Call this to reset the id pool back to its initial state.
   */
  virtual void ResetLayerHashGenerator() {
    InitializeLayerHashGenerator(current_max_layer_ids_);
  }

  /**
   * Call this to set HDCP state for this display. This function
   * tries to take advantage of any HDCP support advertised by
   * the Kernel.
   */
  virtual void SetHDCPState(HWCContentProtection /*state*/,
                            HWCContentType /*content_type*/) {
  }

  virtual void SetPAVPSessionStatus(bool enabled, uint32_t pavp_session_id,
                                    uint32_t pavp_instance_id) {
  }

  virtual void SetHDCPSRM(const int8_t *SRM, uint32_t SRMLength) {
  }

  virtual bool GetDCIP3Support() {
    return false;
  }

  virtual const NativeBufferHandler *GetNativeBufferHandler() const {
    return NULL;
  }

  // return true if connector_id is one of the connector_ids of the physical
  // connections
  virtual bool ContainConnector(const uint32_t connector_id) {
    return false;
  }

  virtual bool EnableDRMCommit(bool enable) {
    return false;
  }

  virtual void MarkFirstCommit() {
  }

  virtual int GetTotalOverlays() const {
    return 0;
  }

 protected:
  friend class PhysicalDisplay;
  friend class GpuDevice;
  virtual void OwnPresentation(NativeDisplay * /*clone*/) {
  }

  virtual void DisOwnPresentation(NativeDisplay * /*clone*/) {
  }

  virtual bool PresentClone(NativeDisplay * /*display*/) {
    return false;
  }

  // Physical pipe order might be different to the display order
  // configured in hwcdisplay.ini. We need to always use any values
  // overridden by SetDisplayOrder to determine if a display is
  // primary or not.
  virtual void SetDisplayOrder(uint32_t /*display_order*/) {
  }

  // Rotates content shown by this diplay as specified by
  // rotation. This is on top of any transformations applied
  // to individual layers shown by this display.
  virtual void RotateDisplay(HWCRotation /*rotation*/) {
  }

 private:
  std::vector<uint64_t> LayerIds_;
  uint64_t current_max_layer_ids_ = 0;
};

/**
 * This is provided for Convenience in case
 * one doesnt want to register for hot plug
 * callback per display.
 */
class DisplayHotPlugEventCallback {
 public:
  virtual ~DisplayHotPlugEventCallback() {
  }
  virtual void Callback(std::vector<NativeDisplay *> connected_displays) = 0;
};

}  // namespace hwcomposer
#endif  // PUBLIC_NATIVEDISPLAY_H_
