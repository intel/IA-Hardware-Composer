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

#ifndef PUBLIC_HWCDEFS_H_
#define PUBLIC_HWCDEFS_H_

#include <stdint.h>

#include <hwcrect.h>

#include <vector>
#include <unordered_map>

namespace hwcomposer {

template <typename T>
using HwcRect = Rect<T>;

typedef std::vector<HwcRect<int>> HwcRegion;

enum class HWCBlending : int32_t {
  kBlendingNone = 0x0100,
  kBlendingPremult = 0x0105,
  kBlendingCoverage = 0x0405,
};

enum HWCTransform {
  kIdentity = 0,
  kReflectX = 1 << 0,
  kReflectY = 1 << 1,
  kTransform90 = 1 << 2,
  kTransform180 = 1 << 3,
  kTransform270 = 1 << 4,
  kTransform45 = kTransform90 | kReflectY,
  kTransform135 = kTransform90 | kReflectX,
  kMaxTransform = 8
};

enum HWCRotation {
  kRotateNone = 0,
  kRotate90,
  kRotate180,
  kRotate270,
  kMaxRotate
};

enum HWCLayerType {
  kLayerNormal = 0,
  kLayerCursor = 1,
  kLayerProtected = 2,
  kLayerVideo = 3
};

enum class HWCDisplayAttribute : int32_t {
  kWidth = 1,
  kHeight = 2,
  kRefreshRate = 3,
  kDpiX = 4,
  kDpiY = 5
};

enum class DisplayType : int32_t { kInternal = 0, kExternal = 1, kVirtual = 2, kLogical = 3, kMosaic = 4 };

enum DisplayPowerMode {
  kOff = 0,         // Display is off
  kDoze = 1,        // Display is off and used by the app during any inactivity
                    // when the device is on battery
  kOn = 2,          // Display is on
  kDozeSuspend = 3  // Dispaly in low power mode and stop applying
                    // updates from the client
};

enum HWCColorTransform {
  kIdentical = 0,       // Applies no transform to the output color
  kArbitraryMatrix = 1  // Applies an arbitrary transform defined by a 4x4 affine matrix
};

enum class HWCColorControl : int32_t {
  kColorHue = 0,
  kColorSaturation = 1,
  kColorBrightness = 2,
  kColorContrast = 3
};

struct EnumClassHash {
  template <typename T>
  std::size_t operator()(T t) const {
    return static_cast<std::size_t>(t);
  }
};

using HWCColorMap = std::unordered_map<HWCColorControl, float, EnumClassHash>;

}  // namespace hwcomposer
#endif  // PUBLIC_HWCDEFS_H_
