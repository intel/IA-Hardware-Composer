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

#include "HwchGlPattern.h"
#include "HwchLayer.h"
#include "HwchSystem.h"
#include "HwchPngImage.h"

#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include "png.h"

#include <platformdefines.h>

#define BUFFER_DEBUG 0

////////////////////////////////////////////////// GlPattern (abstract)
//////////////////////////////////////
Hwch::GlPattern::GlPattern(float updateFreq)
    : Hwch::Pattern(updateFreq),
      mGlInterface(Hwch::System::getInstance().GetGl()) {
}

Hwch::GlPattern::~GlPattern() {
}

///////////////////////////////////////////// HorizontalLineGlPtn
///////////////////////////////////////////
Hwch::HorizontalLineGlPtn::HorizontalLineGlPtn(float mUpdateFreq,
                                               uint32_t fgColour,
                                               uint32_t bgColour)
    : Hwch::GlPattern(mUpdateFreq) {
  mFgColour = fgColour;
  mBgColour = bgColour;
  mLine = 0;
}

Hwch::HorizontalLineGlPtn::~HorizontalLineGlPtn() {
}

int Hwch::HorizontalLineGlPtn::Fill(HWCNativeHandle buf,
                                    const hwcomposer::HwcRect<int>& rect,
                                    uint32_t& bufferParam) {
  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;

  if ((height == 0) || (width == 0)) {
    HWCLOGD_COND(eLogHarness, "HorizontalLineGlPtn::Fill aborted %p %dx%d",
                 buf->handle_, width, height);
    return 0;
  }

  if (mLine > (height - 4)) {
    mLine = 0;
  }

  uint32_t format = buf->meta_data_.format_;
  HWCLOGV_COND(eLogHarness,
               "HorizontalLineGlPtn: %s fill, handle %p %dx%d, mLine=%d",
               FormatToStr(format), buf->handle_, width, height, mLine);

  if (mGlInterface.InitTarget(buf)) {
    mGlInterface.StartFrame();

    mGlInterface.SetClearColour(mBgColour);

    if (bufferParam == HWCH_BUFFERPARAM_UNDEFINED) {
      mGlInterface.Clear(left, top, width, height);
    } else {
      uint32_t oldLine = bufferParam;
      mGlInterface.Clear(left, oldLine, width, 5);
    }

    mGlInterface.SetDrawColour(mFgColour);
    mGlInterface.DrawLine(left, mLine, right, mLine, 5);

    mGlInterface.EndFrame();
    mGlInterface.ReleaseTarget();
  }

  return 0;
}

void Hwch::HorizontalLineGlPtn::Advance() {
  mLine += min(max(60.0f / mUpdateFreq, 1.0f), 8.0f);
}

///////////////////////////////////////////// MatrixGlPtn
///////////////////////////////////////////
Hwch::MatrixGlPtn::MatrixGlPtn(float updateFreq, uint32_t fgColour,
                               uint32_t matrixColour, uint32_t bgColour)
    : Hwch::HorizontalLineGlPtn(updateFreq, fgColour, bgColour) {
  mMatrixColour = matrixColour;
  mLine = 0;
}

Hwch::MatrixGlPtn::~MatrixGlPtn() {
}

int Hwch::MatrixGlPtn::Fill(HWCNativeHandle buf,
                            const hwcomposer::HwcRect<int>& rect,
                            uint32_t& bufferParam) {
  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;

  if ((height == 0) || (width == 0)) {
    HWCLOGD_COND(eLogHarness, "MatrixGlPtn::Fill aborted %p %dx%d", buf->handle_,
                 width, height);
    return 0;
  }

  if (mLine > (height - 4)) {
    mLine = 0;
  }

  uint32_t format = buf->meta_data_.format_;
  HWCLOGV_COND(
      eLogHarness,
      "MatrixGlPtn: %s fill, handle %p %dx%d, mLine=%d, (%d, %d, %d, %d)",
      FormatToStr(format), buf->handle_, width, height, mLine, left, top, right,
      bottom);

  if (mGlInterface.InitTarget(buf)) {
    mGlInterface.StartFrame();

    // If we are just moving the line downwards in the buffer, apply a scissor
    // to prevent
    // unnecessary drawing
    if ((bufferParam != HWCH_BUFFERPARAM_UNDEFINED) && (bufferParam < mLine)) {
      uint32_t oldLine = bufferParam;
      mGlInterface.Scissor(left, oldLine, width, bufferParam - mLine + 5);
    }

    // Clear to the background colour
    mGlInterface.SetClearColour(mBgColour);
    mGlInterface.Clear(left, top, width, height);

    mGlInterface.DisableScissor();

    // Draw the matrix
    mGlInterface.SetDrawColour(mMatrixColour);
    for (uint32_t x = left; x < right; x += 100) {
      mGlInterface.DrawLine(x, 0, x, bottom, 3);
    }

    for (uint32_t y = top; y < bottom; y += 100) {
      mGlInterface.DrawLine(left, y, right, y, 3);
    }

    // Draw the moving line
    mGlInterface.SetDrawColour(mFgColour);
    mGlInterface.DrawLine(left, mLine, right, mLine, 5);

    mGlInterface.DisableScissor();

    // Release the target
    mGlInterface.EndFrame();
    mGlInterface.ReleaseTarget();
  }

  return 0;
}

