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

#include "HwchSystem.h"
#include "HwchDefs.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include <dlfcn.h>

#include <cutils/properties.h>
#include <platformdefines.h>

using namespace hwcomposer;

Hwch::System::System()
    : mQuiet(false),
      mNoFill(false),
      mNoCompose(false),
      mUpdateRateFixed(false),
      mRotationAnimation(false),
      mDefaultNumBuffers(HWCH_DEFAULT_NUM_BUFFERS),
      mVirtualDisplayWidth(0),
      mVirtualDisplayHeight(0),
      mFenceTimeoutMs(HWCH_FENCE_TIMEOUT),
      mSyncOption(eSet),
      mAsyncEventGenerator(0),
      mKernelEventGenerator(0),
      mEnableGl(false),
      mSendRange(INT_MIN, INT_MAX),
      mHDMIToBeTested(true),
      mLinearOption("fblinear", *this),
      mXTileOption("fbxtile", *this),
      mYTileOption("fbytile", *this),
      bufferHandler(NULL) {
  if (bufferHandler == NULL) {
    int fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd == -1) {
      ETRACE("Can't open dri file");
      exit(-1);
     }
     bufferHandler = hwcomposer::NativeBufferHandler::CreateInstance(fd);
  }

  for (uint32_t disp = 0; disp < MAX_DISPLAYS; ++disp) {
    mDisplays[disp].Init(bufferHandler, disp, this);
  }
}

Hwch::System::~System() {
  if (mEnableGl) {
    mGlInterface.Term();
  }
}

void Hwch::System::die() {
  delete this;
  mInstance = 0;
}

#ifdef HWCVAL_RESOURCE_LEAK_CHECKING

extern "C" void malloc_debug_finalize(int malloc_debug_level);
extern "C" void get_malloc_leak_info(uint8_t** info, size_t* overallSize,
                                     size_t* infoSize, size_t* totalMemory,
                                     size_t* backtraceSize);
extern "C" void free_malloc_leak_info(uint8_t* info);

static void* g_demangler = NULL;
typedef char* (*DemanglerFn)(const char*, char*, size_t*, int*);
static DemanglerFn g_demangler_fn = NULL;
static bool noDemangle = false;

static const char* demangle(const char* symbol) {
  if (noDemangle) {
    return symbol;
  }

  if (g_demangler_fn == 0) {
    g_demangler = dlopen("libgccdemangle.so", RTLD_NOW);
    if (g_demangler != NULL) {
      void* sym = dlsym(g_demangler, "__cxa_demangle");
      g_demangler_fn = reinterpret_cast<DemanglerFn>(sym);
    }

    if (g_demangler_fn == 0) {
      noDemangle = true;
      return symbol;
    }
  }

  const char* str = (*g_demangler_fn)(symbol, NULL, NULL, NULL);

  if (str == 0) {
    return symbol;
  } else {
    return str;
  }
}

static void DumpBacktrace(void* addr) {
  uint32_t offset = 0;
  const char* symbol = NULL;

  Dl_info info;
  if (dladdr(addr, &info) != 0) {
    offset =
        reinterpret_cast<char*>(addr) - reinterpret_cast<char*>(info.dli_saddr);
    symbol = info.dli_sname;
  } else {
    return;
  }

  if (symbol == 0) {
    HWCLOGD("    %p", addr);
  } else {
    HWCLOGD("    %s+%d", demangle(symbol), int32_t(offset));
  }
}
#endif

void Hwch::System::QuickExit(int status) {
#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
  // Dump memory leaks from libc
  // Requires libc.debug.malloc set to 1
  //
  uint8_t* info;
  size_t overallSize;
  size_t infoSize;
  size_t totalMemory;
  size_t backtraceSize;
  get_malloc_leak_info(&info, &overallSize, &infoSize, &totalMemory,
                       &backtraceSize);
  HWCLOGD("info@%p overallSize %" PRIi64 " infoSize %" PRIi64
          " totalMemory %" PRIi64 " backtraceSize %" PRIi64,
          info, uint64_t(overallSize), uint64_t(infoSize),
          uint64_t(totalMemory), uint64_t(backtraceSize));

  uint64_t runningTotal = 0;

  for (size_t offset = 0; offset < overallSize; offset += infoSize) {
    uint8_t* ptr = info + offset;
    uint64_t entrySize = *((size_t*)ptr);
    uint64_t allocations = *((size_t*)(ptr + sizeof(size_t)));
    uint64_t total = entrySize * allocations;
    runningTotal += total;

    if (entrySize > 0) {
      HWCLOGD("Leak %" PRIi64 ":  %" PRIi64 " bytes * %" PRIi64 " = %" PRIi64
              " Sum %" PRIi64,
              uint64_t(offset), entrySize, allocations, total, runningTotal);

      // Dump stack trace for anything 1MB or more
      if (total > 1000000) {
        for (uint8_t* p = ptr + 8; ((p < ptr + infoSize) && (*p));
             p += sizeof(uintptr_t)) {
          void* frame = *((void**)p);

          if (frame) {
            DumpBacktrace(frame);
          }
        }
      }
    }
  }

  free_malloc_leak_info(info);

  HWCLOGA(" Total memory: %" PRIi64, uint64_t(totalMemory));
  sleep(1);
#endif

  _exit(status);
}

