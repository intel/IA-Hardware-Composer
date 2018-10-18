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

#ifndef __HwchGlPattern_h__
#define __HwchGlPattern_h__

#include <utils/Vector.h>
#include <platformdefines.h>
#include <hardware/hwcomposer2.h>

#include "HwchDefs.h"
#include "HwchPattern.h"
#include "HwchPngImage.h"

// includes for SSIM and libpng
#include "png.h"
#include "HwchGlInterface.h"

namespace Hwch {
class PngImage;

class GlPattern : public Pattern {
 public:
  GlPattern(float updateFreq = 0);
  virtual ~GlPattern();

 protected:
  GlInterface& mGlInterface;
};

class HorizontalLineGlPtn : public GlPattern {
 public:
  HorizontalLineGlPtn();
  HorizontalLineGlPtn(float updateFreq, uint32_t fgColour, uint32_t bgColour);
  virtual ~HorizontalLineGlPtn();

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);
  virtual void Advance();

 protected:
  uint32_t mFgColour;
  uint32_t mBgColour;

  uint32_t mLine;
};

class MatrixGlPtn : public HorizontalLineGlPtn {
 public:
  MatrixGlPtn();
  MatrixGlPtn(float updateFreq, uint32_t fgColour, uint32_t matrixColour,
              uint32_t bgColour);
  virtual ~MatrixGlPtn();

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);

 protected:
  uint32_t mMatrixColour;
};

class PngGlPtn : public GlPattern {
 public:
  PngGlPtn();
  PngGlPtn(float updateFreq, uint32_t lineColour, uint32_t bgColour = 0,
           bool bIgnore = true);
  virtual ~PngGlPtn();

  // Connect to an image, ownership of the image stays with the caller
  void Set(Hwch::PngImage& image);

  // Connect to an image, we get ownership
  void Set(std::unique_ptr<Hwch::PngImage> spImage);

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);
  virtual void Advance();

 protected:
  uint32_t mFgColour;
  uint32_t mBgColour;
  bool mIgnore;

  uint32_t mLine;
  PngImage* mpImage;

  // Only for ownership
  std::unique_ptr<Hwch::PngImage> mspImage;

  // Convenience pointer to the texture in mpImage.
  // PngGlPtn does NOT own this texture, so it must not be freed by the pattern
  // destructor, only
  // by the destructor in Hwch::PngImage.
  TexturePtr mpTexture;
};

class ClearGlPtn : public GlPattern {
 public:
  ClearGlPtn();
  ClearGlPtn(float updateFreq, uint32_t fgColour, uint32_t bgColour);
  virtual ~ClearGlPtn();

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);
  virtual void Advance();
  virtual bool IsAllTransparent();

 protected:
  uint32_t mFgColour;
  uint32_t mBgColour;

  uint32_t mLine;
};
};

#endif  // __HwchGlPattern_h__
