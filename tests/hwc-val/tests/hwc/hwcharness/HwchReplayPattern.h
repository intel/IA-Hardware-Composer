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

#ifndef __HwchReplayPattern_h__
#define __HwchReplayPattern_h__

#include "HwchLayer.h"
#include "HwchPattern.h"

namespace Hwch {

class ReplayPattern : public HorizontalLinePtn {
  bool mFrameNeedsUpdate = true;

 public:
  /**
   * Default constructor - defaults to 60Hz update frequency with a black
   * line on a white background.
   */
  ReplayPattern(uint32_t bgColour = eWhite, uint32_t fgColour = eBlack,
                float mUpdateFreq = 60.0)
      : HorizontalLinePtn(mUpdateFreq, fgColour, bgColour){};

  /** Default destructor. */
  ~ReplayPattern() = default;

  /** The pattern is copyable constructible. */
  ReplayPattern(const ReplayPattern& rhs) = default;

  /** Disable move semantics - no dynamic state. */
  ReplayPattern(ReplayPattern&& rhs) = delete;

  /** Pattern is copy assignable. */
  ReplayPattern& operator=(const ReplayPattern& rhs) = default;

  /** Disable move semantics - no dynamic state. */
  ReplayPattern& operator=(const ReplayPattern&& rhs) = delete;

  /**
   * Returns a flag to signify whether the frame should be updated (i.e.
   * typically in response to the buffers being rotated (and so the next
   * buffer needs filling).
   */
  bool FrameNeedsUpdate() {
    if (mFrameNeedsUpdate) {
      mUpdatedSinceFBComp = true;
      mFrameNeedsUpdate = false;
      return true;
    }

    return false;
  }

  /** Forces an update the next time the layer is sent */
  void ForceUpdate() {
    mFrameNeedsUpdate = true;
  }
};
}

#endif  // __HwchReplayPattern_h__
