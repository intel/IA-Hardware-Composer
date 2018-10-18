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

#ifndef __HwchChoice_h__
#define __HwchChoice_h__

#include <utils/Vector.h>
#include <vector>
namespace Hwch {
// Choice class to make it generic to choose between options.
template <class C>
class GenericChoice  {
 public:
  GenericChoice<C>() {
  }

  virtual ~GenericChoice<C>() {
  }

  // Return a choice
  virtual C Get() = 0;

  // How many valid choices are there?
  virtual uint32_t NumChoices() = 0;

  // How many iterations should we do? Default to same as number of choices
  virtual uint32_t NumIterations() {
    return NumChoices();
  }

  virtual bool IsEnabled() {
    return (NumChoices() > 0);
  }
};

class Choice : public GenericChoice<int> {
 public:
  Choice(int mn = 0, int mx = 0, const char* name = "Choice");
  virtual ~Choice();
  void IncMax();
  void Setup(int mn = 0, int mx = 0, const char* name = "Choice");
  void SetMax(int mx, bool disable = false);
  void SetMin(int mn);
  virtual int Get();
  virtual uint32_t NumChoices();
  static void Seed(uint32_t seed);
  virtual bool IsEnabled();

 protected:
  int mMin;
  int mMax;
  const char* mName;
};

class FloatChoice : public GenericChoice<float> {
 public:
  FloatChoice(float mn = 0, float mx = 0, const char* name = "Float");
  virtual ~FloatChoice();
  void SetMax(float mx);
  virtual float Get();
  virtual uint32_t NumChoices();

 protected:
  float mMin;
  float mMax;
  const char* mName;
};

class LogarithmicChoice : public GenericChoice<double> {
 public:
  LogarithmicChoice(double mn = 0, double mx = 0,
                    const char* name = "Logarithmic");
  virtual ~LogarithmicChoice();
  void SetMax(double mx);
  virtual double Get();
  virtual uint32_t NumChoices();

 protected:
  FloatChoice mChoice;
};

class LogIntChoice : public GenericChoice<uint32_t> {
 public:
  LogIntChoice(uint32_t mn = 0, uint32_t mx = 0, const char* name = "LogInt");
  virtual ~LogIntChoice();
  void SetMax(uint32_t mx);
  virtual uint32_t Get();
  virtual uint32_t NumChoices();

 protected:
  LogarithmicChoice mLogChoice;
  uint32_t mMin;
  uint32_t mMax;
};

class EventDelayChoice : public GenericChoice<int> {
 public:
  EventDelayChoice(uint32_t mx, const char* name = "EventDelay");
  virtual ~EventDelayChoice();
  void SetMax(int mx);
  virtual int Get();
  virtual uint32_t NumChoices();

 private:
  LogIntChoice mDelayChoice;
  Choice mSyncChoice;
};

template <class C>
class MultiChoice : public GenericChoice<C> {
 public:
  MultiChoice<C>(const char* name = "MultiChoice")
      : GenericChoice<C>(), c(0, -1, name) {
  }

  void Add(C option) {
    mOptions.push_back(option);
    c.IncMax();
  }

  virtual C Get() {
    return mOptions[c.Get()];
  }

  virtual uint32_t NumChoices() {
    return c.NumChoices();
  }

 private:
  Choice c;
  std::vector<C> mOptions;
};
}

#endif  // __HwchChoice_h__
