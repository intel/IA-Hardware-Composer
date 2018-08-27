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

#ifndef COMMON_COMPOSITOR_VA_VARENDERER_H_
#define COMMON_COMPOSITOR_VA_VARENDERER_H_

#include <map>

#include "hwcdefs.h"
#include "overlaybuffer.h"
#include "renderer.h"

#include "vautils.h"

#include <va/va.h>

namespace hwcomposer {

struct OverlayLayer;
class NativeSurface;

class ScopedVABufferID {
 public:
  ScopedVABufferID(VADisplay display) : display_(display) {
  }
  ~ScopedVABufferID() {
    if (buffer_ != VA_INVALID_ID)
      vaDestroyBuffer(display_, buffer_);
  }

  bool CreateBuffer(VAContextID context, VABufferType type, uint32_t size,
                    uint32_t num, void* data) {
    VAStatus ret =
        vaCreateBuffer(display_, context, type, size, num, data, &buffer_);
    return ret == VA_STATUS_SUCCESS ? true : false;
  }

  operator VABufferID() const {
    return buffer_;
  }

  VABufferID buffer() const {
    return buffer_;
  }

  VABufferID& buffer() {
    return buffer_;
  }

 private:
  VADisplay display_;
  VABufferID buffer_ = VA_INVALID_ID;
};

struct HwcColorBalanceCap {
  VAProcFilterCapColorBalance caps_;
  float value_;
  bool use_default_ = true;
};

struct HwcFilterCap {
  VAProcFilterCap caps_;
  float value_;
  bool use_default_ = true;
};

typedef struct _HwcDeinterlaceCap {
  VAProcFilterCapDeinterlacing caps_[VAProcDeinterlacingCount];
  VAProcDeinterlacingType mode_;
} HwcDeinterlaceCap;

class VARenderer : public Renderer {
 public:
  VARenderer() = default;
  ~VARenderer();

  bool Init(int gpu_fd) override;
  bool Draw(const MediaState &state, NativeSurface *surface) override;
  void InsertFence(int32_t /*kms_fence*/) override {
  }
  void SetExplicitSyncSupport(bool /*disable_explicit_sync*/) override {
  }

  bool DestroyMediaResources(std::vector<struct media_import>&) override;

 private:
  bool QueryVAProcFilterCaps(VAContextID context, VAProcFilterType type,
                             void* caps, uint32_t* num);
  unsigned int GetVAProcFilterScalingMode(uint32_t mode);
  bool SetVAProcFilterColorValue(HWCColorControl type,
                                 const HWCColorProp& prop);
  bool SetVAProcFilterDeinterlaceMode(const HWCDeinterlaceProp& prop,
                                      OverlayBuffer* buffer);
  bool SetVAProcFilterColorDefaultValue(VAProcFilterCapColorBalance* caps);
  bool SetVAProcFilterDeinterlaceDefaultMode();
  bool MapVAProcFilterColorModetoHwc(HWCColorControl& vppmode,
                                     VAProcColorBalanceType vamode);
  bool GetVAProcDeinterlaceFlagFromVideo(const HWCDeinterlaceFlag flag,
                                         OverlayBuffer* buffer);
  bool CreateContext();
  void DestroyContext();
  bool LoadCaps();
  bool UpdateCaps();
#if VA_MAJOR_VERSION >= 1
  void HWCTransformToVA(uint32_t transform, uint32_t& rotation,
                        uint32_t& mirror);
#endif

  bool update_caps_ = false;
  void* va_display_ = nullptr;
  std::vector<VABufferID> filters_;
  std::vector<ScopedVABufferID> cb_elements_;
  std::vector<ScopedVABufferID> sharp_;
  std::vector<ScopedVABufferID> deinterlace_;
  std::map<HWCColorControl, HwcColorBalanceCap> colorbalance_caps_;
  HwcFilterCap sharp_caps_;
  HwcDeinterlaceCap deinterlace_caps_;
  int render_target_format_ = VA_RT_FORMAT_YUV420;
  VAContextID va_context_ = VA_INVALID_ID;
  VAConfigID va_config_ = VA_INVALID_ID;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_VA_VARENDERER_H_
