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

#include "HwchLayerChoice.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

static const float delta = 0.0001;

Hwch::BufferSizeChoice::BufferSizeChoice(uint32_t screenSize, uint32_t minSize,
                                         uint32_t maxSize)
    : mScreenSize(screenSize),
      mMaxSize(maxSize),
      mBufferSizeClassChoice("mBufferSizeClassChoice"),
      mSmallerChoice(minSize, min(screenSize - 1, maxSize),
                     "BufferSize mSmaller"),
      mBiggerChoice(screenSize + 1, mMaxSize, "BufferSize mBigger") {
  HWCLOGV_COND(eLogHarness, "BufferSizeChoice screen %d max %d", screenSize,
               maxSize);
  if (maxSize < screenSize) {
    mBufferSizeClassChoice.Add(eSmallerThanScreen);
  } else if (maxSize == screenSize) {
    mBufferSizeClassChoice.Add(eSmallerThanScreen);
    mBufferSizeClassChoice.Add(eSameAsScreen);
  } else {
    // Make small buffers much more likely than big ones
    mBufferSizeClassChoice.Add(eSmallerThanScreen);
    mBufferSizeClassChoice.Add(eSmallerThanScreen);
    mBufferSizeClassChoice.Add(eSameAsScreen);
    mBufferSizeClassChoice.Add(eBiggerThanScreen);
  }
}

Hwch::BufferSizeChoice::~BufferSizeChoice() {
}

uint32_t Hwch::BufferSizeChoice::Get() {
  uint32_t cls = mBufferSizeClassChoice.Get();

  switch (cls) {
    case eSmallerThanScreen:
      return mSmallerChoice.Get();

    case eSameAsScreen:
      return mScreenSize;

    case eBiggerThanScreen:
      return mBiggerChoice.Get();

    default:
      ALOG_ASSERT(0);
      return 0;
  }

  // TODO: Specific cases for panel fitter which supports up to 2048x2047 buffer
  // size.
}

uint32_t Hwch::BufferSizeChoice::NumChoices() {
  // Number of choice classes
  return 3;
}

Hwch::CropAlignmentChoice::CropAlignmentChoice(uint32_t bufferSize,
                                               float cropSize)
    : mBufferSize(bufferSize),
      mCropSize(cropSize),
      mAlignmentChoice(eMinAligned, eAlignmentMax, "mAlignmentChoice"),
      mOffsetChoice(delta, ((float)bufferSize) - cropSize - delta,
                    "mOffsetChoice") {
  HWCLOGV_COND(eLogHarness, "CropAlignmentChoice bufferSize %d crop %d",
               bufferSize, cropSize);
}

Hwch::CropAlignmentChoice::~CropAlignmentChoice() {
}

float Hwch::CropAlignmentChoice::Get() {
  if (mCropSize > (mBufferSize - delta)) {
    return 0;
  }

  uint32_t alignment = mAlignmentChoice.Get();

  switch (alignment) {
    case eMinAligned:
      return 0;

    case eNotAligned:
      return mOffsetChoice.Get();

    case eMaxAligned:
      return mBufferSize - mCropSize;

    default:
      ALOG_ASSERT(0);
      return 0;
  }
}

uint32_t Hwch::CropAlignmentChoice::NumChoices() {
  return 3;
}

// DisplayFrameChoice
Hwch::DisplayFrameChoice::DisplayFrameChoice(uint32_t screenSize,
                                             float cropSize, uint32_t minSize,
                                             uint32_t maxSize)
    : mScreenSize(screenSize),
      mCropSize(int(cropSize + 0.5)),
      mMinSize(minSize),
      mMaxSize(maxSize),
      mScaleTypeChoice("mScaleTypeChoice"),
      mOverlapChoice("mOverlapChoice") {
  HWCLOGV_COND(eLogHarness, "DisplayFrameChoice screen %d crop %d max %d",
               mScreenSize, mCropSize, mMaxSize);
}

Hwch::DisplayFrameChoice::~DisplayFrameChoice() {
}

Hwch::FullDisplayFrameChoice::FullDisplayFrameChoice(uint32_t screenSize,
                                                     float cropSize,
                                                     uint32_t minSize,
                                                     uint32_t maxSize)
    : DisplayFrameChoice(screenSize, cropSize, minSize, maxSize) {
  mScaleTypeChoice.Add(eNotScaled);
  mScaleTypeChoice.Add(eScaledToMin);
  mScaleTypeChoice.Add(eScaledFullScreen);

  if (mCropSize > (mMinSize + 1)) {
    mScaleTypeChoice.Add(eScaledSmaller);
  }
  if (mCropSize < mMaxSize) {
    mScaleTypeChoice.Add(eScaledBigger);
  }

  mScaleTypeChoice.Add(eScaledHuge);
}

