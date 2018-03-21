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

#include "HwcvalStatistics.h"
#include "HwcTestState.h"

namespace android {
ANDROID_SINGLETON_STATIC_INSTANCE(Hwcval::Statistics);
}

namespace Hwcval {
Statistics::Statistic::Statistic(const char* name) : mName(name) {
  Statistics::getInstance().Register(this);
}

Statistics::Statistic::~Statistic() {
}

const char* Statistics::Statistic::GetName() {
  return mName.c_str();
}

Statistics::Counter::Counter(const char* name) : Statistic(name), mCount(0) {
}

Statistics::Counter::~Counter() {
}

void Statistics::Counter::Inc() {
  ++mCount;
}

void Statistics::Counter::Clear() {
  mCount = 0;
}

void Statistics::Counter::Dump(FILE* file, const char* prefix) {
  fprintf(file, "%s,%s,0,%d\n", prefix, GetName(), mCount);
}

uint32_t Statistics::Counter::GetValue() {
  return mCount;
}

// Statistics methods
void Statistics::Register(Statistic* stat) {
  mStats.push_back(stat);
}

void Statistics::Dump(FILE* file, const char* prefix) {
  for (uint32_t i = 0; i < mStats.size(); ++i) {
    Statistic* stat = mStats.at(i);
    stat->Dump(file, prefix);
  }
}

void Statistics::Clear() {
  for (uint32_t i = 0; i < mStats.size(); ++i) {
    Statistic* stat = mStats.at(i);
    stat->Clear();
  }
}

// Histogram
Statistics::Histogram::Histogram(const char* name, bool cumulative)
    : Aggregate<uint32_t>(name), mCumulative(cumulative) {
}

Statistics::Histogram::~Histogram() {
}

void Statistics::Histogram::Add(uint32_t measurement) {
  Aggregate<uint32_t>::Add(measurement);

  for (uint32_t i = mElement.size(); i <= measurement; ++i) {
    mElement.push_back(0);
  }

  mElement.at(measurement)++;
}

void Statistics::Histogram::Clear() {
  Aggregate<uint32_t>::Clear();
  mElement.clear();
}

void Statistics::Histogram::Dump(FILE* file, const char* prefix) {
  Aggregate<uint32_t>::Dump(file, prefix);

  if (mCumulative) {
    uint32_t runningTotal = 0;

    for (uint32_t i = 0; i < mElement.size(); ++i) {
      runningTotal += mElement.at(i);
      fprintf(file, "%s,%s_cf,%d,%d\n", prefix, GetName(), i, runningTotal);
    }
  } else {
    for (uint32_t i = 0; i < mElement.size(); ++i) {
      fprintf(file, "%s,%s_v,%d,%d\n", prefix, GetName(), i,
              mElement.at(i));
    }
  }
}

}  // namespace Hwcval
