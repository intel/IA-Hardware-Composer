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

#include "HwchDisplaySpoof.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include <stdlib.h>

Hwch::DisplaySpoof::DisplaySpoof() {
}

Hwch::DisplaySpoof::~DisplaySpoof() {
}

void Hwch::DisplaySpoof::ModifyStatus(uint32_t frameNo, int& ret) {
  if (mRange.Test(frameNo)) {
    HWCLOGI("Display fail spoof: return value %d replaced with -1", ret);
    ret = -1;
  }
}

void Hwch::DisplaySpoof::Configure(const char* str) {
  mRange = Range(str);
}
