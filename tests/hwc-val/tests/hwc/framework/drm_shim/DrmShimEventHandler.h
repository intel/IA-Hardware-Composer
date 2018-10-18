/*
// Copyright (c) 2018 Intel Corporation
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

#ifndef __Hwcval_DrmEventThread_h__
#define __Hwcval_DrmEventThread_h__

#include "EventThread.h"
#include "HwcTestState.h"

#include <xf86drm.h>

#define HWCVAL_MAX_EVENTS 100

class DrmShimChecks;
class DrmShimCrtc;

//*****************************************************************************
//
// DrmShimEventHandler class - responsible for capturing and forwarding
// page flip and VBlank events
//
//*****************************************************************************

struct DrmEventData {
  enum EventType { eVBlank, ePageFlip, eNone };

  EventType eventType;
  int fd;
  unsigned int seq;
  unsigned int sec;
  unsigned int usec;
  uint64_t data;
  DrmShimCrtc* crtc;

  DrmEventData()
      : eventType(DrmEventData::eNone),
        fd(0),
        seq(0),
        sec(0),
        usec(0),
        data(0),
        crtc(0) {
  }

  DrmEventData(const DrmEventData& rhs)
      : eventType(rhs.eventType),
        fd(rhs.fd),
        seq(rhs.seq),
        sec(rhs.sec),
        usec(rhs.usec),
        data(rhs.data),
        crtc(rhs.crtc) {
  }
};

class DrmShimEventHandler : public EventThread<DrmEventData, HWCVAL_MAX_EVENTS>,
                            public HwcTestEventHandler {
 public:
  DrmShimEventHandler(DrmShimChecks* checks);
  virtual ~DrmShimEventHandler();

  void QueueCaptureVBlank(int fd, uint32_t crtcId);
  void CaptureVBlank(int fd, uint32_t crtcId);
  void CancelEvent(uint32_t crtcId);

  int WaitVBlank(drmVBlankPtr vbl);

  int HandleEvent(int fd, drmEventContextPtr evctx);

 private:
  static DrmShimEventHandler* mInstance;
  DrmShimChecks* mChecks;

  drmEventContext mUserEvctx;
  drmEventContext mRealEvctx;

  int mDrmFd;
  DrmEventData mSavedPF;

  virtual void onFirstRef();
  virtual bool threadLoop();

  bool RaiseEventFromQueue();
  void FwdVBlank(int fd, unsigned int frame, unsigned int sec,
                 unsigned int usec, void* data);
  virtual void Restore(uint32_t disp);

  static void vblank_handler(int fd, unsigned int frame, unsigned int sec,
                             unsigned int usec, void* data);
};

#endif  // __Hwcval_DrmEventThread_h__