Hwch::FullDisplayFrameChoice::~FullDisplayFrameChoice() {
}

Hwch::OnScreenDisplayFrameChoice::OnScreenDisplayFrameChoice(
    uint32_t screenSize, float cropSize, uint32_t minSize, uint32_t maxSize)
    : DisplayFrameChoice(screenSize, cropSize, minSize, maxSize) {
  if ((mCropSize <= mMaxSize) && (mCropSize > mMinSize)) {
    mScaleTypeChoice.Add(eNotScaled);
  }

  mScaleTypeChoice.Add(eScaledToMin);

  if (maxSize > screenSize) {
    mScaleTypeChoice.Add(eScaledFullScreen);
  }

  if ((mCropSize > (mMinSize + 1)) && (minSize < maxSize)) {
    mScaleTypeChoice.Add(eScaledSmaller);
  }
  if (mCropSize < mMaxSize) {
    mScaleTypeChoice.Add(eScaledBigger);
  }
}

Hwch::OnScreenDisplayFrameChoice::~OnScreenDisplayFrameChoice() {
}

Hwch::Coord<int32_t> Hwch::DisplayFrameChoice::Get() {
  ALOG_ASSERT(mScreenSize);
  int32_t dfSize;
  uint32_t scaleType = mScaleTypeChoice.Get();

  switch (scaleType) {
    case eNotScaled: {
      HWCLOGV_COND(eLogHarness,
                   "Hwch::DisplayFrameChoice::Get NotScaled crop %d",
                   mCropSize);
      dfSize = mCropSize;
      break;
    }
    case eScaledToMin: {
      HWCLOGV_COND(eLogHarness, "Hwch::DisplayFrameChoice::Get ScaledToMin");
      dfSize = mMinSize;
      break;
    }
    case eScaledFullScreen: {
      HWCLOGV_COND(eLogHarness,
                   "Hwch::DisplayFrameChoice::Get ScaledFullScreen %d",
                   mScreenSize);
      dfSize = mScreenSize;
      break;
    }
    case eScaledSmaller: {
      HWCLOGV_COND(
          eLogHarness,
          "Hwch::DisplayFrameChoice::Get ScaledSmaller crop %d min %d max %d",
          mCropSize, mMinSize, mMaxSize);
      dfSize = Choice(mMinSize + 1, min(mCropSize - 1, mMaxSize)).Get();
      break;
    }
    case eScaledBigger: {
      HWCLOGV_COND(eLogHarness,
                   "Hwch::DisplayFrameChoice::Get ScaledBigger crop %d max %d",
                   mCropSize, mMaxSize);
      dfSize = Choice(mCropSize + 1, mMaxSize).Get();
      break;
    }
    case eScaledHuge: {
      HWCLOGV_COND(eLogHarness, "Hwch::DisplayFrameChoice::Get ScaledHuge %d",
                   mMaxSize);
      dfSize = mMaxSize;
      break;
    }
    default: {
      ALOG_ASSERT(0);
      dfSize = 0;
      return 0;
    }
  }

  mDfSize = dfSize;
  return Scaled(dfSize, mScreenSize);
}

