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

#ifndef __HwchDisplay_h__
#define __HwchDisplay_h__

#include <sys/types.h>
#include <vector>
#include <hardware/hwcomposer_defs.h>
#include "HwchCoord.h"
#include "HwchBufferSet.h"

#include "hwcserviceapi.h"

#define HWCH_VIRTUAL_NUM_BUFFERS 4

namespace Hwch {
class BufferFormatConfigManager;
class Interface;

inline hwcomposer::HWCRotation AddRotation(hwcomposer::HWCRotation r1,
                                           hwcomposer::HWCRotation r2) {
  int r = (((int)r1) + ((int)r2)) & 3;
  return (hwcomposer::HWCRotation)r;
}

inline hwcomposer::HWCRotation SubtractRotation(hwcomposer::HWCRotation r1,
                                                hwcomposer::HWCRotation r2) {
  int r = (((int)r1) - ((int)r2)) & 3;
  return (hwcomposer::HWCRotation)r;
}

inline bool RotIs90Or270(hwcomposer::HWCRotation rot) {
  int r = (int)rot;
  return (r & hwcomposer::HWCRotation::kRotate90);
}

class Layer;
class System;

class Display {
 public:
  Display();
  ~Display();

  // Initialise, and set display index
  void Init(hwcomposer::NativeBufferHandler *bufferHandler, uint32_t ix, Hwch::System* system);

  struct Attributes {
    uint32_t vsyncPeriod;
    uint32_t width;
    uint32_t height;
  };

  Attributes mAttributes;
  Attributes mOldAttributes;

  uint32_t GetVsyncPeriod();
  uint32_t GetWidth();
  void SetWidth(uint32_t width);
  uint32_t GetHeight();
  void SetHeight(uint32_t height);
  void SetConnected(bool connected);
  uint32_t GetLogicalWidth();
  uint32_t GetLogicalHeight();

  void CloneTransform(Layer& panelLayer, Layer& layer);

  void CreateFramebufferTarget();
  Layer& GetFramebufferTarget();

  bool IsConnected();

  hwcomposer::HWCRotation SetRotation(hwcomposer::HWCRotation rotation);
  hwcomposer::HWCRotation GetRotation();

  static int RotateTransform(int transform, hwcomposer::HWCRotation rot);
  int RotateTransform(int transform);

  // Convert Logical Rect to Rect allowing for current display rotation
  void ConvertRect(uint32_t bufferFormat, LogicalRect<int>& lr,
                   hwcomposer::HwcRect<int>& r);

  // Copy logical rect to rect ignoring current display rotation
  void CopyRect(uint32_t bufferFormat, LogicalRect<int>& lr,
                hwcomposer::HwcRect<int>& r);

  // Enables Virtual display support for this display
  void EmulateVirtualDisplay(void);
  bool IsVirtualDisplay(void);

  // Creates an external buffer set (suitable for use with Virtual
  void CreateExternalBufferSet(void);

  // Returns the next handle in the buffer set.
  HWCNativeHandle GetNextExternalBuffer(void);

  // Display mode control
  typedef HwcsDisplayModeInfo Mode;

  // Video optimization mode
  // Make everything build without too many ifdefs
  typedef int VideoOptimizationMode;

  uint32_t GetModes();
  bool GetCurrentMode(Mode& mode);
  bool GetCurrentMode(uint32_t& ix);
  Mode GetMode(uint32_t ix);
  bool SetMode(uint32_t ix, int32_t delayUs = 0);
  bool SetMode(const Mode& mode, int32_t delayUs = 0);
  bool ClearMode();

  bool HasScreenSizeChanged();
  void RecordScreenSize();
  bool GetHwcsHandle(void);

 private:
  Display(const Display& display);  // verboten

  uint32_t mDisplayIx;
  Layer* mFramebufferTarget;
  hwcomposer::HWCRotation mRotation;
  // HWC Service Api support
  HWCSHANDLE mHwcsHandle = nullptr;

  // This is the BufferSet which acts as the composition target for
  BufferSetPtr mExternalBufferSet;

  bool mVirtualDisplay;
  bool mConnected;

  static const int mRotationTable[hwcomposer::HWCRotation::kMaxRotate]
                                 [hwcomposer::HWCTransform::kMaxTransform];

  BufferFormatConfigManager* mFmtCfgMgr;

  std::vector<Mode> mModes;
  hwcomposer::NativeBufferHandler *bufHandler;
};
};

inline bool IsEqual(const Hwch::Display::Mode& mode1,
                    const Hwch::Display::Mode& mode2) {
  return (memcmp(&mode1, &mode2, sizeof(Hwch::Display::Mode)) == 0);
}

inline uint32_t Hwch::Display::GetVsyncPeriod() {
  return mAttributes.vsyncPeriod;
}

inline uint32_t Hwch::Display::GetWidth() {
  return mAttributes.width;
}

inline uint32_t Hwch::Display::GetHeight() {
  return mAttributes.height;
}

inline void Hwch::Display::SetWidth(uint32_t width) {
  mAttributes.width = width;
}

inline void Hwch::Display::SetHeight(uint32_t height) {
  mAttributes.height = height;
}

inline void Hwch::Display::SetConnected(bool connected) {
  mConnected = connected;
}

#endif  // __HwchDisplay_h__
