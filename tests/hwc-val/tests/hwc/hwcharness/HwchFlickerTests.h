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

#ifndef __HwchFlickerTests_h__
#define __HwchFlickerTests_h__

#include "HwchTest.h"

namespace Hwch {

class Flicker1Test : public Test {
 public:
  Flicker1Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

class Flicker2Test : public Test {
 public:
  Flicker2Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

class Flicker3Test : public Test {
 public:
  Flicker3Test(Hwch::Interface& interface);

  virtual int RunScenario();
};
}

#endif  // __HwchFlickerTests_h__
