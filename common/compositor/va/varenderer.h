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

#include <va/va.h>
#include <va/va_vpp.h>
#include <map>

#include "hwcdefs.h"
#include "renderer.h"

namespace hwcomposer {

struct OverlayLayer;
class NativeSurface;

typedef struct _VppColorBalanceCap {
  VAProcFilterCapColorBalance caps;
  float value;
} VppColorBalanceCap;

typedef std::map<HWCColorControl, VppColorBalanceCap> ColorBalanceCapMap;
typedef ColorBalanceCapMap::iterator ColorBalanceCapMapItr;

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

 private:
  int DrmFormatToVAFormat(int format);
  int DrmFormatToRTFormat(int format);
  bool QueryVAProcFilterCaps(VAContextID context, VAProcFilterType type,
                             void* caps, uint32_t* num);
  bool SetVAProcFilterColorValue(HWCColorControl type, float value);
  bool SetVAProcFilterColorDefaultValue(VAProcFilterCapColorBalance* caps);
  bool MapVAProcFilterColorModetoHwc(HWCColorControl& vppmode,
                                     VAProcColorBalanceType vamode);
  bool CreateContext();
  void DestroyContext();
  uint32_t HWCTransformToVA(uint32_t transform);

  void* va_display_ = nullptr;
  ColorBalanceCapMap caps_;
  int render_target_format_ = VA_RT_FORMAT_YUV420;
  VAContextID va_context_ = VA_INVALID_ID;
  VAConfigID va_config_ = VA_INVALID_ID;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_VA_VARENDERER_H_
