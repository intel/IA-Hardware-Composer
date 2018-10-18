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

#include "HwchRange.h"
#include "HwcTestUtil.h"
#include "HwcTestState.h"

namespace Hwch {
Subrange::~Subrange() {
}

SubrangeContiguous::SubrangeContiguous(int32_t start, int32_t end)
    : mStart(start), mEnd(end) {
}

SubrangeContiguous::~SubrangeContiguous() {
}

bool SubrangeContiguous::Test(int32_t value) {
  return ((value >= mStart) && (value <= mEnd));
}

SubrangePeriod::SubrangePeriod(uint32_t interval) : mInterval(interval) {
}

SubrangePeriod::~SubrangePeriod() {
}

bool SubrangePeriod::Test(int32_t value) {
  return ((value % mInterval) == 0);
}

SubrangeRandom::SubrangeRandom(uint32_t interval) : mChoice(interval) {
}

SubrangeRandom::~SubrangeRandom() {
}

bool SubrangeRandom::Test(int32_t value) {
  HWCVAL_UNUSED(value);
  return (mChoice.Get() == 0);
}

Range::Range() {
}

Range::Range(int32_t mn, int32_t mx) {
  Add(new SubrangeContiguous(mn, mx));
}

// Range specification is a comma-separated list of subranges being either:
// a. number <n>
// b. contiguous subrange [<m>]-[<n>]
//    e.g. 23-46 OR -500 OR 200-
// c. period <x>n e.g. 2n to indicate every second instance
// d. randomized period e.g. 2r to indicate every second instance on average.
Range::Range(const char* spec) {
  HWCLOGD("Constructing range %s", spec);
  const char* p = spec;
  while (*p) {
    int32_t v = INT_MIN;

    if (isdigit(*p)) {
      v = atoiinc(p);
    }

    if (strncmpinc(p, "-") == 0) {
      int32_t v2 = INT_MAX;

      if (isdigit(*p)) {
        v2 = atoiinc(p);
      }

      HWCLOGD("Contiguous subrange %d-%d", v, v2);
      Add(new SubrangeContiguous(v, v2));
    } else if (strncmpinc(p, "n") == 0) {
      Add(new SubrangePeriod(v));
    } else if (strncmpinc(p, "r") == 0) {
      Add(new SubrangeRandom(v));
    } else if (*p == ',') {
      Add(new SubrangeContiguous(v, v));
    }

    if (strncmpinc(p, ",") != 0) {
      return;
    }
  }
}

void Range::Add(Subrange* subrange) {
  mSubranges.push_back(std::shared_ptr<Subrange>(subrange));
}

bool Range::Test(int32_t value) {
  for (uint32_t i = 0; i < mSubranges.size(); ++i) {
    std::shared_ptr<Subrange> subrange = mSubranges.at(i);

    if (subrange->Test(value)) {
      return true;
    }
  }

  return false;
}
}
