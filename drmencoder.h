/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_DRM_ENCODER_H_
#define ANDROID_DRM_ENCODER_H_

#include "drmcrtc.h"

#include <stdint.h>
#include <xf86drmMode.h>
#include <set>
#include <vector>

namespace android {

class DrmEncoder {
 public:
  DrmEncoder(drmModeEncoderPtr e, DrmCrtc *current_crtc,
             const std::vector<DrmCrtc *> &possible_crtcs);
  DrmEncoder(const DrmEncoder &) = delete;
  DrmEncoder &operator=(const DrmEncoder &) = delete;

  uint32_t id() const;

  DrmCrtc *crtc() const;
  void set_crtc(DrmCrtc *crtc);
  bool can_bind(int display) const;
  int display() const;

  const std::vector<DrmCrtc *> &possible_crtcs() const {
    return possible_crtcs_;
  }
  bool CanClone(DrmEncoder *encoder);
  void AddPossibleClone(DrmEncoder *possible_clone);

 private:
  uint32_t id_;
  DrmCrtc *crtc_;
  int display_;

  std::vector<DrmCrtc *> possible_crtcs_;
  std::set<DrmEncoder *> possible_clones_;
};
}  // namespace android

#endif  // ANDROID_DRM_ENCODER_H_
