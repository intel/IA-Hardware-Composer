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

#ifndef __HwchAsyncEventGenerator_h__
#define __HwchAsyncEventGenerator_h__

#include "EventMultiThread.h"
#include <ui/GraphicBuffer.h>
#include <utils/StrongPointer.h>
#include "HwchDefs.h"
#include "HwchDisplay.h"
#include "HwcvalWatchdog.h"
#include "hwcserviceapi.h"

namespace Hwch {
class Interface;

class AsyncEvent {
 public:
  static std::string EventName(uint32_t eventType);

 public:
  // Event type bitmask
  // Some events can be combined (e.g. blank and suspend); other combinations
  // do not make sense (suspend and resume) and hence won't take place.
  enum Type {
    // Send Blank/unblank via HWC interface
    eBlank = 1,
    eUnblank = 2,

    // Suspend/resume power
    eSuspend = 4,
    eResume = 8,

    // Display mode set
    eModeSet = 0x1000,
    eModeClear = 0x2000,
    eSetVideoOptimizationMode = 0x4000,

    // KERNEL EVENTS
    // ESD recovery
    eESDRecovery = 0x10000000,

    // Hotplug/unplug
    eHotPlug = 0x20000000,
    eHotUnplug = 0x40000000
  };

  // Types(s) of display to hot plug
  static const uint32_t cFixedDisplay;
  static const uint32_t cRemovableDisplay;
  static const uint32_t cAllDisplays;

  // Repeat data class for events
  // (especially kernel events)
  class RepeatData  {
   public:
    RepeatData(uint32_t burstLength = 1, uint32_t delayUs = 0)
        : mBurstLength(burstLength), mDelayUs(delayUs) {
    }

    virtual ~RepeatData() {
    }

    uint32_t mBurstLength;
    uint32_t mDelayUs;
  };

  // Generic additional data class for events
  class Data  {
   public:
    Data() {
    }

    virtual ~Data() {
    }
  };

  class HotPlugEventData : public Data {
   public:
    HotPlugEventData(uint32_t displayTypes);
    virtual ~HotPlugEventData();

    uint32_t mDisplayTypes;
  };

  class ModeChangeEventData : public Data {
   public:
    ModeChangeEventData(uint32_t displayIx, const Display::Mode& mode);
    virtual ~ModeChangeEventData();

    uint32_t mDisplayIx;
    Display::Mode mMode;
  };

  class VideoOptimizationModeData : public Data {
   public:
    VideoOptimizationModeData(
        const Display::VideoOptimizationMode videoOptimizationMode);
    virtual ~VideoOptimizationModeData();

    Display::VideoOptimizationMode mVideoOptimizationMode;
  };

  AsyncEvent();
  AsyncEvent(const AsyncEvent& rhs);
  virtual ~AsyncEvent();

  uint32_t mType;

  std::shared_ptr<Data> mData;

  // Delay before the event takes place.
  // Any negative value: event takes place synchronously on the main thread.
  // Zero: event takes place on Event Generator thread, with minimum delay.
  // Positive: event takes place on Event Generator thread after stated delay in
  // microseconds.
  int64_t mTargetTime;

  // Optional event repeat
  std::shared_ptr<RepeatData> mRepeat;
};

class AsyncEventGenerator : public EventMultiThread<AsyncEvent, 256, 16> {
 public:
  AsyncEventGenerator(Hwch::Interface& iface);
  virtual ~AsyncEventGenerator();
  bool Add(uint32_t eventType, int32_t delayUs);
  bool Add(uint32_t eventType, std::shared_ptr<Hwch::AsyncEvent::Data> data,
           int32_t delayUs);
  bool Do(uint32_t eventType, std::shared_ptr<AsyncEvent::Data> data = 0);
  virtual void Do(AsyncEvent& ev);
  bool GetHwcsHandle(void);

 private:
  Hwch::Interface& mInterface;

  bool Blank(bool blank);
  bool SuspendResume(bool suspend);
  bool ModeSet(AsyncEvent::ModeChangeEventData* mc);
  bool ModeClear(AsyncEvent::ModeChangeEventData* mc);
  bool SetVideoOptimizationMode(
      AsyncEvent::VideoOptimizationModeData* eventData);

  bool mAllowSimultaneousBlank;
  volatile int mBlankInProgress;
  bool mBlankStateRequired;
  // HWC Service Api support
  HWCSHANDLE mHwcsHandle = nullptr;
};

// All kernel events will come from one thread.
// We don't want to confuse HWC too much by doing hotplugs and unplugs at the
// same time.
class KernelEventGenerator : public EventThread<AsyncEvent, 256> {
 public:
  KernelEventGenerator();
  virtual ~KernelEventGenerator();
  bool Add(uint32_t eventType, std::shared_ptr<AsyncEvent::Data> data,
           int32_t delayUs, std::shared_ptr<AsyncEvent::RepeatData> repeatData);
  bool Do(uint32_t eventType, std::shared_ptr<AsyncEvent::Data> data,
          std::shared_ptr<AsyncEvent::RepeatData> repeatData);

  void SetEsdConnectorId(uint32_t conn);
  void ClearContinuous();

  void GetCounts(uint32_t& hotUnplugCount, uint32_t& esdRecoveryCount);
  void ResetCounts();

 protected:
  virtual bool threadLoop();

 private:
  uint32_t mEsdConnectorId;

  uint32_t mHotUnplugCount;
  uint32_t mEsdRecoveryCount;

  volatile bool mContinueRepeat;
  volatile bool mRepeating;

  Hwcval::Watchdog mHotPlugWatchdog;

  bool SendEsdRecoveryEvent();
  bool HotPlug(bool connect, uint32_t displayTypes);
  bool ModeSet(AsyncEvent::ModeChangeEventData* mc);
  bool ModeClear(AsyncEvent::ModeChangeEventData* mc);
};
}

#endif  // __HwchAsyncEventGenerator_h__
