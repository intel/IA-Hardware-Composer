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

#ifndef PUBLIC_HWCMETA_H_
#define PUBLIC_HWCMETA_H_

#include <stdint.h>
#include <unistd.h>

#include <hwcdefs.h>

struct HwcMeta {
  HwcMeta() = default;

  HwcMeta &operator=(const HwcMeta &rhs) = delete;
  bool is_interlaced_ = false;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t format_ = 0;  // Drm format equivalent to native_format.
  uint32_t tiling_mode_ = 0;
  uint32_t native_format_ = 0;  // OS specific format.
  uint32_t pitches_[4];
  uint32_t offsets_[4];
  uint32_t gem_handles_[4];
  int prime_fds_[4];
  uint32_t num_planes_ = 0;
  uint32_t fb_modifiers_[8];
  hwcomposer::HWCLayerType usage_ = hwcomposer::kLayerNormal;
};

#endif  // PUBLIC_HWCMETA_H_
