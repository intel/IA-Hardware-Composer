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
#include "renderer.h"
#include "va/va.h"
#include "va/va_vpp.h"

namespace hwcomposer {

struct OverlayLayer;
class NativeSurface;

typedef enum _VppColorBalanceMode {
  COLORBALANCE_NONE = 0,
  COLORBALANCE_HUE,
  COLORBALANCE_SATURATION,
  COLORBALANCE_BRIGHTNESS,
  COLORBALANCE_CONTRAST,
  COLORBALANCE_CONUNT,
} VppColorBalanceMode;

typedef struct _VppColorBalanceCap {
  VAProcFilterCapColorBalance caps;
  float value;
} VppColorBalanceCap;

typedef std::map<VppColorBalanceMode, VppColorBalanceCap> ColorBalanceCapMap;
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
  bool QueryVAProcFilterCaps(VAContextID context, VAProcFilterType type,
                             void* caps, uint32_t* num);
  bool SetVAProcFilterValue(VppColorBalanceMode type, float value);
  bool SetVAProcFilterDefaultValue(VAProcFilterCapColorBalance* caps);
 private:
  int DrmFormatToVAFormat(int format);
  int DrmFormatToRTFormat(int format);
  bool MapVAProcFilterModetoVpp(VppColorBalanceMode& vppmode,
                                VAProcColorBalanceType vamode);
  void *va_display_ = nullptr;
  ColorBalanceCapMap caps_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_VA_VARENDERER_H_