///////////////////////////////////////////// PngGlPtn
///////////////////////////////////////////
Hwch::PngGlPtn::PngGlPtn() : mpImage(0), mpTexture(0) {
}

Hwch::PngGlPtn::PngGlPtn(float mUpdateFreq, uint32_t lineColour,
                         uint32_t bgColour, bool bIgnore)
    : Hwch::GlPattern(mUpdateFreq),
      mFgColour(lineColour),
      mBgColour(bgColour),
      mIgnore(bIgnore),
      mpImage(0),
      mpTexture(0) {
  mLine = 0;

  mGlInterface.SetIgnoreColour(bgColour);
}

Hwch::PngGlPtn::~PngGlPtn() {
}

void Hwch::PngGlPtn::Set(Hwch::PngImage& image) {
  mpImage = &image;
  mpTexture = mpImage->GetTexture();
}

void Hwch::PngGlPtn::Set(std::unique_ptr<Hwch::PngImage> spImage) {
  mspImage.reset(spImage.get());
  Set(*spImage);
}

int Hwch::PngGlPtn::Fill(HWCNativeHandle buf,
                         const hwcomposer::HwcRect<int>& rect,
                         uint32_t& bufferParam) {
  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;

  if ((height == 0) || (width == 0)) {
    HWCLOGD_COND(eLogHarness, "PngGlPtn::Fill aborted %p %dx%d", buf->handle_,
                 width, height);
    return 0;
  }

  if (mLine > (height - 4)) {
    mLine = 0;
  }

  if (mpTexture) {
    if (mGlInterface.InitTarget(buf)) {
      mGlInterface.StartFrame();

      // If we are just moving the line downwards in the buffer, apply a scissor
      // to prevent
      // unnecessary drawing
      if ((bufferParam != HWCH_BUFFERPARAM_UNDEFINED) &&
          (bufferParam < mLine)) {
        uint32_t oldLine = bufferParam;
        mGlInterface.Scissor(left, oldLine, width, bufferParam - mLine + 5);
      }

      mGlInterface.ApplyTexture(mpTexture, left, top, width, height, mIgnore);

      mGlInterface.SetDrawColour(mFgColour);
      mGlInterface.DrawLine(left, mLine, right, mLine, 5);

      mGlInterface.DisableScissor();

      mGlInterface.EndFrame();
      mGlInterface.ReleaseTarget();
    }
  }

  return 0;
}

void Hwch::PngGlPtn::Advance() {
  mLine += min(max(60.0f / mUpdateFreq, 1.0f), 8.0f);
}

Hwch::ClearGlPtn::ClearGlPtn(float mUpdateFreq, uint32_t fgColour,
                             uint32_t bgColour)
    : Hwch::GlPattern(mUpdateFreq) {
  mFgColour = fgColour;
  mBgColour = bgColour;
  mLine = 0;
}

Hwch::ClearGlPtn::~ClearGlPtn() {
}

int Hwch::ClearGlPtn::Fill(HWCNativeHandle buf,
                           const hwcomposer::HwcRect<int>& rect,
                           uint32_t& bufferParam) {
  HWCVAL_UNUSED(bufferParam);

  uint32_t left = max(0, min((int)buf->meta_data_.width_, rect.left));
  uint32_t top = max(0, min((int)buf->meta_data_.height_, rect.top));
  uint32_t right = max(0, min((int)buf->meta_data_.width_, rect.right));
  uint32_t bottom = max(0, min((int)buf->meta_data_.height_, rect.bottom));
  uint32_t height = bottom - top;
  uint32_t width = right - left;

  if ((height == 0) || (width == 0)) {
    HWCLOGD_COND(eLogHarness, "ClearGlPtn::Fill aborted %p %dx%d", buf->handle_,
                 width, height);
    return 0;
  }

  if (mLine > (height - 4)) {
    mLine = 0;
  }

  if (mGlInterface.InitTarget(buf)) {
    mGlInterface.StartFrame();

    mGlInterface.SetClearColour(mFgColour);
    mGlInterface.Clear(left, top, width, height);

    mGlInterface.SetDrawColour(mFgColour);
    mGlInterface.DrawLine(left, mLine, right, mLine, 5);

    mGlInterface.EndFrame();
    mGlInterface.ReleaseTarget();
  }

  return 0;
}

bool Hwch::ClearGlPtn::IsAllTransparent() {
  return (mFgColour == 0);
}

void Hwch::ClearGlPtn::Advance() {
  mLine += min(max(60.0f / mUpdateFreq, 1.0f), 8.0f);
}
