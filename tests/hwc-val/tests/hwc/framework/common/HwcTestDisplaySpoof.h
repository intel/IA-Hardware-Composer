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

#ifndef __HwcTestDisplaySpoof_h__
#define __HwcTestDisplaySpoof_h__

#include "HwcTestDefs.h"

class HwcTestDisplaySpoof {
 public:
  virtual ~HwcTestDisplaySpoof();
  virtual void ModifyStatus(uint32_t frameNo, int& status) = 0;
};

class HwcTestNullDisplaySpoof : public HwcTestDisplaySpoof {
 public:
  virtual ~HwcTestNullDisplaySpoof();

  virtual void ModifyStatus(uint32_t frameNo, int& status);
};

#endif  // __HwcTestDisplaySpoof_h__
