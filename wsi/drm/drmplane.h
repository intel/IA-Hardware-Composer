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

#include <stdlib.h>
#include <stdint.h>

#include <xf86drmMode.h>

#include <drmscopedtypes.h>

#include <vector>

#include "displayplane.h"

namespace hwcomposer {

class GpuDevice;
struct OverlayLayer;

class DrmPlane : public DisplayPlane {
 public:
  DrmPlane(uint32_t plane_id, uint32_t possible_crtcs);

  ~DrmPlane();

  bool Initialize(uint32_t gpu_fd, const std::vector<uint32_t>& formats);

  bool UpdateProperties(drmModeAtomicReqPtr property_set, uint32_t crtc_id,
                        const OverlayLayer* layer,
                        bool test_commit = false) const;

  void SetNativeFence(int32_t fd);

  bool Disable(drmModeAtomicReqPtr property_set);

  bool GetCrtcSupported(uint32_t pipe_id) const;

  uint32_t type() const;

  uint32_t id() const override;
  void SetEnabled(bool enabled) override;

  bool IsEnabled() const override {
    return enabled_;
  }

  bool ValidateLayer(const OverlayLayer* layer) override;

  bool IsSupportedFormat(uint32_t format) override;

  uint32_t GetFormatForFrameBuffer(uint32_t format) override;
  uint32_t GetPreferredVideoFormat() const override;

  void Dump() const override;

 private:
  struct Property {
    Property();
    bool Initialize(uint32_t fd, const char* name,
                    const ScopedDrmObjectPropertyPtr& plane_properties);
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

  uint32_t id_;

  uint32_t possible_crtc_mask_;

  uint32_t type_;

  uint32_t last_valid_format_;

  bool enabled_;

  std::vector<uint32_t> supported_formats_;
  int32_t kms_fence_ = 0;
  uint32_t prefered_video_format_ = 0;
};

}  // namespace hwcomposer
#endif  // WSI_DRMPLANE_H_
