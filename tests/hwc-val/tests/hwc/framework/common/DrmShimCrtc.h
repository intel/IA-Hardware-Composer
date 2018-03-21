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

#ifndef __DrmShimCrtc_h__
#define __DrmShimCrtc_h__

#include "HwcTestCrtc.h"
#include <ui/GraphicBuffer.h>

class DrmShimPlane;
class HwcTestKernel;
class HwcTestDisplayContents;

class DrmShimCrtc : public HwcTestCrtc {
 public:
  //-----------------------------------------------------------------------------
  // Constructor & Destructor
  DrmShimCrtc(uint32_t crtcId, uint32_t width, uint32_t height, uint32_t clock,
              uint32_t vrefresh);

  virtual ~DrmShimCrtc();

  void SetChecks(DrmShimChecks* checks);

  // Pipe index
  void SetPipeIndex(uint32_t pipeIx);
  uint32_t GetPipeIndex();

  // Connector Id
  void SetConnector(uint32_t connectorId);
  uint32_t GetConnector();
  virtual bool IsDRRSEnabled();

  // VBlank interception
  drmVBlankPtr SetupVBlank();
  drmVBlankPtr GetVBlank();
  void SetUserVBlank(drmVBlankPtr vbl);
  bool IssueVBlank(unsigned int frame, unsigned int sec, unsigned int usec,
                   void*& userData);

  void SavePageFlipUserData(uint64_t userData);
  uint64_t GetPageFlipUserData();

  // Drm call duration evaluation
  void DrmCallStart();
  int64_t GetDrmCallDuration();
  int64_t GetTimeSinceVBlank();

  bool IsVBlankRequested(uint32_t frame);
  void* GetVBlankUserData();

  // Hotplug
  bool SimulateHotPlug(bool connected);

  // Latch power state
  uint32_t SetDisplayEnter(bool suspended);
  void StopSetDisplayWatchdog();
  bool WasSuspended();

  // Report power at start of set display and now
  const char* ReportSetDisplayPower(char* strbuf,
                                    uint32_t len = HWCVAL_DEFAULT_STRLEN);

  // Nuclear->SetDisplay conversion
  typedef int (*DrmModeAddFB2Func)(int fd, uint32_t width, uint32_t height,
                                   uint32_t pixel_format,
                                   uint32_t bo_handles[4], uint32_t pitches[4],
                                   uint32_t offsets[4], uint32_t* buf_id,
                                   uint32_t flags);

  void NotifyPageFlip() override;

 private:
  // DRM checks
  DrmShimChecks* mChecks;

  // DRM pipe index
  uint32_t mPipeIx;

  // DRM connector id
  uint32_t mConnectorId;

  // Vblank structure issued to Drm
  drmVBlank mVBlank;

  // Vblank callback request data
  uint32_t mVBlankFrame;
  uint64_t mVBlankSignal;

  // User data from page flip event
  uint64_t mPageFlipUserData;

  // Start time for atomic DRM call
  uint64_t mDrmCallStartTime;

  // Power state at start of set display
  PowerState mPowerStartSetDisplay;
  bool mSuspendStartSetDisplay;
};

inline void DrmShimCrtc::SetChecks(DrmShimChecks* checks) {
  mChecks = checks;
}

inline bool DrmShimCrtc::WasSuspended() {
  return mSuspendStartSetDisplay;
}

inline void DrmShimCrtc::SetPipeIndex(uint32_t pipeIx) {
  mPipeIx = pipeIx;
}

inline uint32_t DrmShimCrtc::GetPipeIndex() {
  return mPipeIx;
}

// Connector Id
inline void DrmShimCrtc::SetConnector(uint32_t connectorId) {
  mConnectorId = connectorId;
}

inline uint32_t DrmShimCrtc::GetConnector() {
  return mConnectorId;
}

#endif  // __DrmShimCrtc_h__
