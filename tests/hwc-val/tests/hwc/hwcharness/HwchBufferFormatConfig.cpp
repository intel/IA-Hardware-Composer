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

#include "HwchBufferFormatConfig.h"
#include "HwchLayer.h"
#include "HwcTestUtil.h"

namespace Hwch {
BufferFormatConfig::BufferFormatConfig(
    uint32_t minDfWidth, uint32_t minDfHeight, uint32_t minBufferWidth,
    uint32_t minBufferHeight, uint32_t bufferWidthAlignment,
    uint32_t bufferHeightAlignment, float cropAlignment, float minCropWidth,
    float minCropHeight, uint32_t dfXMask, uint32_t dfYMask)
    : mMinDisplayFrameWidth(minDfWidth),
      mMinDisplayFrameHeight(minDfHeight),
      mDfXMask(dfXMask),
      mDfYMask(dfYMask),
      mMinBufferWidth(max<uint32_t>(minBufferWidth, minCropWidth)),
      mMinBufferHeight(max<uint32_t>(minBufferHeight, minCropHeight)),
      mBufferWidthAlignment(bufferWidthAlignment),
      mBufferHeightAlignment(bufferHeightAlignment),
      mCropAlignment(cropAlignment),
      mMinCropWidth(minCropWidth),
      mMinCropHeight(minCropHeight) {
  mMinDisplayFrameWidth = max(minDfWidth, (~dfXMask) + 1);
  mMinDisplayFrameHeight = max(minDfHeight, (~dfYMask) + 1);
}

// Adjust display frame to comply with the min width & height
void BufferFormatConfig::AdjustDisplayFrame(hwcomposer::HwcRect<int>& r,
                                            uint32_t displayWidth,
                                            uint32_t displayHeight) const {
  HWCLOGV_COND(eLogHarness, "AdjustDisplayFrame entry (%d, %d, %d, %d) %dx%d",
               r.left, r.top, r.right, r.bottom, displayWidth, displayHeight);

  if (HwcTestState::getInstance()->IsOptionEnabled(
          eOptDispFrameAlwaysInsideScreen)) {
    if ((r.right > int(displayWidth)) || (r.bottom > int(displayHeight))) {
      HWCLOGD_COND(eLogHarness, "Adjusting to %dx%d", displayWidth,
                   displayHeight);
    }

    if (r.right > int(displayWidth)) {
      r.right = displayWidth;
    }

    if (r.bottom > int(displayHeight)) {
      r.bottom = displayHeight;
    }

    if (r.left < 0) {
      r.left = 0;
    }

    if (r.top < 0) {
      r.top = 0;
    }
  }

  if ((r.right - r.left) < static_cast<int32_t>(mMinDisplayFrameWidth)) {
    int32_t right = r.left + mMinDisplayFrameWidth;
    if (right >= static_cast<int32_t>(displayWidth)) {
      r.left = r.right - mMinDisplayFrameWidth;
    } else {
      r.right = right;
    }
  }

  r.left &= mDfXMask;
  r.right &= mDfXMask;

  if ((r.bottom - r.top) < static_cast<int32_t>(mMinDisplayFrameHeight)) {
    int32_t bottom = r.top + mMinDisplayFrameHeight;
    if (bottom >= static_cast<int32_t>(displayHeight)) {
      r.top = r.bottom - mMinDisplayFrameHeight;
    } else {
      r.bottom = bottom;
    }
  }

  r.top &= mDfYMask;
  r.bottom &= mDfYMask;

  HWCLOGV_COND(eLogHarness, "AdjustDisplayFrame exit (%d, %d, %d, %d)", r.left,
               r.top, r.right, r.bottom);
  ALOG_ASSERT(r.right > r.left);
  ALOG_ASSERT(r.bottom > r.top);
}

// Adjust buffer size to comply with the min width & height, and alignment (i.e.
// whether odd values are permitted)
void BufferFormatConfig::AdjustBufferSize(uint32_t& w, uint32_t& h) const {
  if (w < mMinBufferWidth) {
    w = mMinBufferWidth;
  }

  if (h < mMinBufferHeight) {
    h = mMinBufferHeight;
  }

  uint32_t bufferWidthMask = mBufferWidthAlignment - 1;
  if ((w & bufferWidthMask) != 0) {
    w = (w & ~bufferWidthMask) + mBufferWidthAlignment;
  }

  uint32_t bufferHeightMask = mBufferHeightAlignment - 1;
  if ((h & bufferHeightMask) != 0) {
    h = (h & ~bufferHeightMask) + mBufferHeightAlignment;
  }
}

// Adjust crop rectangle to comply with crop size and alignment restrictions
void BufferFormatConfig::AdjustCropSize(uint32_t bw, uint32_t bh, float& w,
                                        float& h) const {
  if (mCropAlignment > 0.0) {
    float wtrunc = float(mCropAlignment * int(w / mCropAlignment));
    if (w != wtrunc) {
      if (wtrunc == 0) {
        w = uint32_t(wtrunc + mCropAlignment + 0.5);
      } else {
        w = wtrunc;
      }
    }

    float htrunc = float(mCropAlignment * int(h / mCropAlignment));
    if (h != htrunc) {
      if (htrunc == 0) {
        h = uint32_t(htrunc + mCropAlignment + 0.5);
      } else {
        h = htrunc;
      }
    }
  }

  if (w < mMinCropWidth) {
    w = mMinCropWidth;
  }

  if (h < mMinCropHeight) {
    h = mMinCropHeight;
  }

  if (w > bw) {
    w = bw;
  }

  if (h > bh) {
    h = bh;
  }
}

// Adjust crop rectangle to comply with crop size and alignment restrictions
void BufferFormatConfig::AdjustCrop(uint32_t bw, uint32_t bh, float& l,
                                    float& t, float& w, float& h) const {
  if (mCropAlignment > 0.0) {
    float ltrunc = float(mCropAlignment * int(l / mCropAlignment));
    l = ltrunc;

    float ttrunc = float(mCropAlignment * int(t / mCropAlignment));
    t = ttrunc;
  }

  if (l > bw - mMinCropWidth) {
    l = bw - mMinCropWidth;
  }

  if (t > bh - mMinCropHeight) {
    t = bh - mMinCropHeight;
  }

  AdjustCropSize(bw, bh, w, h);
}

BufferFormatConfigManager::BufferFormatConfigManager() {
}

// Adjust display frame to comply with the min width & height
void BufferFormatConfigManager::AdjustDisplayFrame(uint32_t format,
                                                   hwcomposer::HwcRect<int>& r,
                                                   uint32_t displayWidth,
                                                   uint32_t displayHeight) {
  if (find(format) != end()) {
    const BufferFormatConfig& cfg =  at(format);
    cfg.AdjustDisplayFrame(r, displayWidth, displayHeight);
  } else {
    mDeflt.AdjustDisplayFrame(r, displayWidth, displayHeight);
  }
}

// Adjust buffer size to comply with the min width & height, and alignment (i.e.
// whether odd values are permitted)
void BufferFormatConfigManager::AdjustBufferSize(uint32_t format, uint32_t& w,
                                                 uint32_t& h) {

  if (find(format) != end()) {
    const BufferFormatConfig& cfg = at(format);
    cfg.AdjustBufferSize(w, h);
  } else {
    mDeflt.AdjustBufferSize(w, h);
  }
}

void BufferFormatConfigManager::AdjustCropSize(uint32_t format, uint32_t bw,
                                               uint32_t bh, float& w,
                                               float& h) {
  if (find(format) != end()) {
    const BufferFormatConfig& cfg = at(format);
    cfg.AdjustCropSize(bw, bh, w, h);
  } else {
    mDeflt.AdjustCropSize(bw, bh, w, h);
  }
}

void BufferFormatConfigManager::AdjustCrop(uint32_t format, uint32_t bw,
                                           uint32_t bh, float& l, float& t,
                                           float& w, float& h) {
  if (find(format) != end()) {
    const BufferFormatConfig& cfg = at(format);
    cfg.AdjustCrop(bw, bh, l, t, w, h);
  } else {
    mDeflt.AdjustCrop(bw, bh, l, t, w, h);
  }
}

// Define parameters to be used when no configuration is present for the
// selected format.
void BufferFormatConfigManager::SetDefault(const BufferFormatConfig& cfg) {
  mDeflt = cfg;
}

}  // namespace Hwch