Hwch::Coord<int32_t> Hwch::DisplayFrameChoice::GetOffset() {
  ALOG_ASSERT(mScreenSize);
  MultiChoice<OverlapType> overlap("overlap");

  if (mScreenSize < mMaxSize) {
    if (mDfSize > mScreenSize) {
      overlap.Add(eOverlappingBothSides);
    }

    if (mDfSize > 1) {
      overlap.Add(eOverlappingMinOnly);
      overlap.Add(eOverlappingMaxOnly);
    }
  }

  overlap.Add(eAlignedMin);
  overlap.Add(eAlignedMax);

  if (mScreenSize > mDfSize + 1) {
    overlap.Add(eNotOverlapping);
  }

  switch (overlap.Get()) {
    case eOverlappingBothSides: {
      HWCLOGV_COND(
          eLogHarness,
          "Hwch::DisplayFrameChoice OverlappingBothSides df %d screen %d",
          mDfSize, mScreenSize);
      int32_t overlapRange = mDfSize - mScreenSize;
      return Scaled(
          Choice(-overlapRange, -1, "Scaled eOverlappingBothSides").Get(),
          mScreenSize);
    }
    case eOverlappingMinOnly: {
      HWCLOGV_COND(
          eLogHarness,
          "Hwch::DisplayFrameChoice OverlappingMinOnly df %d screen %d",
          mDfSize, mScreenSize);
      int32_t smallestOffset = 1;

      if (mDfSize > mScreenSize) {
        smallestOffset = mDfSize - mScreenSize + 1;
      }

      return Scaled(Choice(-mDfSize + 1, -smallestOffset,
                           "Scaled eOverlappingMinOnly").Get(),
                    mScreenSize);
    }
    case eOverlappingMaxOnly: {
      HWCLOGV_COND(
          eLogHarness,
          "Hwch::DisplayFrameChoice OverlappingMaxOnly df %d screen %d",
          mDfSize, mScreenSize);
      int32_t smallestOffset = 1;

      if (mDfSize > mScreenSize) {
        smallestOffset = mDfSize - mScreenSize + 1;
      }

      return Scaled(
          Choice(mScreenSize - mDfSize + 1, mScreenSize - smallestOffset,
                 "Scaled eOverlappingMaxOnly").Get(),
          mScreenSize);
    }
    case eAlignedMin: {
      HWCLOGV_COND(eLogHarness,
                   "Hwch::DisplayFrameChoice AlignedMin df %d screen %d",
                   mDfSize, mScreenSize);
      return Scaled(0, mScreenSize);
    }
    case eAlignedMax: {
      HWCLOGV_COND(eLogHarness,
                   "Hwch::DisplayFrameChoice AlignedMax df %d screen %d",
                   mDfSize, mScreenSize);
      return Scaled(mScreenSize - mDfSize, mScreenSize);
    }
    case eNotOverlapping: {
      HWCLOGV_COND(eLogHarness,
                   "Hwch::DisplayFrameChoice NotOverlapping df %d screen %d",
                   mDfSize, mScreenSize);
      return Scaled(
          Choice(1, mScreenSize - mDfSize - 1, "Scaled eNotOverlapping").Get(),
          mScreenSize);
    }
    default: {
      ALOG_ASSERT(0);
      return 0;
    }
  }
}

uint32_t Hwch::DisplayFrameChoice::NumChoices() {
  return mScaleTypeChoice.NumChoices() * 5;
}

Hwch::PanelFitterScaleChoice::PanelFitterScaleChoice()
    : mModeChoice(0, eModeMax - 1, "PanelFitterScale mModeChoice"),
      mScalingChoice(0, eScalingMax - 1, "PanelFitterScale mScalingChoice"),
      mScreenWidth(0),
      mScreenHeight(0),
      mMinScaleFactor(HWCH_PANELFITVAL_MIN_SCALE_FACTOR),
      mMinPFScaleFactor(HWCH_PANELFITVAL_MIN_PF_SCALE_FACTOR),
      mMaxPFScaleFactor(HWCH_PANELFITVAL_MAX_PF_SCALE_FACTOR),
      mMaxScaleFactor(HWCH_PANELFITVAL_MAX_SCALE_FACTOR) {
}

void Hwch::PanelFitterScaleChoice::SetScreenSize(uint32_t w, uint32_t h) {
  mScreenWidth = w;
  mScreenHeight = h;
}

void Hwch::PanelFitterScaleChoice::SetLimits(float minScaleFactor,
                                             float minPFScaleFactor,
                                             float maxScaleFactor,
                                             float maxPFScaleFactor) {
  mMinScaleFactor = minScaleFactor;
  mMinPFScaleFactor = minPFScaleFactor;
  mMaxScaleFactor = maxScaleFactor;
  mMaxPFScaleFactor = maxPFScaleFactor;
  HWCLOGD("PanelFitterScaleChoice: factor limits set to %f,%f,%f,%f",
          (double)minScaleFactor, (double)minPFScaleFactor,
          (double)maxScaleFactor, (double)maxPFScaleFactor);
}

