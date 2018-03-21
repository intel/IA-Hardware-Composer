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

#include "HwchPattern.h"
#include "HwchLayer.h"
#include "HwchSystem.h"
#include "HwchPngImage.h"

#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include "png.h"

#define BUFFER_DEBUG 0

///////////////////////////////////////////////// SPixelWord
/////////////////////////////////////////////
// "Word of pixels" abstraction
Hwch::SPixelWord::SPixelWord() {
  mBytesPerPixel = 4;
  mPixelsPerWord32 = 1;
  mWord32 = 0;
  mNv12ChromaWord32 = 0;
  mYv12VWord32 = 0;
  mYv12UWord32 = 0;
}

Hwch::SPixelWord::SPixelWord(uint32_t colour, uint32_t format) {
  // set pixel format
  switch (format) {
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ARGB8888:
      mBytesPerPixel = 4;
      break;
    case DRM_FORMAT_RGB888:
      mBytesPerPixel = 3;
      break;
    case DRM_FORMAT_RGB565:
      mBytesPerPixel = 2;
      break;
    case DRM_FORMAT_YVU420:
      // N.B. NV12 is a complicated format with a total memory usage of 1.5
      // bytes per pixel.
      // However, in the luma space, it uses exactly one byte per pixel, which
      // is what this means.
      mBytesPerPixel = 1;
      break;
    case DRM_FORMAT_YUYV:
      mBytesPerPixel = 2;
      break;
    default:
      printf("UNSUPPORTED PIXEL FORMAT %d\n", format);
      ALOG_ASSERT(0);
  }

  mPixelsPerWord32 = 4 / mBytesPerPixel;
  mWord32 = 0;

  if (GetPixelBytes(colour, format)) {
    printf("GetPixelBytes - UNSUPPORTED PIXEL FORMAT %d\n", format);
    ALOG_ASSERT(0);
  }

  HWCLOGD_COND(eLogHarness, "Colour: %08x mBytesPerPixel: %d Pixel Word: %08x",
               colour, mBytesPerPixel, mWord32);

  mChroma[1] = mChroma[0];
}

uint32_t Hwch::SPixelWord::GetPixelBytes(uint32_t colour, uint32_t format) {
  uint32_t result = 0;

  uint32_t R = ((colour >> 24) & 0xFF);
  uint32_t G = ((colour >> 16) & 0xFF);
  uint32_t B = ((colour >> 8) & 0xFF);
  uint32_t A = colour & 0xFF;

  uint32_t y = ((65 * R + 128 * G + 24 * B + 128) >> 8) + 16;      // Y
  uint32_t v = (((112 * R - 93 * G - 18 * B + 128) >> 8) + 128);   // V-Cr
  uint32_t u = (((-37 * R - 74 * G + 112 * B + 128) >> 8) + 128);  // U-Cb

  switch (format) {
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888: {
      mBytes[0] = R;
      mBytes[1] = G;
      mBytes[2] = B;
      mBytes[3] = A;

      break;
    }
    case DRM_FORMAT_ARGB8888: {
      mBytes[0] = B;
      mBytes[1] = G;
      mBytes[2] = R;
      mBytes[3] = A;

      break;
    }
    case DRM_FORMAT_RGB565: {
      uint32_t red = ((colour >> 24) & 0xFF) >> 3;
      uint32_t green = ((colour >> 16) & 0xFF) >> 2;
      uint32_t blue = ((colour >> 8) & 0xFF) >> 3;

      mBytes[0] = ((green & 3) << 5) | blue;
      mBytes[1] = red << 3 | ((green >> 3) & 3);

      mBytes[2] = mBytes[0];
      mBytes[3] = mBytes[1];

      break;
    }
    case DRM_FORMAT_RGB888: {
      mBytes[0] = ((colour >> 24) & 0xFF);
      mBytes[1] = ((colour >> 16) & 0xFF);
      mBytes[2] = ((colour >> 8) & 0xFF);

      break;
    }
    case DRM_FORMAT_YVU420: {
      mBytes[0] = y;
      mBytes[3] = mBytes[2] = mBytes[1] = mBytes[0];
      mChroma[0].v = v;  // V-Cr
      mChroma[0].u = u;  // U-Cb

      mVBytes[0] = mVBytes[1] = mVBytes[2] = mVBytes[3] = v;
      mUBytes[0] = mUBytes[1] = mUBytes[2] = mUBytes[3] = u;

      HWCLOGD_COND(eLogHarness, "\t Y: %x V-Cr: %x U-Cb: %x", y, v, u);
      break;
    }
    case DRM_FORMAT_YUYV: {
      // Two other choices from wikipedia
      // Doesn't seem to make a lot of difference
      // float r = R; float g = G; float b = B;
      // y = 0.299 * r + 0.587 * g + 0.114 * b;
      // u = -0.147 * r - 0.289 * g + 0.436 * b;
      // v = 0.615 * r -0.515 * g -0.1 * b;

      // y = 0.299 * r + 0.587 * g + 0.114 * b;
      // u = -0.169 * r - 0.331 * g + 0.499 * b + 128;
      // v = 0.499 * r -0.418 * g -0.0813 * b + 128;

      mBytes[0] = y;
      mBytes[1] = u;
      mBytes[2] = y;
      mBytes[3] = v;

      HWCLOGD_COND(eLogHarness, "\t Y: %x V-Cr: %x U-Cb: %x", y, v, u);
      break;
    }
    default: {
      HWCERROR(eCheckInternalError, "Color Space %d not supported yet", format);
      result = 1;
    }
  }

  return result;
}

