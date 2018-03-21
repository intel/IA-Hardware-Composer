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

#ifndef __HwchRange_h__
#define __HwchRange_h__

#include "HwchChoice.h"
#include "HwcvalSelector.h"
#include <vector>
namespace Hwch {
class Subrange  {
 public:
  virtual ~Subrange();
  virtual bool Test(int32_t value) = 0;
};

class SubrangeContiguous : public Subrange {
 public:
  SubrangeContiguous(int32_t start, int32_t end);
  virtual ~SubrangeContiguous();
  virtual bool Test(int32_t value);

 private:
  int32_t mStart;
  int32_t mEnd;
};

class SubrangePeriod : public Subrange {
 public:
  SubrangePeriod(uint32_t interval);
  virtual ~SubrangePeriod();
  virtual bool Test(int32_t value);

 private:
  uint32_t mInterval;
};

class SubrangeRandom : public Subrange {
 public:
  SubrangeRandom(uint32_t interval);
  virtual ~SubrangeRandom();
  virtual bool Test(int32_t value);

 private:
  Choice mChoice;
};

class Range : public Hwcval::Selector {
 public:
  Range();
  Range(int32_t mn, int32_t mx);

  // Range specification is a comma-separated list of subranges being either:
  // a. number <n>
  // b. contiguous subrange [<m>]-[<n>]
  //    e.g. 23-46 OR -500 OR 200-
  // c. period <x>n e.g. 2n to indicate every second instance
  // d. randomized period e.g. 2r to indicate every second instance on average.
  Range(const char* spec);

  // Add a subrange to the range
  void Add(Subrange* subrange);

  // return true if the number is in the range
  virtual bool Test(int32_t n);

 private:
  // list of subranges
  std::vector<std::shared_ptr<Subrange> > mSubranges;
};
}

#endif  // __HwchRange_h__
