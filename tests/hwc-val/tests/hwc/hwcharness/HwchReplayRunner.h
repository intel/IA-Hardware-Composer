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

#ifndef __HwchReplayRunner_h__
#define __HwchReplayRunner_h__

#include "HwchInterface.h"
#include "HwchTest.h"
#include "HwchReplayLayer.h"
#include "HwchReplayParser.h"

namespace Hwch {

class ReplayRunner :  public Test {
 protected:
  /** The base class owns the parser instance. */
  std::unique_ptr<ReplayParser> mParser;

  /** Reference to the HWC interface. */
  Hwch::Interface& mInterface;

  /** RAII file handle. */
  std::ifstream mFile;

  /**
   * Set to true if the replay file has been opened and the regex
   * compilation has been successful.
   */
  bool mReplayReady = false;

 public:
  /**
   * @name  ReplayRunner
   * @brief Base class constructor.
   *
   * @param interface  Reference to the Hardware Composer interface.
   * @param filename   File to replay (typically from command line).
   *
   * @details Handles file opening and dynamically allocates an instance
   * of the parser.
   */
  ReplayRunner(Hwch::Interface& interface, const char* filename);

  /** Returns whether the replay file was opened successfully. */
  bool IsReady() {
    return mReplayReady;
  }

  /** Virtual function to print individual 'runner' statistics. */
  virtual void PrintStatistics(void) {
    std::printf("No replay statistics implemented for this runner\n");
  }

  /** Empty virtual destructor. */
  virtual ~ReplayRunner() = default;

  /** Runs the regular expression unit tests for the parser. */
  bool RunParserUnitTests(void) {
    return mParser->RunParserUnitTests();
  }
};
}

#endif  // __HwchReplayRunner_h__
