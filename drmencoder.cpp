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

#include "drmencoder.h"
#include "drmcrtc.h"
#include "drmdevice.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

DrmEncoder::DrmEncoder(drmModeEncoderPtr e, DrmCrtc *current_crtc,
                       const std::vector<DrmCrtc *> &possible_crtcs)
    : id_(e->encoder_id),
      crtc_(current_crtc),
      display_(-1),
      possible_crtcs_(possible_crtcs) {
}

uint32_t DrmEncoder::id() const {
  return id_;
}

DrmCrtc *DrmEncoder::crtc() const {
  return crtc_;
}

bool DrmEncoder::CanClone(DrmEncoder *possible_clone) {
  return possible_clones_.find(possible_clone) != possible_clones_.end();
}

void DrmEncoder::AddPossibleClone(DrmEncoder *possible_clone) {
  possible_clones_.insert(possible_clone);
}

void DrmEncoder::set_crtc(DrmCrtc *crtc) {
  crtc_ = crtc;
  display_ = crtc->display();
}

int DrmEncoder::display() const {
  return display_;
}

bool DrmEncoder::can_bind(int display) const {
  return display_ == -1 || display_ == display;
}
}  // namespace android
