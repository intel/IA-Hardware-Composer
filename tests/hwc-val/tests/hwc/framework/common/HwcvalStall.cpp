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

#include "HwcvalStall.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

Hwcval::Stall::Stall()
    : mName("Undefined"), mUs(0), mPct(0), mRandThreshold(0) {
}

// Constructor from a string like [<p>%]<d><unit>
// where <unit>=s|ms|us|ns
// order can also be reversed, i.e. delay first
// Percentage indicates percent of sample points where delay will take place.
// If omitted, delay will take place at all sample points.
Hwcval::Stall::Stall(const char* configStr, const char* name)
    : mName(name), mUs(0), mPct(100.0) {
  // Parse string of the format [<x>%][<y><unit>]
  // where <unit>=s|ms|us|ns
  // and x and y are floating point

  const char* p = configStr;

  while (*p) {
    skipws(p);
    double n = atofinc(p);
    skipws(p);

    if (strncmpinc(p, "%") == 0) {
      mPct = n;
    } else if (strncmpinc(p, "s") == 0) {
      mUs = n * HWCVAL_SEC_TO_US;
    } else if (strncmpinc(p, "ms") == 0) {
      mUs = n * HWCVAL_MS_TO_US;
    } else if (strncmpinc(p, "us") == 0) {
      mUs = n;
    } else if (strncmpinc(p, "ns") == 0) {
      mUs = n / HWCVAL_US_TO_NS;
    } else {
      HWCLOGV_COND(eLogStall, "Stall::Stall %f NO MATCH %s", n, p);
    }
  }

  if (mUs == 0) {
    // Stall is disabled
    mPct = 0;
    mRandThreshold = 0;
  } else {
    mRandThreshold = RAND_MAX * mPct / 100.0;
  }

  HWCLOGD_COND(eLogStall, "Stall::Stall %s %s -> %f%% %fms threshold %d", name,
               configStr, mPct, double(mUs) / HWCVAL_MS_TO_US, mRandThreshold);
}

Hwcval::Stall::Stall(uint32_t us, double pct)
    : mName("Unknown"),
      mUs(us),
      mPct(pct),
      mRandThreshold(pct * RAND_MAX / 100.0) {
}

Hwcval::Stall::Stall(const Stall& rhs)
    : mName(rhs.mName), mUs(rhs.mUs), mPct(rhs.mPct), mRandThreshold(rhs.mPct) {
}

void Hwcval::Stall::Do(Hwcval::Mutex* mtx) {
  HWCLOGV_COND(eLogStall, "Do %s threshold %d", mName.c_str(), mRandThreshold);
  if (mRandThreshold > 0) {
    if (rand() < mRandThreshold) {
      HWCLOGV_COND(eLogStall, "Executing %s stall %fms", mName.c_str(),
                   double(mUs) / HWCVAL_MS_TO_US);

      if (mtx) {
        mtx->unlock();
      }

      usleep(mUs);

      if (mtx) {
        mtx->lock();
      }

      HWCLOGD_COND(eLogStall, "Completed %s stall %fms", mName.c_str(),
                   double(mUs) / HWCVAL_MS_TO_US);
    }
  }
}