////////////////////////////////////////////////////// Abstract Pattern class
////////////////////////////////////
Hwch::Pattern::Pattern(float updateFreq)
    : mUpdatedSinceFBComp(false), mMilliFramesToUpdate(0) {
  SetUpdateFreq(updateFreq);
  mNextUpdateTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

Hwch::Pattern::~Pattern() {
}

void Hwch::Pattern::Init() {
}

void Hwch::Pattern::Advance() {
}

void Hwch::Pattern::SetUpdateFreq(float updateFreq) {
  mUpdateFreq = updateFreq;
  if (mUpdateFreq < 0.0000001) {
    // Basically never update
    mUpdatePeriodNs = int64_t(100000) * int64_t(1000000000);
    mUpdatePeriodMilliFrames = INT_MAX;
  } else {
    mUpdatePeriodNs = 1000000000.0 / updateFreq;
    mUpdatePeriodMilliFrames = 60000 / updateFreq;
  }

  if (!Hwch::System::getInstance().IsUpdateRateFixed()) {
    mUpdatePeriodMilliFrames = 0;
  }
}

float Hwch::Pattern::GetUpdateFreq() {
  return mUpdateFreq;
}

uint64_t Hwch::Pattern::GetUpdatePeriodNs() {
  return mUpdatePeriodNs;
}

bool Hwch::Pattern::FrameNeedsUpdate() {
  if (mUpdatePeriodMilliFrames > 0) {
    if (mMilliFramesToUpdate <= 0) {
      mMilliFramesToUpdate += mUpdatePeriodMilliFrames;
      return true;
    } else {
      mMilliFramesToUpdate -= 1000;
      return false;
    }
  } else {
    int64_t currentTime = systemTime(SYSTEM_TIME_MONOTONIC);
    if (currentTime > mNextUpdateTime) {
      mNextUpdateTime += mUpdatePeriodNs;
      return true;
    } else {
      return false;
    }
  }
}

void Hwch::Pattern::ForceUpdate() {
}

void Hwch::Pattern::SetUpdatedSinceLastFBComp() {
  mUpdatedSinceFBComp = true;
}

bool Hwch::Pattern::IsUpdatedSinceLastFBComp() {
  return mUpdatedSinceFBComp;
}

void Hwch::Pattern::ClearUpdatedSinceLastFBComp() {
  mUpdatedSinceFBComp = false;
}

bool Hwch::Pattern::IsAllTransparent() {
  return false;
}

//////////////////////////////////// FramebufferTargetPtn
////////////////////////////////////////////////
Hwch::FramebufferTargetPtn::FramebufferTargetPtn()
    : SolidColourPtn(
          eBlack)  // Background colour before overwriting with composition
{
}

Hwch::FramebufferTargetPtn::~FramebufferTargetPtn() {
}

bool Hwch::FramebufferTargetPtn::FrameNeedsUpdate() {
  // Never update frame before the "Prepare"
  return false;
}

//////////////////////////////////// SolidColourPtn
////////////////////////////////////////////////
Hwch::SolidColourPtn::SolidColourPtn(uint32_t colour) {
  mColour = colour;
}

Hwch::SolidColourPtn::~SolidColourPtn() {
}

int Hwch::SolidColourPtn::Fill(HWCNativeHandle buf,
                               const hwcomposer::HwcRect<int>& rect,
                               uint32_t& bufferParam) {
  HWCVAL_UNUSED(bufferParam);

  void* data = 0;
  unsigned char* chromaStart = 0;

  uint32_t format = buf->meta_data_.format_;
  mPixel = SPixelWord(mColour, format);
  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;
  uint32_t stride;
  uint32_t cstride = 0;

  // Fill gralloc buffer
  data = Hwch::System::getInstance().bufferHandler->Map(
      buf, left, top, width, height, &stride, &data, 0);
  stride = buf->meta_data_.width_;

  HWCLOGD_IF(BUFFER_DEBUG, "FillBuffer: stride=%d\n", stride);
  HWCLOGD_IF(BUFFER_DEBUG, "FillBuffer: height=%d\n", buf->meta_data_.height_);
  HWCLOGD_IF(BUFFER_DEBUG, "FillBuffer: fillValue=0x%.8x\n", mPixel);

  if (data) {
    uint8_t* lineStart = (uint8_t*)data + stride * mPixel.mBytesPerPixel * top;

    for (uint32_t y = 0; y < height; y++) {
      uint8_t* data8 = lineStart + mPixel.mBytesPerPixel * left;

      if (mPixel.mBytesPerPixel != 3) {
        uint32_t x = 0;
        uint32_t* data32 = (uint32_t*)data8;

        for (uint32_t px = 0; px < width; px += mPixel.mPixelsPerWord32) {
          data32[x++] = mPixel.mWord32;
        }
      } else {
        uint32_t x = 0;
        for (uint32_t px = 0; px < width; ++px) {
          data8[x++] = mPixel.mBytes[0];
          data8[x++] = mPixel.mBytes[1];
          data8[x++] = mPixel.mBytes[2];
        }
      }

      lineStart += stride * mPixel.mBytesPerPixel;
    }

    Hwch::System::getInstance().bufferHandler->UnMap(buf, &data);
  } else {
    HWCERROR(eCheckInternalError, "Error locking GraphicBuffer to fill %08x\n",
             mPixel);
    return 1;
  }
  return 0;
}

bool Hwch::SolidColourPtn::IsAllTransparent() {
  return (mColour == 0);
}

////////////////////////////////////////////// HorizontalLinePtn
///////////////////////////////////////////
Hwch::HorizontalLinePtn::HorizontalLinePtn(float mUpdateFreq, uint32_t fgColour,
                                           uint32_t bgColour)
    : Hwch::Pattern(mUpdateFreq),
      mFgColour(fgColour),
      mBgColour(bgColour),
      mLine(0) {
}

Hwch::HorizontalLinePtn::~HorizontalLinePtn() {
}

void Hwch::HorizontalLinePtn::FillLumaLine(unsigned char* data, uint32_t row,
                                           uint32_t stride, uint32_t left,
                                           uint32_t width, SPixelWord pixel) {
  unsigned char* lineStart = data + row * pixel.mBytesPerPixel * stride;
  unsigned char* ptr = lineStart + left * pixel.mBytesPerPixel;

  switch (mFgPixel.mBytesPerPixel) {
    case 4:
      for (uint32_t px = 0; px < width; ++px) {
        *((uint32_t*)ptr) = pixel.mWord32;
        ptr += 4;
      }
      break;

    case 3:
      for (uint32_t px = 0; px < width; ++px) {
        *ptr++ = pixel.mBytes[0];
        *ptr++ = pixel.mBytes[1];
        *ptr++ = pixel.mBytes[2];
      }
      break;

    case 2:
      for (uint32_t px = 0; px < width; px += 2) {
        *((uint32_t*)ptr) = pixel.mWord32;
        ptr += 4;
      }
      break;

    case 1:
      for (uint32_t px = 0; px < width; px += 4) {
        *((uint32_t*)ptr) = pixel.mWord32;
        ptr += 4;
      }
      break;
  }
}

void Hwch::HorizontalLinePtn::FillChromaLineNV12(unsigned char* chromaData,
                                                 uint32_t row, uint32_t stride,
                                                 uint32_t left, uint32_t width,
                                                 SPixelWord pixel) {
  unsigned char* lineStart = chromaData + stride * (row / 2);

  // Align chroma pixels which are twice the size of luma pixels in each axis
  unsigned char* ptr = lineStart + (left & 0xfffffffe);

  for (uint32_t px = 3; px < width; px += 4) {
    *((uint32_t*)ptr) = pixel.mNv12ChromaWord32;
    ptr += 4;
  }
}

void Hwch::HorizontalLinePtn::FillChromaULineYV12(unsigned char* chromaData,
                                                  uint32_t row, uint32_t stride,
                                                  uint32_t left, uint32_t width,
                                                  SPixelWord pixel) {
  unsigned char* lineStart = chromaData + stride * (row / 2);

  // Align chroma pixels which are twice the size of luma pixels in each axis
  unsigned char* ptr = lineStart + (left & 0xfffffffe);

  for (uint32_t px = 7; px < width; px += 8) {
    *((uint32_t*)ptr) = pixel.mYv12UWord32;
    ptr += 4;
  }
}

void Hwch::HorizontalLinePtn::FillChromaVLineYV12(unsigned char* chromaData,
                                                  uint32_t row, uint32_t stride,
                                                  uint32_t left, uint32_t width,
                                                  SPixelWord pixel) {
  unsigned char* lineStart = chromaData + stride * (row / 2);

  // Align chroma pixels which are twice the size of luma pixels in each axis
  unsigned char* ptr = lineStart + (left & 0xfffffffe);
  for (uint32_t px = 7; px < width; px += 8) {
    *((uint32_t*)ptr) = pixel.mYv12VWord32;
    ptr += 4;
  }
}

int Hwch::HorizontalLinePtn::Fill(HWCNativeHandle buf,
                                  const hwcomposer::HwcRect<int>& rect,
                                  uint32_t& bufferParam) {
  void* data = 0;
  unsigned char* chromaStart = 0;
  unsigned char* ustart = 0;
  unsigned char* vstart = 0;

  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;
  uint32_t stride;
  uint32_t cstride = 0;
  android_ycbcr ycbcr = android_ycbcr();

  if ((height == 0) || (width == 0)) {
    HWCLOGD_COND(eLogHarness, "HorizontalLinePtn::Fill aborted %p %dx%d",
                 buf->handle_, width, height);
    return 0;
  }

  // Fill gralloc buffer
  uint32_t format = buf->meta_data_.format_;
  if ((format == DRM_FORMAT_YVU420)) {
    data = Hwch::System::getInstance().bufferHandler->Map(
        buf, left, top, width, height, &stride, &data, 0);
    data = ycbcr.y;
    chromaStart = (unsigned char*)ycbcr.cb;
    ustart = (unsigned char*)ycbcr.cb;
    vstart = (unsigned char*)ycbcr.cr;
    stride = ycbcr.ystride;
    cstride = ycbcr.cstride;
    HWCLOGV_COND(eLogHarness,
                 "Starting %s fill, handle %p %dx%d, ustart=%p, vstart=%p, "
                 "stride=%d, cstride=%d",
                 (format == DRM_FORMAT_YVU420) ? "YVU420_ANDROID" : "NV12",
                 buf->handle_, width, height, ustart, vstart, stride, cstride);
  } else {
    data = Hwch::System::getInstance().bufferHandler->Map(
        buf, left, top, width, height, &stride, &data, 0);
    stride = buf->meta_data_.width_;
  }

  if (data == 0) {
    HWCERROR(eCheckInternalError, "Gralloc lock failed. ");
    return 1;
  }

  mFgPixel = SPixelWord(mFgColour, format);
  mBgPixel = SPixelWord(mBgColour, format);

  if ((mLine + 4) > height) {
    mLine = 0;
  }

  if (bufferParam == HWCH_BUFFERPARAM_UNDEFINED) {
    // loop to set the luminance component of the surface
    for (uint32_t row = 0; row < height; ++row) {
      SPixelWord currentPixel;

      if (row >= mLine && row < mLine + 4) {
        currentPixel = mFgPixel;
      } else {
        currentPixel = mBgPixel;
      }

      FillLumaLine((unsigned char*)data, row, stride, left, width,
                   currentPixel);
    }

    // loop to set the color value only if NV12 case
    // Note that px is measured in luma pixels, then it still needs to
    // progress by 4 even for the chroma values

    if (format == DRM_FORMAT_YVU420) {
      SPixelWord currentPixel;

      for (uint32_t row = 0; row < (height - 1); row += 2) {
        if (row >= mLine && row < mLine + 4) {
          currentPixel = mFgPixel;
        } else {
          currentPixel = mBgPixel;
        }

        FillChromaVLineYV12(vstart, row, cstride, left, width, currentPixel);
        FillChromaULineYV12(ustart, row, cstride, left, width, currentPixel);
      }
    }
  } else {
    uint32_t oldLine = bufferParam;

    for (uint32_t row = oldLine; row < (oldLine + 4); ++row) {
      FillLumaLine((unsigned char*)data, row, stride, left, width, mBgPixel);
    }

    for (uint32_t row = mLine; row < (mLine + 4); ++row) {
      FillLumaLine((unsigned char*)data, row, stride, left, width, mFgPixel);
    }

    if (format == DRM_FORMAT_YVU420) {
      for (uint32_t row = oldLine; row < (oldLine + 4); row += 2) {
        FillChromaVLineYV12(vstart, row, cstride, left, width, mBgPixel);
        FillChromaULineYV12(ustart, row, cstride, left, width, mBgPixel);
      }

      for (uint32_t row = mLine; row < (mLine + 4); row += 2) {
        FillChromaVLineYV12(vstart, row, cstride, left, width, mFgPixel);
        FillChromaULineYV12(ustart, row, cstride, left, width, mFgPixel);
      }
    }
  }

  Hwch::System::getInstance().bufferHandler->UnMap(buf, &data);
  bufferParam = mLine;

  return 0;
}

void Hwch::HorizontalLinePtn::Advance() {
  mLine += min(max(60.0f / mUpdateFreq, 1.0f), 8.0f);
}

//////////////////////////////////// PngPtn
////////////////////////////////////////////////
Hwch::PngPtn::PngPtn(float updateFreq, uint32_t lineColour)
    : Hwch::HorizontalLinePtn(updateFreq, lineColour, lineColour),
      mRowPointers(0) {
}

Hwch::PngPtn::~PngPtn() {
}

void Hwch::PngPtn::Set(Hwch::PngImage& image) {
  mpImage = &image;
  mRowPointers = image.GetRowPointers();
}

void Hwch::PngPtn::Set(std::unique_ptr<Hwch::PngImage> spImage) {
  mspImage.reset(spImage.get());
  Set(*spImage);
}

void Hwch::PngPtn::FillLineFromImage(unsigned char* data, uint32_t row,
                                     uint32_t stride, uint32_t left,
                                     uint32_t width) {
  unsigned char* lineStart = data + row * mFgPixel.mBytesPerPixel * stride;
  unsigned char* ptr = lineStart + left * mFgPixel.mBytesPerPixel;

  png_byte* rowData = mRowPointers[row];

  memcpy(ptr, rowData, width * mFgPixel.mBytesPerPixel);
}

int Hwch::PngPtn::Fill(HWCNativeHandle buf,
                       const hwcomposer::HwcRect<int>& rect,
                       uint32_t& bufferParam) {
  ALOG_ASSERT(mRowPointers);
  void* data = 0;

  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;
  uint32_t stride = 0;

  // Fill gralloc buffer
  data = Hwch::System::getInstance().bufferHandler->Map(
      buf, left, top, width, height, &stride, &data, 0);

  if (data == 0) {
    HWCERROR(eCheckInternalError, "Gralloc lock failed");
    return 1;
  }

  stride = buf->meta_data_.width_;
  uint32_t format = buf->meta_data_.format_;

  mFgPixel = SPixelWord(mFgColour, format);

  if (mLine > (height - 4)) {
    mLine = 0;
  }

  if (bufferParam == HWCH_BUFFERPARAM_UNDEFINED) {
    // loop to set the luminance component of the surface
    for (uint32_t row = 0; row < height; ++row) {
      if (row >= mLine && row < mLine + 4) {
        FillLumaLine((unsigned char*)data, row, stride, left, width, mFgPixel);
      } else {
        FillLineFromImage((unsigned char*)data, row, stride, left, width);
      }
    }

    // NV12 not supported yet
  } else {
    uint32_t oldLine = bufferParam;

    for (uint32_t row = oldLine; row < (oldLine + 4); ++row) {
      FillLineFromImage((unsigned char*)data, row, stride, left, width);
    }

    for (uint32_t row = mLine; row < (mLine + 4); ++row) {
      FillLumaLine((unsigned char*)data, row, stride, left, width, mFgPixel);
    }
  }

  Hwch::System::getInstance().bufferHandler->UnMap(buf, &data);
  bufferParam = mLine;

  return 0;
}
