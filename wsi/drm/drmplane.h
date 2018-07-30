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

#ifndef WSI_DRMPLANE_H_
#define WSI_DRMPLANE_H_

#include <stdint.h>
#include <stdlib.h>

#include <xf86drmMode.h>

#include <drmscopedtypes.h>

#include <vector>

#include "displayplane.h"
#include "drmbuffer.h"

namespace hwcomposer {

class GpuDevice;
struct OverlayLayer;

class DrmPlane : public DisplayPlane {
 public:
  DrmPlane(uint32_t plane_id, uint32_t possible_crtcs);

  ~DrmPlane();

  bool Initialize(uint32_t gpu_fd, const std::vector<uint32_t>& formats,
                  bool use_modifer);

  bool UpdateProperties(drmModeAtomicReqPtr property_set, uint32_t crtc_id,
                        const OverlayLayer* layer,
                        bool test_commit = false) const;

  void SetNativeFence(int32_t fd);

  void SetBuffer(std::shared_ptr<OverlayBuffer>& buffer);

  bool Disable(drmModeAtomicReqPtr property_set);

  bool GetCrtcSupported(uint32_t pipe_id) const;

  uint32_t type() const;

  uint32_t id() const override;

  bool ValidateLayer(const OverlayLayer* layer) override;

  bool IsSupportedFormat(uint32_t format) override;

  bool IsSupportedTransform(uint32_t transform) const override;

  uint32_t GetPreferredVideoFormat() const override;
  uint32_t GetPreferredFormat() const override;
  uint64_t GetPreferredFormatModifier() const override;

  void BlackListPreferredFormatModifier() override;

  void PreferredFormatModifierValidated() override;

  void Dump() const override;

  void SetInUse(bool in_use) override;

  bool InUse() const override {
    return in_use_;
  }

  bool IsUniversal() override {
    return !(type_ == DRM_PLANE_TYPE_CURSOR);
  }

  // check if modifier is supported for given format
  bool IsSupportedModifier(uint64_t modifier, uint32_t format);

 private:
  struct Property {
    Property();
    bool Initialize(uint32_t fd, const char* name,
                    const ScopedDrmObjectPropertyPtr& plane_properties,
                    uint32_t* rotation = NULL,
                    uint64_t* in_formats_prop_value = NULL);
    uint32_t id = 0;
  };

  Property crtc_prop_;
  Property fb_prop_;
  Property crtc_x_prop_;
  Property crtc_y_prop_;
  Property crtc_w_prop_;
  Property crtc_h_prop_;
  Property src_x_prop_;
  Property src_y_prop_;
  Property src_w_prop_;
  Property src_h_prop_;
  Property rotation_prop_;
  Property alpha_prop_;
  Property in_fence_fd_prop_;
  Property in_formats_prop_;

  uint32_t id_;

  uint32_t possible_crtc_mask_;

  uint32_t type_;

  uint32_t last_valid_format_;
  bool in_use_;
  bool prefered_modifier_succeeded_ = false;

  std::vector<uint32_t> supported_formats_;
  int32_t kms_fence_ = 0;
  uint32_t prefered_video_format_ = 0;
  uint32_t prefered_format_ = 0;
  uint64_t prefered_modifier_ = 0;
  uint32_t rotation_ = 0;

  // keep supported modifiers for each supported format
  typedef struct format_mods {
    std::vector<uint64_t> mods;
    uint32_t format;
  } format_mods;
  std::vector<format_mods> formats_modifiers_;
  std::shared_ptr<OverlayBuffer> buffer_ = NULL;
  bool use_modifier_ = true;
};

}  // namespace hwcomposer
#endif  // WSI_DRMPLANE_H_
