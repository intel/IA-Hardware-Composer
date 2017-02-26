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

#include "pageflipeventhandler.h"

#include <stdlib.h>
#include <time.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>

#include "hwctrace.h"

namespace hwcomposer {

static const int64_t kOneSecondNs = 1 * 1000 * 1000 * 1000;

PageFlipEventHandler::PageFlipEventHandler()
    : HWCThread(-8, "PageFlipEventHandler") {
}

PageFlipEventHandler::~PageFlipEventHandler() {
}

void PageFlipEventHandler::Init(float refresh, int fd, int pipe) {
  ScopedSpinLock lock(spin_lock_);
  refresh_ = refresh;
  fd_ = fd;
  pipe_ = pipe;
}

int PageFlipEventHandler::RegisterCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display) {
  spin_lock_.lock();
  callback_ = callback;
  display_ = display;
  last_timestamp_ = -1;
  spin_lock_.unlock();

  if (!InitWorker()) {
    ETRACE("Failed to initalize thread for PageFlipEventHandler. %s",
           PRINTERROR());
  }

  return 0;
}

int PageFlipEventHandler::VSyncControl(bool enabled) {
  IPAGEFLIPEVENTTRACE("PageFlipEventHandler VSyncControl enabled %d", enabled);
  if (enabled_ == enabled)
    return 0;

  ScopedSpinLock lock(spin_lock_);
  enabled_ = enabled;
  last_timestamp_ = -1;

  return 0;
}

void PageFlipEventHandler::HandlePageFlipEvent(unsigned int sec,
                                               unsigned int usec) {
  ScopedSpinLock lock(spin_lock_);
  if (!enabled_ || !callback_)
    return;

  int64_t timestamp = (int64_t)sec * kOneSecondNs + (int64_t)usec * 1000;
  IPAGEFLIPEVENTTRACE("HandleVblankCallBack Frame Time %f",
                      static_cast<float>(timestamp - last_timestamp_) / (1000));
  last_timestamp_ = timestamp;

  IPAGEFLIPEVENTTRACE("Callback called from HandlePageFlipEvent. %lu",
                      timestamp);
  callback_->Callback(display_, timestamp);
}

void PageFlipEventHandler::HandleRoutine() {
  spin_lock_.lock();

  bool enabled = enabled_;
  int fd = fd_;
  int pipe = pipe_;

  spin_lock_.unlock();

  if (!enabled)
    return;

  uint32_t high_crtc = (pipe << DRM_VBLANK_HIGH_CRTC_SHIFT);

  drmVBlank vblank;
  memset(&vblank, 0, sizeof(vblank));
  vblank.request.type = (drmVBlankSeqType)(
      DRM_VBLANK_RELATIVE | (high_crtc & DRM_VBLANK_HIGH_CRTC_MASK));
  vblank.request.sequence = 1;

  int ret = drmWaitVBlank(fd, &vblank);
  if (!ret)
    HandlePageFlipEvent(vblank.reply.tval_sec, (int64_t)vblank.reply.tval_usec);
}

}  // namespace hwcomposer
