/*
// Copyright (c) 2017 Intel Corporation
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

#ifndef PUBLIC_HWCRECT_H_
#define PUBLIC_HWCRECT_H_

#include <stdint.h>

#include <cstring>

namespace hwcomposer {

// Some of the structs are adopted from drm_hwcomposer
template <typename TFloat>
struct Rect {
  union {
    struct {
      TFloat left, top, right, bottom;
    };
    TFloat bounds[4];
  };
  typedef TFloat TNum;
  Rect() {
    reset();
  }
  Rect(TFloat left_, TFloat top_, TFloat right_, TFloat bottom_)
      : left(left_), top(top_), right(right_), bottom(bottom_) {
  }
  template <typename T>
  Rect(const Rect<T> &rhs) {
    for (int i = 0; i < 4; i++)
      bounds[i] = rhs.bounds[i];
  }
  template <typename T>
  Rect<TFloat> &operator=(const Rect<T> &rhs) {
    for (int i = 0; i < 4; i++)
      bounds[i] = rhs.bounds[i];
    return *this;
  }
  bool operator==(const Rect &rhs) const {
    for (int i = 0; i < 4; i++) {
      if (bounds[i] != rhs.bounds[i])
        return false;
    }
    return true;
  }

  /**
   * Check if bounds are unset
   *
   * @return True if all bounds are set to 0
   */
  bool empty() const {
    for (int i = 0; i < 4; i++) {
      if (bounds[i] != 0)
        return false;
    }
    return true;
  }

  /**
   * Set bounds to 0
   */
  void reset() {
    for (int i = 0; i < 4; i++) {
      memset(&bounds, 0, sizeof(bounds));
    }
  }
};

}  // namespace hwcomposer

#endif  // PUBLIC_HWCRECT_H_
