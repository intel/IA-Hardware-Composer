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

#ifndef __HwchBufferFormatConfig_h__
#define __HwchBufferFormatConfig_h__

#include "HwcTestState.h"
#include <map>

namespace Hwch {

class BufferFormatConfig {
 private:
  // Display frame minimum size
  uint32_t mMinDisplayFrameWidth;
  uint32_t mMinDisplayFrameHeight;

  // Display frame alignment mask
  uint32_t mDfXMask;
  uint32_t mDfYMask;

  uint32_t mMinBufferWidth;
  uint32_t mMinBufferHeight;

  // DONT allow buffers to have a size where
  // (size & mask) != 0
  // Round up to avoid this.
  uint32_t mBufferWidthAlignment;
  uint32_t mBufferHeightAlignment;

  // Crop alignment
  float mCropAlignment;
  float mMinCropWidth;
  float mMinCropHeight;

 public:
  BufferFormatConfig(uint32_t minDfWidth = 0, uint32_t minDfHeight = 0,
                     uint32_t minBufferWidth = 0, uint32_t minBufferHeight = 0,
                     uint32_t bufferWidthAlignment = 1,
                     uint32_t bufferHeightAlignment = 1,
                     float cropAlignment = 0.0, float minCropWidth = 0.0,
                     float minCropHeight = 0.0, uint32_t dfXMask = 0xffffffff,
                     uint32_t dfYMask = 0xffffffff);

  // Adjust display frame to comply with the min width & height
  void AdjustDisplayFrame(hwcomposer::HwcRect<int>& r, uint32_t displayWidth,
                          uint32_t displayHeight) const;

  // Adjust buffer size to comply with the buffer size and alignment
  // restrictions
  void AdjustBufferSize(uint32_t& w, uint32_t& h) const;

  // Adjust crop rectangle to comply with crop size and alignment restrictions
  void AdjustCropSize(uint32_t bw, uint32_t bh, float& w, float& h) const;
  void AdjustCrop(uint32_t bw, uint32_t bh, float& l, float& t, float& w,
                  float& h) const;
};

// Key: Buffer format
class BufferFormatConfigManager
    : public std::map<uint32_t, BufferFormatConfig> {
 public:
  BufferFormatConfigManager();

  // Adjust display frame to comply with the min width & height
  void AdjustDisplayFrame(uint32_t format, hwcomposer::HwcRect<int>& r,
                          uint32_t displayWidth, uint32_t displayHeight);

  // Adjust buffer size to comply with the
  void AdjustBufferSize(uint32_t format, uint32_t& w, uint32_t& h);

  // Adjust crop rectangle to comply with crop size and alignment restrictions
  void AdjustCropSize(uint32_t format, uint32_t bw, uint32_t bh, float& w,
                      float& h);
  void AdjustCrop(uint32_t format, uint32_t bw, uint32_t bh, float& l, float& t,
                  float& w, float& h);

  // Define parameters to be used when no configuration is present for the
  // selected format.
  void SetDefault(const BufferFormatConfig& cfg);

 private:
  BufferFormatConfig mDeflt;
};
}

#endif  // __HwchBufferFormatConfig_h__
