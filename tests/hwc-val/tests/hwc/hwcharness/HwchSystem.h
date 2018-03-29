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

#ifndef __HwchSystem_h__
#define __HwchSystem_h__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "HwcvalDebug.h"
#include "HwchDefs.h"
#include "HwchDisplay.h"
#include "HwchLayer.h"
#include "HwchVSync.h"
#include "HwchBufferFormatConfig.h"
#include "HwchBufferDestroyer.h"
#include "HwchAsyncEventGenerator.h"
#include "HwchGlInterface.h"
#include "HwchPatternMgr.h"
#include "HwchRange.h"
#include "HwchInputGenerator.h"

#include "iservice.h"

namespace Hwch {
enum SyncOptionType { eCompose, ePrepare, eSet, eNone };

class System {
 public:
  System();
  ~System();

  // Perform any essential shutdown functions and die
  static void QuickExit(int status = 0);

  Display& GetDisplay(uint32_t disp);

  void CreateFramebufferTargets();
  uint32_t GetWallpaperSize();
  void SetQuiet(bool quiet);

  // Setup GL
  void EnableGl();
  GlInterface& GetGl();

  // Inhibit buffer fill
  void SetNoFill(bool noFill);
  bool IsFillDisabled();

  // set/get Reference Composer composition
  void SetNoCompose(bool noCompose);
  bool GetNoCompose();

  // Set/get is update rate fixed, i.e. worked out by frame counting rather than
  // time
  void SetUpdateRateFixed(bool fixed);
  bool IsUpdateRateFixed();

  // Enables rotation animation (and checks)
  void SetRotationAnimation(bool animate);
  bool IsRotationAnimation();

  BufferFormatConfigManager& GetBufferFormatConfigManager();

  // Default number of buffers in each buffer set
  void SetDefaultNumBuffers(uint32_t numBuffers);
  uint32_t GetDefaultNumBuffers();

  Hwch::BufferDestroyer& GetBufferDestroyer();
  void SetEventGenerator(Hwch::AsyncEventGenerator* eventGen);
  void SetKernelEventGenerator(Hwch::KernelEventGenerator* eventGen);
  bool AddEvent(uint32_t eventType, int32_t delayUs);
  bool AddEvent(uint32_t eventType, std::shared_ptr<Hwch::AsyncEvent::Data> data,
                int32_t delayUs,
                std::shared_ptr<AsyncEvent::RepeatData> repeatData = 0);
  Hwch::AsyncEventGenerator& GetEventGenerator();
  Hwch::KernelEventGenerator& GetKernelEventGenerator();

  // Functions for setting up and querying the Virtual Display settings
  void EnableVirtualDisplayEmulation(int32_t width, int32_t height);
  uint32_t GetVirtualDisplayWidth();
  uint32_t GetVirtualDisplayHeight();
  bool IsVirtualDisplayEmulationEnabled();

  enum FenceReleaseMode { eSequential = 0, eRandom, eRetainOldest, eLastEntry };

  void SetSendFrames(Hwch::Range& range);
  bool IsFrameToBeSent(uint32_t frameNo);

  void SetHwcOption(std::string& option, std::string& value);
  void OverrideTile(uint32_t tile);
  void ResetTile();

  void SetGlobalRenderCompression(Hwch::Layer::CompressionType compType);
  Hwch::Layer::CompressionType GetGlobalRenderCompression(void);
  void SetRCIgnoreHintRange(Hwch::Range& range);
  bool IsRCHintToBeIgnored();
  bool IsGlobalRenderCompressionEnabled();

  VSync& GetVSync();

  void SetSyncOption(const char* syncOptionStr);
  SyncOptionType GetSyncOption();

  void SetFenceTimeout(uint32_t fenceTimeoutMs);
  uint32_t GetFenceTimeout();

  void RetainBufferSet(std::shared_ptr<BufferSet>& bufs);
  void FlushRetainedBufferSets();

  PatternMgr& GetPatternMgr();
  InputGenerator& GetInputGenerator();

  void SetHDMIToBeTested(bool enable);
  bool IsHDMIToBeTested();

  static System& getInstance();
  void die();
 hwcomposer::NativeBufferHandler *bufferHandler;
 private:
  class HwcOptionState {
   public:
    std::string mName;
    System& mSystem;

    std::string mDefaultValue;
    std::string mCurrentValue;
    bool mDefaultSet;

    HwcOptionState(const char* name, System& system);
    void Override(bool enable);
    void Reset();
  };

  static System* mInstance;

  Display mDisplays[MAX_DISPLAYS];

  bool mQuiet;

  bool mNoFill;
  bool mNoCompose;  // flag to allow/disable Reference Composer composition
  bool mUpdateRateFixed;
  bool mRotationAnimation;

  // Default number of buffers in each buffer set
  uint32_t mDefaultNumBuffers;

  bool mVirtualDisplayEnabled = false;
  uint32_t mVirtualDisplayWidth = 0;
  uint32_t mVirtualDisplayHeight = 0;

  uint32_t mFenceTimeoutMs;

  VSync mVSync;
  SyncOptionType mSyncOption;

  BufferFormatConfigManager mFmtCfgMgr;

  std::unique_ptr<Hwch::BufferDestroyer> mBufferDestroyer;
  Hwch::AsyncEventGenerator* mAsyncEventGenerator;
  Hwch::KernelEventGenerator* mKernelEventGenerator;

  bool mEnableGl;
  GlInterface mGlInterface;

  std::vector<std::shared_ptr<Hwch::BufferSet> > mRetainedBufferSets;
  std::vector<std::shared_ptr<Hwch::BufferSet> > mRetainedBufferSets2;

  // Frame range to be actually sent to HWC
  Hwch::Range mSendRange;

  // Pattern manager - factory for GL/CPU-based pattern classes
  PatternMgr mPatternMgr;

  // Input Generator - avert the screen turning off when we don't want it to
  InputGenerator mInputGenerator;

  // Do we test HDMI if it is connected?
  bool mHDMIToBeTested;

  // Tiling options in HWC
  HwcOptionState mLinearOption;
  HwcOptionState mXTileOption;
  HwcOptionState mYTileOption;

  // Render Compression member variables
  bool mGlobalRCEnabled = false;
  Hwch::Layer::CompressionType mGlobalRenderCompression =
      Hwch::Layer::eCompressionAuto;
  Hwch::Range mRCIgnoreHintRange;
};

PatternMgr& GetPatternMgr();

inline bool System::IsHDMIToBeTested() {
  return mHDMIToBeTested;
}

inline void System::SetHDMIToBeTested(bool enable) {
  mHDMIToBeTested = enable;
}
};

#endif  // __HwchSystem_h__