float Hwch::PanelFitterScaleChoice::Get() {
  ALOG_ASSERT(mScreenWidth);
  ALOG_ASSERT(mScreenHeight);

  int mode = mModeChoice.Get();
  float xscale;

  switch (mode) {
    case eAuto: {
      xscale = mYscale = GetAValue();
      mPFMinX = mPFMinY = INT_MIN;
      mPFMaxX = mPFMaxY = INT_MAX;
      break;
    }

    case eLetterbox: {
      xscale = mYscale = GetAValue();
      mPFMinX = INT_MIN;
      mPFMaxX = INT_MAX;
      mPFMinY = mLetterboxYOffset;
      mPFMaxY = mScreenHeight - mLetterboxYOffset;
      break;
    }

    case ePillarbox: {
      xscale = mYscale = GetAValue();
      mPFMinX = mPillarboxXOffset;
      mPFMaxX = mScreenWidth - mPillarboxXOffset;
      mPFMinY = INT_MIN;
      mPFMaxY = INT_MAX;
      break;
    }

    default: { ALOG_ASSERT(0); }
  }

  return xscale;
}

float Hwch::PanelFitterScaleChoice::GetY() {
  return mYscale;
}

float Hwch::PanelFitterScaleChoice::GetAValue() {
  const float scalingDelta = 0.05;
  float scaleFactor = 1.0;
  switch (mScalingChoice.Get()) {
    case eMuchTooSmall: {
      scaleFactor = mMinScaleFactor;
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eMuchTooSmall %f",
                   scaleFactor);
      break;
    }

    case eTooSmall: {
      scaleFactor = FloatChoice(mMinScaleFactor + scalingDelta,
                                mMinPFScaleFactor - scalingDelta,
                                "PanelFitterScale eTooSmall").Get();
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eTooSmall %f",
                   scaleFactor);
      break;
    }

    case eSmallestSupported: {
      scaleFactor =
          mMinPFScaleFactor + scalingDelta;  // If we choose mMinPfScaleFactor
                                             // exactly, we are unlikely to get
                                             // global scaling owing to rounding
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eSmallestSupported %f",
                   scaleFactor);
      break;
    }

    case eSmaller: {
      scaleFactor =
          FloatChoice(mMinPFScaleFactor + scalingDelta, 1.0 - scalingDelta,
                      "PanelFitterScale eSmaller").Get();
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eSmaller %f",
                   scaleFactor);
    }

    case eUnity: {
      scaleFactor = 1.0;
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eUnity %f",
                   scaleFactor);
      break;
    }

    case eBigger: {
      scaleFactor =
          FloatChoice(1.0 + scalingDelta, mMaxPFScaleFactor - scalingDelta,
                      "PanelFitterScale eBigger").Get();
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eBigger %f",
                   scaleFactor);
      break;
    }

    case eTooBig: {
      scaleFactor =
          FloatChoice(mMaxPFScaleFactor + scalingDelta, mMaxScaleFactor,
                      "PanelFitterScale eTooBig").Get();
      HWCLOGD_COND(eLogHarness, "PanelFitterScaleChoice eTooBig %f",
                   scaleFactor);
      break;
    }

    default: { ALOG_ASSERT(0); }
  }

  return scaleFactor;
}

// provides constraints within which display frames must be generated
void Hwch::PanelFitterScaleChoice::GetDisplayFrameBounds(int32_t& minX,
                                                         int32_t& minY,
                                                         int32_t& maxX,
                                                         int32_t& maxY) {
  minX = mPFMinX;
  minY = mPFMinY;
  maxX = mPFMaxX;
  maxY = mPFMaxY;
}

uint32_t Hwch::PanelFitterScaleChoice::NumChoices() {
  return ((uint32_t)eScalingMax) * ((uint32_t)eModeMax);
}

const int Hwch::PanelFitterScaleChoice::mPillarboxXOffset = 50;
const int Hwch::PanelFitterScaleChoice::mLetterboxYOffset = 50;

Hwch::AlphaChoice::AlphaChoice()
    : mPlaneAlphaClassChoice(eTransparent, ePlaneAlphaMax,
                             "mPlaneAlphaClassChoice"),
      mValueChoice(1, 254, "mValueChoice") {
}

Hwch::AlphaChoice::~AlphaChoice() {
}

uint32_t Hwch::AlphaChoice::Get() {
  switch (mPlaneAlphaClassChoice.Get()) {
    case eTransparent:
      return 0;

    case eTranslucent:
      return mValueChoice.Get();

    case eOpaque:
      return 255;

    default:
      ALOG_ASSERT(0);
      return 0;
  }
}

uint32_t Hwch::AlphaChoice::NumChoices() {
  return 3;
}