void Hwch::System::EnableGl() {
  mEnableGl = true;

  if (mEnableGl) {
    mGlInterface.Init();
  }
}

Hwch::GlInterface& Hwch::System::GetGl() {
  return mGlInterface;
}

Hwch::System* Hwch::System::mInstance = 0;

Hwch::System& Hwch::System::getInstance() {
  if (mInstance == 0) {
    mInstance = new Hwch::System();
  }
  return *mInstance;
}

Hwch::Display& Hwch::System::GetDisplay(uint32_t disp) {
  return mDisplays[disp];
}

void Hwch::System::CreateFramebufferTargets() {
  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    GetDisplay(i).CreateFramebufferTarget();
  }
}

uint32_t Hwch::System::GetWallpaperSize() {
  return max(mDisplays[0].GetWidth(), mDisplays[0].GetHeight());
}

void Hwch::System::SetNoFill(bool noFill) {
  mNoFill = noFill;
}

bool Hwch::System::IsFillDisabled() {
  return mNoFill;
}

void Hwch::System::SetNoCompose(bool noCompose) {
  mNoCompose = noCompose;
}

bool Hwch::System::GetNoCompose() {
  return mNoCompose;
}

Hwch::VSync& Hwch::System::GetVSync() {
  return mVSync;
}

void Hwch::System::SetSyncOption(const char* syncOptionStr) {
  // What are we synchronzing to?
  mSyncOption = eSet;

  if (strcmp(syncOptionStr, "compose") == 0) {
    mSyncOption = eCompose;
  } else if (strcmp(syncOptionStr, "prepare") == 0) {
    mSyncOption = ePrepare;
  } else if (strcmp(syncOptionStr, "set") == 0) {
    mSyncOption = eSet;
  } else if (strcmp(syncOptionStr, "none") == 0) {
    mSyncOption = eNone;
  } else {
    HWCERROR(eCheckTestFail, "Unknown sync option %s", syncOptionStr);
  }
}

Hwch::SyncOptionType Hwch::System::GetSyncOption() {
  return mSyncOption;
}

void Hwch::System::SetFenceTimeout(uint32_t timeoutMs) {
  mFenceTimeoutMs = timeoutMs;
}

uint32_t Hwch::System::GetFenceTimeout() {
  return mFenceTimeoutMs;
}

void Hwch::System::SetUpdateRateFixed(bool fixed) {
  mUpdateRateFixed = fixed;
}

bool Hwch::System::IsUpdateRateFixed() {
  return mUpdateRateFixed;
}

void Hwch::System::SetRotationAnimation(bool animate) {
  mRotationAnimation = animate;
}

bool Hwch::System::IsRotationAnimation() {
  return mRotationAnimation;
}

Hwch::BufferFormatConfigManager& Hwch::System::GetBufferFormatConfigManager() {
  return mFmtCfgMgr;
}

// Default number of buffers in each buffer set
void Hwch::System::SetDefaultNumBuffers(uint32_t numBuffers) {
  mDefaultNumBuffers = numBuffers;
}

uint32_t Hwch::System::GetDefaultNumBuffers() {
  return mDefaultNumBuffers;
}

Hwch::BufferDestroyer& Hwch::System::GetBufferDestroyer() {
  if (mBufferDestroyer.get() == 0) {
    mBufferDestroyer = std::unique_ptr<Hwch::BufferDestroyer>(new Hwch::BufferDestroyer);
  }

  return *mBufferDestroyer;
}

void Hwch::System::SetEventGenerator(Hwch::AsyncEventGenerator* eventGen) {
  ALOG_ASSERT(mAsyncEventGenerator == 0);
  mAsyncEventGenerator = eventGen;
}

void Hwch::System::SetKernelEventGenerator(
    Hwch::KernelEventGenerator* eventGen) {
  ALOG_ASSERT(mKernelEventGenerator == 0);
  mKernelEventGenerator = eventGen;
}

bool Hwch::System::AddEvent(uint32_t eventType, int32_t delayUs) {
  return AddEvent(eventType, 0, delayUs);
}

bool Hwch::System::AddEvent(
    uint32_t eventType, std::shared_ptr<Hwch::AsyncEvent::Data> data,
    int32_t delayUs, std::shared_ptr<Hwch::AsyncEvent::RepeatData> repeatData) {
  if (eventType & (AsyncEvent::eESDRecovery | AsyncEvent::eHotPlug |
                   AsyncEvent::eHotUnplug)) {
    ALOG_ASSERT(mKernelEventGenerator);
    return mKernelEventGenerator->Add(eventType, data, delayUs, repeatData);
  } else {
    ALOG_ASSERT(mAsyncEventGenerator);

    // Repeat data not currently supported in the multithreaded event generator.
    return mAsyncEventGenerator->Add(eventType, data, delayUs);
  }
}

