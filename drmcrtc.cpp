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

#include "drmcrtc.h"
#include "drmresources.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

DrmCrtc::DrmCrtc(drmModeCrtcPtr c, unsigned pipe)
    : id_(c->crtc_id),
      pipe_(pipe),
      display_(-1),
      requires_modeset_(true),
      x_(c->x),
      y_(c->y),
      width_(c->width),
      height_(c->height),
      mode_(&c->mode),
      modeValid_(c->mode_valid) {
}

DrmCrtc::~DrmCrtc() {
}

uint32_t DrmCrtc::id() const {
  return id_;
}

unsigned DrmCrtc::pipe() const {
  return pipe_;
}

bool DrmCrtc::requires_modeset() const {
  return requires_modeset_;
}

void DrmCrtc::set_requires_modeset(bool requires_modeset) {
  requires_modeset_ = requires_modeset;
}

int DrmCrtc::display() const {
  return display_;
}

void DrmCrtc::set_display(int display) {
  display_ = display;
  requires_modeset_ = true;
}

bool DrmCrtc::can_bind(int display) const {
  return display_ == -1 || display_ == display;
}
}
