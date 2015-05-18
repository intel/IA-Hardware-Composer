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

#ifndef ANDROID_DRM_CRTC_H_
#define ANDROID_DRM_CRTC_H_

#include "drmmode.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

class DrmCrtc {
 public:
  DrmCrtc(drmModeCrtcPtr c, unsigned pipe);
  ~DrmCrtc();

  uint32_t id() const;
  unsigned pipe() const;

  bool requires_modeset() const;
  void set_requires_modeset(bool requires_modeset);

  int display() const;
  void set_display(int display);

  bool can_bind(int display) const;

 private:
  DrmCrtc(const DrmCrtc &);

  uint32_t id_;
  unsigned pipe_;
  int display_;

  bool requires_modeset_;

  uint32_t x_;
  uint32_t y_;
  uint32_t width_;
  uint32_t height_;

  DrmMode mode_;
  bool mode_valid_;
};
}

#endif  // ANDROID_DRM_CRTC_H_
