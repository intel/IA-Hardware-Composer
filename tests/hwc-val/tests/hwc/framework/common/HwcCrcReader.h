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

#ifndef __HwcCrcReader_h
#define __HwcCrcReader_h
#include "hwcthread.h"

#include "CrcDebugfs.h"

class HwcCrcReaderInterface {
 public:
  enum CRC_SUSPENSIONS {
    CRC_SUSPEND_NOT_VALID,
    CRC_SUSPEND_BLANKING,
    CRC_SUSPEND_MODE_CHANGE
  };

 public:
  HwcCrcReaderInterface() {
  }
  virtual ~HwcCrcReaderInterface() {
  }

  virtual bool IsEnabled() const = 0;
  virtual void CheckEnabledState(HwcTestCrtc *crtc) = 0;
  virtual void NotifyPageFlip(HwcTestCrtc *crtc) = 0;
  virtual void SuspendCRCs(int crtcId, enum CRC_SUSPENSIONS reason,
                           bool suspend) = 0;

 protected:
  virtual void Reset() = 0;
  virtual bool Enable() = 0;
  virtual bool Disable(bool calledFromReaderThread = false) = 0;
  virtual void DoRenderStall(HwcTestCrtc *crtc) = 0;
  virtual bool ConfigurePipe() = 0;
  virtual bool GetCRCSource(int &pipe, int &source) = 0;
  virtual uint32_t StableCRCCount() const = 0;
  virtual bool UpdateCRCRuns() = 0;
  virtual void ProcessCRC(crc_t &res) = 0;
  virtual void DebugCRC() = 0;
};

class HwcCrcReaderShim;

class HwcCrcReader : public HwcCrcReaderInterface, public hwcomposer::HWCThread {
  friend class HwcCrcReaderShim;

 public:
  HwcCrcReader(HwcTestKernel *pKernel, HwcTestState *pState);
  virtual ~HwcCrcReader();

  bool IsEnabled() const;
  void CheckEnabledState(HwcTestCrtc *crtc);
  void NotifyPageFlip(HwcTestCrtc *crtc);
  void SuspendCRCs(int crtcId, enum CRC_SUSPENSIONS reason, bool suspend);

 protected:
  void Reset();
  bool Enable();
  bool Disable(bool calledFromReaderThread = false);
  void DoRenderStall(HwcTestCrtc *crtc);
  bool ConfigurePipe();
  bool GetCRCSource(int &pipe, int &source);
  uint32_t StableCRCCount() const;
  bool UpdateCRCRuns();
  void ProcessCRC(crc_t &res);
  void DebugCRC();
  void HandleRoutine();

 private:
  HwcTestKernel *mpKernel;
  HwcTestState *mpState;
  bool mThreadRunning;
  Debugfs mDbgfs;
  CRCCtlFile mfCtl;
  CRCDataFile mfCrc;
  Hwcval::Mutex mMtxCRCEnabled;
  Hwcval::Condition mCRCEnabledCondition;
  int mCRCCrtcId;
  enum CRC_SUSPENSIONS mCRCsSuspensionReason;

  bool mEnabled;   // true if CRC validation is enabled
  uint32_t mCrcs;  // number of CRC results (equates to the number of VSYNCs
                   // since CRCs enabled)
  uint32_t mCrcsOnEnable;  // mCrcs at the point that the CRC reader was enabled
  uint32_t mCrcRuns;       // number of CRC runs
  uint32_t mCrcRunLength;  // number of CRCs in the current run
  crc_t mCrcRes;           // the last CRC result...
  crc_t mCrcResPrev;       // ...the one prior to it
  uint32_t
      mPageFlips;  // the number of page flips since the CRC reader was enabled
  uint32_t mRepeatedFrames;  // the number of page flips in which the displayed
                             // frame was repeated
  uint32_t mCrcErrors;       // the number of CRC errors detected
  uint32_t mShortRuns;       // the number of valid short runs

  enum pipe mPipe;
  enum intel_pipe_crc_source mSource;
};

class HwcCrcReaderShim : public HwcCrcReaderInterface {
 public:
  HwcCrcReaderShim(HwcTestKernel *pKernel, HwcTestState *pState)
      // HwcCrcReaderShim(HwcTestKernel *, HwcTestState *)
      : pReader(NULL) {
    pReader = new HwcCrcReader(pKernel, pState);
  }

  virtual ~HwcCrcReaderShim() {
    if (pReader)
      delete pReader;
  }

  bool IsEnabled() const {
    ALOGE("HwcCrcReaderShim - IsEnabled");
    return (pReader == NULL) ? false : pReader->IsEnabled();
  }

  void CheckEnabledState(HwcTestCrtc *crtc) {
    ALOGE("HwcCrcReaderShim - CheckEnabledState");
    if (pReader)
      pReader->CheckEnabledState(crtc);
  }

  void NotifyPageFlip(HwcTestCrtc *crtc) {
    ALOGE("HwcCrcReaderShim - NotifyPageFlip");
    if (pReader)
      pReader->NotifyPageFlip(crtc);
  }

  void SuspendCRCs(int crtcId, enum HwcCrcReader::CRC_SUSPENSIONS reason,
                   bool suspend) {
    ALOGE("HwcCrcReaderShim - SuspendCRCs");
    if (pReader)
      pReader->SuspendCRCs(crtcId, reason, suspend);
  }

 protected:
  void Reset() {
    ALOGE("HwcCrcReaderShim - Reset");
    if (pReader)
      pReader->Reset();
  }

  bool Enable() {
    ALOGE("HwcCrcReaderShim - Enable");
    return (pReader == NULL) ? false : pReader->Enable();
  }

  bool Disable(bool calledFromReaderThread = false) {
    ALOGE("HwcCrcReaderShim - Disable");
    return (pReader == NULL) ? false : pReader->Disable(calledFromReaderThread);
  }

  void DoRenderStall(HwcTestCrtc *crtc) {
    ALOGE("HwcCrcReaderShim - DoRenderStall");
    if (pReader)
      pReader->DoRenderStall(crtc);
  }

  bool ConfigurePipe() {
    ALOGE("HwcCrcReaderShim - ConfigurePipe");
    return (pReader == NULL) ? false : pReader->ConfigurePipe();
  }

  bool GetCRCSource(int &pipe, int &source) {
    ALOGE("HwcCrcReaderShim - GetCRCSource");
    return (pReader == NULL) ? false : pReader->GetCRCSource(pipe, source);
  }

  uint32_t StableCRCCount() const {
    ALOGE("HwcCrcReaderShim - StableCRCCount");
    return (pReader == NULL) ? 0 : pReader->StableCRCCount();
  }

  bool UpdateCRCRuns() {
    ALOGE("HwcCrcReaderShim - UpdateCRCRuns");
    return (pReader == NULL) ? false : pReader->UpdateCRCRuns();
  }

  void ProcessCRC(crc_t &res) {
    ALOGE("HwcCrcReaderShim - ProcessCRC");
    if (pReader)
      pReader->ProcessCRC(res);
  }

  void DebugCRC() {
    ALOGE("HwcCrcReaderShim - DebugCRC");
    if (pReader)
      pReader->DebugCRC();
  }

 private:
  HwcCrcReader *pReader;
};

#endif  // __HwcCrcReader_h
