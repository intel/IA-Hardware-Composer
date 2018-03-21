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

#include "HwchPatternMgr.h"
#include "HwchPattern.h"
#include "HwchGlPattern.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

Hwch::PatternMgr::PatternMgr() {
}

Hwch::PatternMgr::~PatternMgr() {
}

void Hwch::PatternMgr::Configure(bool forceGl, bool forceNoGl) {
  mForceGl = forceGl;
  mForceNoGl = forceNoGl;
}

bool Hwch::PatternMgr::IsGlPreferred(uint32_t bufferFormat) {
  if (mForceGl) {
    return true;
  } else if (mForceNoGl) {
    return false;
  } else {
    switch (bufferFormat) {
      case DRM_FORMAT_ABGR8888:
      case DRM_FORMAT_ARGB8888:
      case DRM_FORMAT_XBGR8888:
      case DRM_FORMAT_RGB565:
        return true;

      default:
        return false;
    }
  }
}

Hwch::Pattern* Hwch::PatternMgr::CreateSolidColourPtn(uint32_t bufferFormat,
                                                      uint32_t colour,
                                                      uint32_t flags) {
  HWCVAL_UNUSED(flags);

  if (IsGlPreferred(bufferFormat)) {
    return new ClearGlPtn(0, colour, colour);
  } else {
    return new SolidColourPtn(colour);
  }
}

Hwch::Pattern* Hwch::PatternMgr::CreateHorizontalLinePtn(
    uint32_t bufferFormat, float updateFreq, uint32_t fgColour,
    uint32_t bgColour, uint32_t matrixColour, uint32_t flags) {
  HWCVAL_UNUSED(flags);

  if (IsGlPreferred(bufferFormat)) {
    if (matrixColour != 0) {
      return new MatrixGlPtn(updateFreq, fgColour, matrixColour, bgColour);
    } else {
      return new HorizontalLineGlPtn(updateFreq, fgColour, bgColour);
    }
  } else {
    return new HorizontalLinePtn(updateFreq, fgColour, bgColour);
  }
}

Hwch::Pattern* Hwch::PatternMgr::CreatePngPtn(
    uint32_t bufferFormat, float updateFreq, Hwch::PngImage& image,
    uint32_t lineColour, uint32_t bgColour, uint32_t flags) {
  if (IsGlPreferred(bufferFormat)) {
    PngGlPtn* ptn = new PngGlPtn(updateFreq, lineColour, bgColour,
                                 (flags & ePtnUseIgnore) != 0);
    ptn->Set(image);
    return ptn;
  } else {
    PngPtn* ptn = new PngPtn(updateFreq, lineColour);
    ptn->Set(image);
    return ptn;
  }
}
