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

#ifndef __HwchGlTests_h__
#define __HwchGlTests_h__

#include "HwchTest.h"

namespace Hwch {
class PngGlLayer : public Layer {
 public:
  PngGlLayer(){};
  PngGlLayer(Hwch::PngImage& png, float updateFreq = 60.0,
             uint32_t lineColour = eWhite, uint32_t bgColour = 0,
             bool bIgnore = false);
};

class GlBasicLineTest : public OptionalTest {
 public:
  GlBasicLineTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicClearTest : public OptionalTest {
 public:
  GlBasicClearTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicTextureTest : public OptionalTest {
 public:
  GlBasicTextureTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicCombo1Test : public OptionalTest {
 public:
  GlBasicCombo1Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicCombo2Test : public OptionalTest {
 public:
  GlBasicCombo2Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicCombo3Test : public OptionalTest {
 public:
  GlBasicCombo3Test(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicPixelDiscardTest : public OptionalTest {
 public:
  GlBasicPixelDiscardTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicViewportTest : public OptionalTest {
 public:
  GlBasicViewportTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicMovingLineTest : public OptionalTest {
 public:
  GlBasicMovingLineTest(Hwch::Interface& interface);

  virtual int RunScenario();
};

class GlBasicPixelDiscardNOPTest : public OptionalTest {
 public:
  GlBasicPixelDiscardNOPTest(Hwch::Interface& interface);

  virtual int RunScenario();
};
}

#endif  // __HwchGlTests_h__
