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

#ifndef __Hwcval_LogDisplay_h__
#define __Hwcval_LogDisplay_h__
#include<vector>
namespace Hwcval {
class LogDisplayMapping {
 public:
  LogDisplayMapping();
  LogDisplayMapping(uint32_t logDisp, uint32_t disp, uint32_t flags,
                    uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh,
                    uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh);

  void Log(const char* str);

  // Logical source display index
  uint32_t mLogDisplayIx;

  // Physical destination display index
  uint32_t mDisplayIx;

  // Flags
  uint32_t mFlags;

  // Source (logical display) co-ordinates
  uint32_t mSrcX;
  uint32_t mSrcY;
  uint32_t mSrcW;
  uint32_t mSrcH;

  // Destination (physical display) co-ordinates
  uint32_t mDstX;
  uint32_t mDstY;
  uint32_t mDstW;
  uint32_t mDstH;
};

class LogDisplay {
 public:
  LogDisplay(uint32_t displayIx = eNoDisplayIx);
  void SetDisplayIx(uint32_t displayIx);

  void SetConfigs(uint32_t* configs, uint32_t numConfigs);
  void SetActiveConfig(uint32_t configId);
  void SetDisplayAttributes(uint32_t configId, const int32_t attributes,
                            int32_t* values);

  int32_t GetWidth();
  int32_t GetHeight();

 private:
  // Display config ids by display config index
  std::vector<uint32_t> mConfigs;

  // Current configuration
  uint32_t mVSyncPeriod;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mXDPI;
  uint32_t mYDPI;

  uint32_t mConfigId;
  uint32_t mDisplayIx;
};

inline void LogDisplay::SetDisplayIx(uint32_t displayIx) {
  mDisplayIx = displayIx;
}

inline int32_t LogDisplay::GetWidth() {
  return mWidth;
}

inline int32_t LogDisplay::GetHeight() {
  return mHeight;
}
}

#endif  // __Hwcval_LogDisplay_h__
