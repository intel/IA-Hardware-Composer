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

#ifndef WSI_DRMDISPLAY_H_
#define WSI_DRMDISPLAY_H_

#include <stdint.h>
#include <stdlib.h>
#include <xf86drmMode.h>

#include <drmscopedtypes.h>

#include "drmplane.h"
#include "physicaldisplay.h"

#ifndef DRM_RGBA8888
#define DRM_RGBA8888(r, g, b, a) DrmRGBA(8, r, g, b, a)
#define DRM_RGBA16161616(r, g, b, a) DrmRGBA(16, r, g, b, a)
#endif

namespace hwcomposer {

enum pipe_bpc {
  PIPE_BPC_INVALID = -1,
  PIPE_BPC_SIX = 6,
  PIPE_BPC_EIGHT = 8,
  PIPE_BPC_TEN = 10,
  PIPE_BPC_TWELVE = 12,
  PIPE_BPC_SIXTEEN = 16
};

class DrmDisplayManager;
class DisplayPlaneState;
class DisplayQueue;
class NativeBufferHandler;
class GpuDevice;
struct HwcLayer;

class DrmDisplay : public PhysicalDisplay {
 public:
  DrmDisplay(uint32_t gpu_fd, uint32_t pipe_id, uint32_t crtc_id,
             DrmDisplayManager *manager);
  ~DrmDisplay() override;

