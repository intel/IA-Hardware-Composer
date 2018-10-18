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

#ifndef __HwchReplayLayer_h__
#define __HwchReplayLayer_h__

#include <set>
#include <utils/Errors.h>

#include "HwchCoord.h"
#include "HwchLayer.h"
#include "HwchSystem.h"

namespace Hwch {

class ReplayLayer : public Layer {
 private:
  /**
   * A SortedVector of buffer handles that are associated with this
   * layer. Provides lookup and insertion in O(log N) time.
   */
  std::set<uint32_t> mKnownBufs;

  /**
   * Cache the last handle seen by this layer. This is necessary for
   * the buffer rotation code i.e. layers that are triple-buffered
   * have three buffer handls associated with them.
   */
  uint32_t mLastHandle = 0;

 public:
  /**
   * @name  ReplayLayer
   * @brief Base class constructor.
   *
   * @param name Name of the layer e.g. StatusBar
   * @param width Layer width (in pixels)
   * @param height Layer height (in pixels)
   * @param format Defines the clour space format
   *
   * @details Calls the parent class constructor.
   */
  ReplayLayer(const char* name, Coord<int32_t> width, Coord<int32_t> height,
              uint32_t format = HAL_PIXEL_FORMAT_RGBA_8888, uint32_t bufs = 1)
      : Layer(name, width, height, format, bufs){};

  /** Copy constructor (required for Dup). */
  ReplayLayer(const ReplayLayer& rhs)
      : Layer(rhs), mKnownBufs(rhs.mKnownBufs){};

  /** Associates a handle with the layer and returns its index. */
  size_t AddKnownBuffer(uint32_t handle) {
    mKnownBufs.emplace(handle);
    return std::distance(mKnownBufs.begin(), mKnownBufs.find(handle));
  }

  /** Tests whether a handle is associated with the layer. */
  bool IsKnownBuffer(uint32_t handle) const {
    return (mKnownBufs.find(handle) != mKnownBufs.end());
  }

  /** Returns the index of a handle in the vector (if it exists). */
  size_t GetKnownBufferIndex(uint32_t handle) const {
    return std::distance(mKnownBufs.begin(), mKnownBufs.find(handle));
  }

  /** Returns the number of handles that are known to this layer */
  size_t GetNumHandles() const {
    return mKnownBufs.size();
  }

  /** Sets the last handle seen on this layer. */
  void SetLastHandle(uint32_t handle) {
    mLastHandle = handle;
  }

  /** Returns the last handle seen on this layer. */
  uint32_t GetLastHandle() const {
    return mLastHandle;
  }

  /**
   * Returns whether the layer fills the screen (e.g. Wallpaper).
   *
   * Note: this function uses the coordinates of the layer's logical
   * display frame to determine whether or not it is full screen. This
   * is fine in the Replay tool, but may be invalid in other contexts.
   */
  bool IsFullScreen(uint32_t display) {
    LogDisplayRect& ldf = mLogicalDisplayFrame;
    Display& system_display = mSystem.GetDisplay(display);

    return (((ldf.bottom.mValue - ldf.top.mValue) >=
             static_cast<int32_t>(system_display.GetHeight())) &&
            ((ldf.right.mValue - ldf.left.mValue) >=
             static_cast<int32_t>(system_display.GetWidth())));
  }

  /** Overrides Dup so that we can duplicate layers as required. */
  ReplayLayer* Dup() override {
    return new ReplayLayer(*this);
  }
};
}

#endif  // __HwchReplayLayer_h__
