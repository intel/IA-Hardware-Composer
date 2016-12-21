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

#ifndef HWC_DEFS_H_
#define HWC_DEFS_H_

#include <stdint.h>

#include "separate_rects.h"

namespace hwcomposer {

template <typename T>
using HwcRect = separate_rects::Rect<T>;

enum class HWCBlending : int32_t {
  kBlendingNone = 0x0100,
  kBlendingPremult = 0x0105,
  kBlendingCoverage = 0x0405,
};

enum HWCTransform {
  kIdentity = 0,
  kReflectX = 1 << 0,
  kReflectY = 1 << 1,
  kRotate90 = 1 << 2,
  kRotate180 = 1 << 3,
  kRotate270 = 1 << 4
};

enum HWCLayerType {
  kLayerNormal = 0,
  kLayerCursor = 1 << 1,
  kLayerProtected = 1 << 2,
  kLayerVideo = 1 << 3
};

enum class HWCDisplayAttribute : int32_t {
  kWidth = 1,
  kHeight = 2,
  kRefreshRate = 3,
  kDpiX = 4,
  kDpiY = 5
};

enum class DisplayType : int32_t {
  kInternal = 0,
  kExternal = 1,
  kVirtual = 2,
  kHeadless = 3
};

}  // namespace hardware
#endif  // HWC_DEFS_H_