  bool GetDisplayAttribute(uint32_t config, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;

  bool SetBroadcastRGB(const char *range_property) override;

  void SetHDCPState(HWCContentProtection state,
                    HWCContentType content_type) override;
  void SetHDCPSRM(const int8_t *SRM, uint32_t SRMLength) override;

  bool ContainConnector(const uint32_t connector_id) override;

  bool InitializeDisplay() override;
  void PowerOn() override;
  void UpdateDisplayConfig() override;
  void SetColorCorrection(struct gamma_colors gamma, uint32_t contrast,
                          uint32_t brightness) const override;
  void SetPipeCanvasColor(uint16_t bpc, uint16_t red, uint16_t green,
                          uint16_t blue, uint16_t alpha) const override;
  bool SetPipeMaxBpc(uint16_t max_bpc) const override;
  void SetColorTransformMatrix(
      const float *color_transform_matrix,
      HWCColorTransform color_transform_hint) const override;
  void Disable(const DisplayPlaneStateList &composition_planes) override;
  bool Commit(const DisplayPlaneStateList &composition_planes,
              const DisplayPlaneStateList &previous_composition_planes,
              bool disable_explicit_fence, int32_t previous_fence,
              int32_t *commit_fence, bool *previous_fence_released) override;

  uint32_t CrtcId() const {
    return crtc_id_;
  }

  bool ConnectDisplay(const drmModeModeInfo &mode_info,
                      const drmModeConnector *connector, uint32_t config);

  void SetDrmModeInfo(const std::vector<drmModeModeInfo> &mode_info);
  void SetDisplayAttribute(const drmModeModeInfo &mode_info);
  void SetFakeAttribute(const drmModeModeInfo &mode_info);

  bool TestCommit(const DisplayPlaneStateList &commit_planes) const override;

  bool PopulatePlanes(
      std::vector<std::unique_ptr<DisplayPlane>> &overlay_planes) override;

  void NotifyClientsOfDisplayChangeStatus() override;

  void ForceRefresh();

  bool GetDCIP3Support() {
    return dcip3_;
  }

  void IgnoreUpdates();

  uint32_t GetConnectorID() {
    return connector_;
  }

  void ReleaseUnreservedPlanes(std::vector<uint32_t> &reserved_planes);

  void HandleLazyInitialization() override;

  void SetPlanesUpdated(bool updated) {
    planes_updated_ = updated;
  }

  bool IsPlanesUpdated() {
    return planes_updated_;
  }

  void MarkFirstCommit() override {
    first_commit_ = true;
  }

 private:
  void ShutDownPipe();
  void GetDrmObjectPropertyValue(const char *name,
                                 const ScopedDrmObjectPropertyPtr &props,
                                 uint64_t *value) const;
  void GetDrmObjectProperty(const char *name,
                            const ScopedDrmObjectPropertyPtr &props,
                            uint32_t *id) const;
  void GetDrmHDCPObjectProperty(const char *name,
                                const drmModeConnector *connector,
                                const ScopedDrmObjectPropertyPtr &props,
                                uint32_t *id, int *value = NULL) const;
  float TransformGamma(float value, float gamma) const;
  float TransformContrastBrightness(float value, float brightness,
                                    float contrast) const;
  int64_t FloatToFixedPoint(float value) const;
  void ApplyPendingCTM(struct drm_color_ctm *ctm,
                       struct drm_color_ctm_post_offset *ctm_post_offset) const;
  void ApplyPendingLUT(struct drm_color_lut *lut) const;
  bool ApplyPendingModeset(drmModeAtomicReqPtr property_set);
  bool GetFence(drmModeAtomicReqPtr property_set, int32_t *out_fence);
  bool CommitFrame(const DisplayPlaneStateList &comp_planes,
                   const DisplayPlaneStateList &previous_composition_planes,
                   drmModeAtomicReqPtr pset, uint32_t flags,
                   int32_t previous_fence, bool *previous_fence_released);
  uint64_t DrmRGBA(uint16_t, uint16_t red, uint16_t green, uint16_t blue,
                   uint16_t alpha) const;
  std::unique_ptr<DrmPlane> CreatePlane(uint32_t plane_id,
                                        uint32_t possible_crtcs);
  std::vector<uint8_t *> FindExtendedBlocksForTag(uint8_t *edid,
                                                  uint8_t block_tag);
  void DrmConnectorGetDCIP3Support(const ScopedDrmObjectPropertyPtr &props);

  void TraceFirstCommit();

  uint32_t FindPreferedDisplayMode(size_t modes_size);
  uint32_t FindPerformaceDisplayMode(size_t modes_size);

  uint32_t crtc_id_ = 0;
  uint32_t mmWidth_ = 0;
  uint32_t mmHeight_ = 0;
  uint32_t out_fence_ptr_prop_ = 0;
  uint32_t dpms_prop_ = 0;
  uint32_t ctm_id_prop_ = 0;
  uint32_t ctm_post_offset_id_prop_ = 0;
  uint32_t lut_id_prop_ = 0;
  uint32_t crtc_prop_ = 0;
  uint32_t broadcastrgb_id_ = 0;
  uint32_t blob_id_ = 0;
  uint32_t old_blob_id_ = 0;
  uint32_t active_prop_ = 0;
  uint32_t mode_id_prop_ = 0;
  uint32_t hdcp_id_prop_ = 0;
  uint32_t hdcp_srm_id_prop_ = 0;
  uint32_t edid_prop_ = 0;
  uint32_t canvas_color_prop_ = 0;
  uint32_t connector_ = 0;
  bool dcip3_ = false;
  uint32_t max_bpc_prop_ = 0;
  uint64_t lut_size_ = 0;
  int64_t broadcastrgb_full_ = -1;
  int64_t broadcastrgb_automatic_ = -1;
  uint32_t flags_ = DRM_MODE_ATOMIC_ALLOW_MODESET;
  bool planes_updated_ = false;
  bool first_commit_ = false;
  uint32_t prefer_display_mode_ = 0;
  uint32_t perf_display_mode_ = 0;
  std::string display_name_ = "";
  HWCContentProtection current_protection_support_ =
      HWCContentProtection::kUnSupported;
  HWCContentProtection desired_protection_support_ =
      HWCContentProtection::kUnSupported;
  drmModeModeInfo current_mode_;
  HWCContentType content_type_ = kCONTENT_TYPE0;
  std::vector<drmModeModeInfo> modes_;
  SpinLock display_lock_;
  DrmDisplayManager *manager_;
};

}  // namespace hwcomposer
#endif  // WSI_DRMDISPLAY_H_
