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

#ifndef __HwchPatternMgr_h__
#define __HwchPatternMgr_h__

#include "HwchPattern.h"

#include <drm_fourcc.h>


namespace Hwch {
class PatternMgr {
 public:
  enum PatternOptions { ePtnUseClear = 1, ePtnUseIgnore = 2 };

  PatternMgr();
  ~PatternMgr();

  // Set up preferences
  void Configure(bool forceGl, bool forceNoGl);

  // Should we use GL for this format?
  bool IsGlPreferred(uint32_t bufferFormat);

  // Pattern creation
  Pattern* CreateSolidColourPtn(uint32_t bufferFormat, uint32_t colour,
                                uint32_t flags = 0);
  Pattern* CreateHorizontalLinePtn(uint32_t bufferFormat, float updateFreq,
                                   uint32_t fgColour, uint32_t bgColour,
                                   uint32_t matrixColour = 0,
                                   uint32_t flags = 0);
  Pattern* CreatePngPtn(uint32_t bufferFormat, float updateFreq,
                        Hwch::PngImage& image, uint32_t lineColour,
                        uint32_t bgColour = 0, uint32_t flags = 0);

 private:
  bool mForceGl;
  bool mForceNoGl;
};
}

#endif  // __HwchPatternMgr_h__
