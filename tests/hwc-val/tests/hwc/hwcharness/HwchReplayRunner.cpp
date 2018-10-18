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

#include "HwchInterface.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"

#include "HwchReplayRunner.h"

Hwch::ReplayRunner::ReplayRunner(Hwch::Interface& interface,
                                 const char* filename)
    : Hwch::Test(interface), mInterface(interface) {
  // Open the file and initialise the parser
  mFile.open(filename);
  mParser = std::unique_ptr<Hwch::ReplayParser>(new Hwch::ReplayParser());

  if (!mFile.good()) {
    HWCERROR(eCheckReplayFail, "Fatal error opening replay file");
  } else if (!mParser->IsReady()) {
    HWCERROR(eCheckReplayFail, "Replay parser not ready");
  } else {
    mReplayReady = true;
  }
}
