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

#ifndef __HwchPattern_h__
#define __HwchPattern_h__

#include <drm_fourcc.h>
#include <utils/Vector.h>
#include <platformdefines.h>
#include <hardware/hwcomposer2.h>

#include <hwcdefs.h>

#include "HwchDefs.h"

namespace Hwch {
class PngImage;

struct SNV12Chroma {
  uint8_t u;
  uint8_t v;
};

// This structure holds 1, 2 or 4 pixels depending on the pixel size.
class SPixelWord {
 public:
  uint32_t mBytesPerPixel;
  uint32_t mPixelsPerWord32;

  union {
    char mBytes[4];
    uint16_t mWord16[2];
    uint32_t mWord32;
  };

  union {
    SNV12Chroma mChroma[2];
    uint32_t mNv12ChromaWord32;
  };

  union {
    char mVBytes[4];
    uint32_t mYv12VWord32;
  };

  union {
    char mUBytes[4];
    uint32_t mYv12UWord32;
  };

  // Constructor
  SPixelWord();
  SPixelWord(uint32_t colour, uint32_t format);

  uint32_t GetPixelBytes(uint32_t colour, uint32_t format);
};

class Pattern {
 public:
  Pattern(float mUpdateFreq = 0);
  virtual ~Pattern();

  // Will be called by the framework shortly after construction
  virtual void Init();

  // Called each frame when FrameNeedsUpdate() returns true
  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam) = 0;

  // Can be overriden to give non-uniform update period
  virtual bool FrameNeedsUpdate();
  virtual void ForceUpdate();

  // Called at the end of each frame to update internal variables
  virtual void Advance();

  // Called at the end of each frame to update internal variables
  void SetUpdateFreq(float updateFreq);
  float GetUpdateFreq();
  uint64_t GetUpdatePeriodNs();
  void SetUpdatedSinceLastFBComp();
  bool IsUpdatedSinceLastFBComp();
  void ClearUpdatedSinceLastFBComp();

  // Is this an empty transparent pattern i.e. all 0s?
  virtual bool IsAllTransparent();

 protected:
  float mUpdateFreq;
  int64_t mUpdatePeriodNs;
  int64_t mNextUpdateTime;
  bool mUpdatedSinceFBComp;  // Updated since last FB composition

  // Alternate method of working out how often to update, using frame counting
  // for 100% predictability.
  // Use milliframes to cope with update rates not a factor of 60.
  int32_t mUpdatePeriodMilliFrames;
  int32_t mMilliFramesToUpdate;
};

class SolidColourPtn : public Pattern {
 public:
  SolidColourPtn(uint32_t colour);
  virtual ~SolidColourPtn();

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);
  virtual bool IsAllTransparent();

 private:
  uint32_t mColour;
  SPixelWord mPixel;
};

class FramebufferTargetPtn : public SolidColourPtn {
 public:
  FramebufferTargetPtn();
  virtual ~FramebufferTargetPtn();

  virtual bool FrameNeedsUpdate();
};

class HorizontalLinePtn : public Pattern {
 public:
  HorizontalLinePtn(){};
  HorizontalLinePtn(float updateFreq, uint32_t fgColour, uint32_t bgColour);
  virtual ~HorizontalLinePtn();

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);
  virtual void Advance();

 protected:
  void FillLumaLine(unsigned char* data, uint32_t row, uint32_t stride,
                    uint32_t left, uint32_t width, SPixelWord pixel);
  void FillChromaLineNV12(unsigned char* chromaData, uint32_t row,
                          uint32_t stride, uint32_t left, uint32_t width,
                          SPixelWord pixel);
  void FillChromaVLineYV12(unsigned char* chromaData, uint32_t row,
                           uint32_t stride, uint32_t left, uint32_t width,
                           SPixelWord pixel);
  void FillChromaULineYV12(unsigned char* chromaData, uint32_t row,
                           uint32_t stride, uint32_t left, uint32_t width,
                           SPixelWord pixel);

  uint32_t mFgColour;
  uint32_t mBgColour;

  SPixelWord mFgPixel;
  SPixelWord mBgPixel;

  uint32_t mLine;
};

class PngPtn : public HorizontalLinePtn {
 public:
  PngPtn(float updateFreq, uint32_t lineColour);

  // Connect to an image, ownership of the image stays with the caller
  void Set(Hwch::PngImage& image);

  // Connect to an image, we get ownership
  void Set(std::unique_ptr<Hwch::PngImage> spImage);
  virtual ~PngPtn();

  virtual int Fill(HWCNativeHandle buf, const hwcomposer::HwcRect<int>& rect,
                   uint32_t& bufferParam);

 private:
  void FillLineFromImage(unsigned char* data, uint32_t row, uint32_t stride,
                         uint32_t left, uint32_t width);

  Hwch::PngImage* mpImage;

  // Only for ownership
  std::unique_ptr<Hwch::PngImage> mspImage;

  // Pointers to the actual image data
  uint8_t** mRowPointers;
};
};

#endif  // __HwchPattern_h__
