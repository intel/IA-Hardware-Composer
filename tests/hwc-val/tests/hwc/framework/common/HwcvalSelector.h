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

#ifndef __HwcvalSelector_h__
#define __HwcvalSelector_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include "HwcTestDefs.h"
#include "HwcTestUtil.h"
#include <string>
#include <utils/RefBase.h>

namespace Hwcval {
// Abstract selector class
// Implementation should give a true or false which is dependent on the numeric
// input.
// (This may not be entirely true for randomly based selectors).
class Selector : public android::RefBase {
 public:
  Selector() : mValue(0) {
  }

  Selector(const Selector& rhs) : RefBase(), mValue(0) {
    HWCVAL_UNUSED(rhs);
  }

  Selector& operator=(const Selector& rhs) {
    // Value is not copied. Only the selection criteria which are in the
    // subclass.
    HWCVAL_UNUSED(rhs);

    return *this;
  }

  // return true if the number is in the range
  virtual bool Test(int32_t n) = 0;

  // increment a counter, and return true if it is in the range
  bool Next();

 protected:
  // current value to test
  uint32_t mValue;
};

// increment a counter, and return true if it is in the range
inline bool Selector::Next() {
  return Test(mValue++);
}

}  // namespace Hwcval

#endif  // __HwcvalSelector_h__
