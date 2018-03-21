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

#ifndef __HwchReplayDSRunner_h__
#define __HwchReplayDSRunner_h__

#include "HwchInterface.h"
#include "HwchReplayRunner.h"
#include "HwchReplayParser.h"

namespace Hwch {

class ReplayDSRunner : public ReplayRunner {
 private:
  /** Type for cacheing pointers to dynamically allocated layers. */
  using layer_cache_t = std::vector<ReplayLayer *>;

  /* Utility function to process layers outside of the main loop */
  bool AddLayers(Hwch::Frame& frame, int32_t display,
                 layer_cache_t& layer_cache);

  /* The number of frames to replay (specified on the command-line) */
  int32_t mNumFrames;

  /**
   * Statistics structure to count the following events:
   *
   *   parsed_frame_count - number of frames parsed
   *   parsed_layer_count - number of layers parsed (in total).
   *   hwc_frame_count - number of frames sent to the HWC.
   *   processed_layer_count - number of layers that are not
   *                           framebuffer targets
   */
  struct statistics {
    uint32_t parsed_frame_count = 0;
    uint32_t parsed_layer_count = 0;
    uint32_t hwc_frame_count = 0;
    uint32_t processed_layer_count = 0;
  } mStats;

 public:
  /**
   * No default constructor. There is currently no mechanism for
   * instantiating a ReplayDSRunner and then subsequently passing
   * in a file to parse.
   */
  ReplayDSRunner() = delete;

  /** Default destructor. */
  ~ReplayDSRunner() = default;

  /** DSRunner is not copyable (see default constructor). */
  ReplayDSRunner(const ReplayRunner& rhs) = delete;

  /** Disable move semantics (see default constructor). */
  ReplayDSRunner(ReplayRunner&& rhs) = delete;

  /** DSRunner is not copyable (see default constructor). */
  ReplayDSRunner& operator=(const ReplayDSRunner& rhs) = delete;

  /** Disable move semantics (see default constructor). */
  ReplayDSRunner& operator=(const ReplayDSRunner&& rhs) = delete;

  /**
   * @name  ReplayDSRunner
   * @brief Main constructor for running a dumpsys scenario.
   *
   * @param interface  Reference to the Hardware Composer interface.
   * @param filename   File to replay (typically from command line).
   * @param num_frames Number of frames to replay.
   *
   * @details This is the main user constructor for replaying dumpsys
   * scenarios. Note, if the file can not be opened (or is empty) the
   * program sets a status flag which can be tested at the top-level.
   */
  ReplayDSRunner(Hwch::Interface& interface, const char* filename,
                 int32_t num_frames)
      : ReplayRunner(interface, filename), mNumFrames(num_frames){};

  /**
   * @name  PrintStatistics
   * @brief Outputs statistics relating to the dumpsys snapshot replay.
   *
   * @details Calls printf to display statistics relating to the dumpsys
   * snapshot replay.
   */
  void PrintStatistics(void) override;

  /**
   * @name  RunScenario
   * @brief Top-level function to run the replay.
   *
   * @retval true      Scenario was replayed sucessfully.
   * @retval false     An error occurred (check logs).
   *
   * @details This is the entry point to the dumpsys runner. Calling
   * this function runs the replay scenario.
   */
  int RunScenario(void) override;
};
}

#endif  // __HwchReplayDSRunner_h__
