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

#ifndef ANDROID_DRM_CONNECTOR_H_
#define ANDROID_DRM_CONNECTOR_H_

#include "drmencoder.h"
#include "drmmode.h"
#include "drmproperty.h"

#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>

namespace android {

class DrmDevice;

class DrmConnector {
 public:
  DrmConnector(DrmDevice *drm, drmModeConnectorPtr c,
               DrmEncoder *current_encoder,
               std::vector<DrmEncoder *> &possible_encoders);
  DrmConnector(const DrmProperty &) = delete;
  DrmConnector &operator=(const DrmProperty &) = delete;

  int Init();

  uint32_t id() const;

  int display() const;
  void set_display(int display);

  bool internal() const;
  bool external() const;
  bool writeback() const;
  bool valid_type() const;

  int UpdateModes();

  const std::vector<DrmMode> &modes() const {
    return modes_;
  }
  const DrmMode &active_mode() const;
  void set_active_mode(const DrmMode &mode);

  const DrmProperty &dpms_property() const;
  const DrmProperty &crtc_id_property() const;
  const DrmProperty &writeback_pixel_formats() const;
  const DrmProperty &writeback_fb_id() const;
  const DrmProperty &writeback_out_fence() const;

  const std::vector<DrmEncoder *> &possible_encoders() const {
    return possible_encoders_;
  }
  DrmEncoder *encoder() const;
  void set_encoder(DrmEncoder *encoder);

  drmModeConnection state() const;

  uint32_t mm_width() const;
  uint32_t mm_height() const;

  uint32_t get_preferred_mode_id() const {
    return preferred_mode_id_;
  }

 private:
  DrmDevice *drm_;

  uint32_t id_;
  DrmEncoder *encoder_;
  int display_;

  uint32_t type_;
  drmModeConnection state_;

  uint32_t mm_width_;
  uint32_t mm_height_;

  DrmMode active_mode_;
  std::vector<DrmMode> modes_;

  DrmProperty dpms_property_;
  DrmProperty crtc_id_property_;
  DrmProperty writeback_pixel_formats_;
  DrmProperty writeback_fb_id_;
  DrmProperty writeback_out_fence_;

  std::vector<DrmEncoder *> possible_encoders_;

  uint32_t preferred_mode_id_;
};
}  // namespace android

#endif  // ANDROID_DRM_PLANE_H_