Hwch::AsyncEventGenerator& Hwch::System::GetEventGenerator() {
  ALOG_ASSERT(mAsyncEventGenerator);
  return *mAsyncEventGenerator;
}

Hwch::KernelEventGenerator& Hwch::System::GetKernelEventGenerator() {
  ALOG_ASSERT(mKernelEventGenerator);
  return *mKernelEventGenerator;
}

Hwch::InputGenerator& Hwch::System::GetInputGenerator() {
  return mInputGenerator;
}

void Hwch::System::RetainBufferSet(std::shared_ptr<BufferSet>& bufs) {
  mRetainedBufferSets.push_back(bufs);
}

void Hwch::System::FlushRetainedBufferSets() {
  // Keep them for one more frame, until we are sure they have been replaced on
  // the screen
  mRetainedBufferSets2 = mRetainedBufferSets;
  mRetainedBufferSets.clear();
}

Hwch::PatternMgr& Hwch::System::GetPatternMgr() {
  return mPatternMgr;
}

////////////////////////////////////////////////////////////////
// Functions relating to the creation of Virtual Displays
void Hwch::System::EnableVirtualDisplayEmulation(int32_t width,
                                                 int32_t height) {
  mVirtualDisplayEnabled = true;
  mVirtualDisplayWidth = width;
  mVirtualDisplayHeight = height;
}

uint32_t Hwch::System::GetVirtualDisplayWidth() {
  return mVirtualDisplayWidth;
}

uint32_t Hwch::System::GetVirtualDisplayHeight() {
  return mVirtualDisplayHeight;
}

bool Hwch::System::IsVirtualDisplayEmulationEnabled() {
  return mVirtualDisplayEnabled;
}

// Provided as a regular function to avoid awkward compilation dependency in
// HwchCoord.h
uint32_t Hwch::GetWallpaperSize() {
  return Hwch::System::getInstance().GetWallpaperSize();
}

Hwch::PatternMgr& Hwch::GetPatternMgr() {
  return Hwch::System::getInstance().GetPatternMgr();
}

void Hwch::System::SetSendFrames(Hwch::Range& range) {
  mSendRange = range;
}

bool Hwch::System::IsFrameToBeSent(uint32_t frameNo) {
  return mSendRange.Test(frameNo);
}

void Hwch::System::SetHwcOption(std::string& option,
                                std::string& value) {
}

Hwch::System::HwcOptionState::HwcOptionState(const char* name,
                                             Hwch::System& system)
    : mName(name), mSystem(system), mDefaultSet(false) {
}

void Hwch::System::HwcOptionState::Override(bool enable) {
  if (!mDefaultSet) {
    const char* str =
        HwcTestState::getInstance()->GetHwcOptionStr(mName.c_str());

    if (!str) {
      HWCLOGD("%s option not set yet", mName.c_str());
      return;
    }

    mDefaultValue.assign(str);
    mDefaultSet = true;
    mCurrentValue = mDefaultValue;
  }

  std::string desiredValue(enable ? "1" : "0");

  if (desiredValue != mCurrentValue) {
    HWCLOGV_COND(eLogHarness, "Setting %s option to %s", mName.c_str(),
                 desiredValue.c_str());
    // mSystem.SetHwcOption(mName, desiredValue);
    mCurrentValue = desiredValue;
  }
}

void Hwch::System::HwcOptionState::Reset() {
  if (mDefaultSet) {
    if (mCurrentValue != mDefaultValue) {
      HWCLOGV_COND(eLogHarness, "Resetting %s option to %s", mName.c_str(),
                   mDefaultValue.c_str());
      mSystem.SetHwcOption(mName, mDefaultValue);
      mCurrentValue = mDefaultValue;
    }
  }
}

void Hwch::System::OverrideTile(uint32_t tile) {
  HWCLOGV_COND(eLogHarness, "Overriding tiling to %d", tile);
  mLinearOption.Override(tile & Hwch::Layer::eLinear);
  mXTileOption.Override(tile & Hwch::Layer::eXTile);
  mYTileOption.Override(tile & Hwch::Layer::eYTile);
}

void Hwch::System::ResetTile() {
  HWCLOGV_COND(eLogHarness, "Resetting tiling");
  mLinearOption.Reset();
  mXTileOption.Reset();
  mYTileOption.Reset();
}

void Hwch::System::SetGlobalRenderCompression(
    Hwch::Layer::CompressionType compType) {
  mGlobalRenderCompression = compType;
  mGlobalRCEnabled = true;
}

bool Hwch::System::IsGlobalRenderCompressionEnabled(void) {
  return mGlobalRCEnabled;
}

Hwch::Layer::CompressionType Hwch::System::GetGlobalRenderCompression(void) {
  return mGlobalRenderCompression;
}

void Hwch::System::SetRCIgnoreHintRange(Hwch::Range& range) {
  mRCIgnoreHintRange = range;
}

bool Hwch::System::IsRCHintToBeIgnored(void) {
  return mRCIgnoreHintRange.Next();
}
