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

#ifndef __HwchReplayHWCLRunner_h__
#define __HwchReplayHWCLRunner_h__

#include "HwchInterface.h"

#include "HwchReplayRunner.h"
#include "HwchReplayParser.h"

/** Flag to enable debug code in the HWCL Replay Runner. */
#define REPLAY_HWCL_DEBUG 1

namespace Hwch {

class ReplayHWCLRunner : public ReplayRunner {
 private:
  /** Layer cache key type. */
  using layer_cache_key_t = uint32_t;

  /** Type for mapping buffer handles to dynamically allocated layers. */
  using layer_cache_t =
      std::map<layer_cache_key_t, std::shared_ptr<ReplayLayer>>;
  using layer_cache_itr_t =
      std::map<layer_cache_key_t, std::shared_ptr<ReplayLayer>>::iterator;

  /** Constants for number of nano/millseconds in a second. */
  const uint64_t mNanosPerSec = 1000000000;
  const uint64_t mMillisPerSec = 1000000;

  /** Skip inter-frame delays that are larger than mInterframeBound (seconds).
   */
  int32_t mInterframeBound = 60;

  /** Flag to store the match algorithm to use in the buffer tracking. */
  uint32_t mReplayMatch = 0;

  /** Command line argument to disable inter-frame spacing. */
  bool mReplayNoTiming = false;

  /** Command line argument to set the alpha value. */
  int32_t mAlpha = 0xFF;

  /** Function to create a layer cache key from an index and a display. */
  inline layer_cache_key_t MakeLayerCacheKey(uint32_t layer_index,
                                             uint32_t display) {
    return (layer_index << 4) | display;
  }

  /** Utility function to process layers outside of the main loop. */
  bool AddLayers(Frame& frame, uint32_t display, layer_cache_t& layer_cache,
                 layer_cache_t& prev_layer_cache, int32_t secs, int32_t msecs);

  /**
   * Statistics structure to count the following events:
   *
   *   parsed_onset_count - number of 'onSet Entry' statements parsed
   *   parsed_layer_count - number of layers parsed (in total).
   *   skip_layer_count - number of skip layers parsed.
   *   hwc_frame_count - number of frames sent to the HWC.
   *   processed_layer_count - number of layers excluding framebuffer
   *                           targets and layers with unsupported colour
   *                           spaces (i.e. '???').
   *   encrypted_layer_count - number of encrypted layers parsed
   *   match_count - number of layers matched by the buffer tracking.
   *   allocation_count - total number of buffer allocations
   *   hotplug_count - total number of hotplug events
   *   hotplug_connects_count - number of hotplug connects
   *   hotplug_disconnects_count - number of hotplug disconnects
   *   blanking_count - total number of blanking events
   *   blanking_blank_count - number of 'blank' events
   *   blanking_unblank_count - number of 'unblank' events
   */
  struct statistics {
    uint32_t parsed_onset_count = 0;
    uint32_t parsed_layer_count = 0;
    uint32_t skip_layer_count = 0;
    uint32_t hwc_frame_count = 0;
    uint32_t processed_layer_count = 0;
    uint32_t encrypted_layer_count = 0;
    uint32_t match_count = 0;
    uint32_t allocation_count = 0;
    uint32_t hotplug_count = 0;
    uint32_t hotplug_connects_count = 0;
    uint32_t hotplug_disconnects_count = 0;
    uint32_t blanking_count = 0;
    uint32_t blanking_blank_count = 0;
    uint32_t blanking_unblank_count = 0;
  } mStats;

 public:
  /**
   * No default constructor. There is currently no mechanism for
   * instantiating a ReplayHWCLRunner and then subsequently passing
   * in a file to parse.
   */
  ReplayHWCLRunner() = delete;

  /** HWCLRunner is not copyable (see default constructor). */
  ReplayHWCLRunner(const ReplayRunner& rhs) = delete;

  /** Disable move semantics (see default constructor). */
  ReplayHWCLRunner(ReplayRunner&& rhs) = delete;

  /** HWCLRunner is not copyable (see default constructor). */
  ReplayHWCLRunner& operator=(const ReplayHWCLRunner& rhs) = delete;

  /** Disable move semantics (see default constructor). */
  ReplayHWCLRunner& operator=(const ReplayHWCLRunner&& rhs) = delete;
  ~ReplayHWCLRunner() = default;

  /**
   * @name  ReplayHWCLRunner
   * @brief Main constructor for replaying a scenario based on HWC logs.
   *
   * @param interface         Reference to the Hardware Composer interface.
   * @param filename          File to replay (typically from command line).
   * @param replayMatch       Integer to select the buffer tracking algorithm.
   * @param replayNoTiming    Flag to disable inter-frame spacing
   * @param alpha             Alpha value to apply to each layer
   *
   * @details This is the main user constructor for replaying HWC log
   * scenarios. Note, if the file can not be opened (or is empty) the
   * program sets a status flag which can be tested from the top-level.
   */
  ReplayHWCLRunner(Hwch::Interface& interface, const char* filename,
                   uint32_t replayMatch, bool replayNoTiming,
                   int32_t alpha)
      : ReplayRunner(interface, filename),
        mReplayMatch(replayMatch),
        mReplayNoTiming(replayNoTiming),
        mAlpha(alpha){};

  /**
   * @name  PrintStatistics
   * @brief Outputs statistics relating to the HWC log replay.
   *
   * @details Calls printf to display statistics relating to the HWC log replay.
   */
  void PrintStatistics(void) override;

  /**
   * @name  RunScenario
   * @brief Top-level function to run the replay.
   *
   * @retval true      Scenario was replayed sucessfully.
   * @retval false     An error occurred (check logs).
   *
   * @details This is the entry point to the HWC log runner. Calling
   * this function runs the replay scenario.
   */
  int RunScenario(void) override;

};
}

#endif  // __HwchReplayHWCLRunner_h__
