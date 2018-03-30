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

#ifndef __HwchPngImage_h__
#define __HwchPngImage_h__

#include <utils/Vector.h>
#include <string>

#include "HwchDefs.h"

// includes for SSIM and libpng
#include "png.h"

namespace Hwch {
class GlImage;
typedef GlImage* TexturePtr;

class PngImage  {
 public:
  PngImage(const char* filename = 0);
  virtual ~PngImage();

  bool ReadPngFile(const char* fileName);

  uint32_t GetWidth();
  uint32_t GetHeight();
  uint32_t GetColorType();
  uint32_t GetBitDepth();
  uint8_t** GetRowPointers();
  uint8_t* GetDataBlob();
  const char* GetName();
  bool IsLoaded();
  TexturePtr GetTexture();

 private:
  bool ProcessFile(void);

  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mColorType;
  uint32_t mBitDepth;
  png_bytep* mRowPointers;

  std::string mName;
  std::string mInputFile;
  bool mLoaded;

  uint8_t* mDataBlob;
  TexturePtr mpTexture;
};

inline uint32_t Hwch::PngImage::GetWidth() {
  return mWidth;
}

inline uint32_t Hwch::PngImage::GetHeight() {
  return mHeight;
}

inline uint32_t Hwch::PngImage::GetColorType() {
  return mColorType;
}

inline uint32_t Hwch::PngImage::GetBitDepth() {
  return mBitDepth;
}

inline uint8_t** Hwch::PngImage::GetRowPointers() {
  return static_cast<uint8_t**>(mRowPointers);
}

inline uint8_t* Hwch::PngImage::GetDataBlob() {
  return static_cast<uint8_t*>(mDataBlob);
}

class PngReader {
 public:
  PngReader();
  virtual ~PngReader();

  bool Read(const char* path, png_bytep*& rowPointers, uint8_t*& dataBlob,
            uint32_t& width, uint32_t& height, uint32_t& colorType,
            uint32_t& bitDepth);
  uint32_t GetWidth();
  uint32_t GetHeight();

 private:
  uint32_t mWidth, mHeight;
  uint32_t mBytesPerPixel;
  uint32_t mBytesPerRow;
  png_byte mColorType;
  png_byte mBitDepth;

  png_structp mpPngStruct;  // internal structure used by libpng
  png_infop mpPngInfo;      // structure with the information of the png file
  FILE* mFp;
};
};

#endif  // __HwchPngImage_h__
