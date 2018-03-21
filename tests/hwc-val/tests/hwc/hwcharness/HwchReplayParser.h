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

#ifndef __HwchReplayParser_h__
#define __HwchReplayParser_h__

#include <fstream>
#include <string>

#include "re2/re2.h"

#include "HwchFrame.h"
#include "HwchLayer.h"

/**
 * Flag to enable log messages from the parser. This is useful when making
 * changes to the regular expressions.
 */
#define PARSER_DEBUG 1

namespace Hwch {

class ReplayParser  {
 private:
  // Regular expressions for matching HWC log entries
  //
  // See the RE2 wiki for details of the syntax: code.google.com/p/re2
  const std::string hwcl_onset_string =
      "^(\\d+)s (\\d+)ms (?:(\\d+)ns )?(?:TID:(\\d+) )?D(\\d) onSet Entry "
      "(?:frame:(\\d+) )?Fd:(-?\\d{1,2}) "
      "outBuf:0x(.{1,8}) outFd:(-?\\d{1,2}) [fF]lags:(\\d+)(:?.*)$";

  const std::string hwcl_onset_1533_string =
      "^(\\d+)s (\\d+)ms D(\\d) onSet Entry Fd:(-?\\d{1,2}) outBuf:"
      "0x(.{1,8}) outFd:(-?\\d{1,2}) Flags:(\\d+)(:?.*)$";

  const std::string hwcl_layer_string_hdr =
      "^\\s*(\\d+) (\\w{2}) *0x(.{1,12}): ?(?:--|(\\d{1,3})): ?(\\d) ?(\\d+) "
      "(\\w{2}): ?(.{1,2}) ([[:alnum:]]{1,5}) *:[XLY]  *(\\d{1,4})x(\\d{1,4}) "
      "* "
      " *(-?\\d+\\.?\\d*), *(-?\\d+\\.?\\d*), *(-?\\d+\\.?\\d*), "
      "*(-?\\d+\\.?\\d*)"
      " *(-?\\d{1,4}), *(-?\\d{1,4}), *(-?\\d{1,4}), *(-?\\d{1,4}) "
      "(-?\\d+) (-?\\d+) "
      "V: *(\\d{1,4}), *(\\d{1,4}), *(\\d{1,4}), *(\\d{1,4}) ";

  const std::string hwcl_layer_1533_string_hdr =
      "^\\s*(\\d+) (\\w{2}) *0x(.{1,8}): ?(\\d{1,2}): ?(\\d{1,2}) "
      "(\\w{2}): ?(.{1,2}) ([?[:alnum:]]{1,5})  *(\\d{1,4})x(\\d{1,4}) * "
      " *(-?\\d{1,4}), *(-?\\d{1,4}), *(-?\\d{1,4}), *(-?\\d{1,4})->"
      " *(-?\\d{1,4}), *(-?\\d{1,4}), *(-?\\d{1,4}), *(-?\\d{1,4}) "
      "(-?\\d+) (-?\\d+) "
      "V: *(\\d{1,4}), *(\\d{1,4}), *(\\d{1,4}), *(\\d{1,4}) ";

  const std::string hwcl_layer_string_vbr =
      " *(\\d{1,4}), *(\\d{1,4}), *(\\d{1,4}), *(\\d{1,4})";

  const std::string hwcl_layer_string_trl =
      " *U:(.{1,8}) * Hi:(\\d+)(:?[[:alpha:]]*) "
      "Fl:(\\d+)(:?[ [:alnum:]]*).*";

  // Patterns for matching the output from dumpsys SurfaceFlinger
  const std::string ds_display_string =
      "^\\s*Display\\[(\\d+)\\] : (\\d{1,4})x(\\d{1,4}), "
      "xdpi=(.*), ydpi=(.*), refresh=(\\d*).*$";

  const std::string ds_layer_string =
      "^\\s*([ [:alnum:]]+) \\| (.{1,8}) \\| (\\d+) \\| (\\d+) \\| "
      "(\\d+) \\| (\\d+) \\| (.{1,8}) \\| \\[ *([\\.\\d]*), *([\\.\\d]*), "
      "*([\\.\\d]*), *([\\.\\d]*)] \\| \\[ *([\\.\\d]*), *([\\.\\d]*), "
      "*([\\.\\d]*), *([\\.\\d]*)] *(:?[\\._/[:alpha:]]*) *"
      "([\\.\\d]*)? *([[:alpha:]]*)?\\s*$";

  // Patterns for matching hotplug events
  const std::string hotplug_connected_string = ".+ HotPlug connected$";
  const std::string hotplug_disconnected_string = ".+ HotPlug disconnected$";

  // Patterns for matching blanking events
  const std::string blank_string =
      ".+ HardwareManager::onBlank Display (\\d) Blank.*$";
  const std::string unblank_string =
      ".+ HardwareManager::onBlank Display (\\d) Unblank.*$";

  // RE2 compiled Regex data structures
  RE2 hwcl_onset_regex, hwcl_onset_1533_regex, hwcl_layer_regex_hdr,
      hwcl_layer_1533_regex_hdr, hwcl_layer_regex_vbr, hwcl_layer_regex_trl,
      ds_display_regex, ds_layer_regex, hotplug_connected_regex,
      hotplug_disconnected_regex, blank_regex, unblank_regex;

  // Flag to denote whether the parser is ready
  bool mRegexCompilationSuccess;

  // Default update frequency for the dumpsys replay layers
  const float mDefaultDSUpdateFreq = 60.0;

  // Width for hex prints (including the '0x' leader)
  const uint32_t mHexPrintWidth = 10;

 public:
  /** The constructor compiles the regexs into RE2 structures. */
  ReplayParser();

  /** Default destructor. */
  ~ReplayParser() = default;

  /** The parser is not copyable (see above). */
  ReplayParser(const ReplayParser& rhs) = delete;

  /** Disable move semantics (see above). */
  ReplayParser(ReplayParser&& rhs) = delete;

  /** Parser is not copy assignable (see above). */
  ReplayParser& operator=(const ReplayParser& rhs) = delete;

  /** Disable move semantics (see default constructor). */
  ReplayParser& operator=(const ReplayParser&& rhs) = delete;

  /** Returns whether the Regular Expression Compilation was successful. */
  bool IsReady() {
    return mRegexCompilationSuccess;
  }

  /*
   * Funtions for operating on HWC log files
   */

  /** Parses frame headers (i.e. 'onSet Entry') lines in HWC log files. */
  bool ParseHwclOnSet(const std::string& line, int32_t& secs, int32_t& msecs,
                      int32_t& nsecs, int32_t& frame, int32_t& disp,
                      int32_t& flags);

  /** Parses layer lines in HWC log files. */
  bool ParseHwclLayer(const std::string& line, Layer& layer);

  /*
   * Functions for parsing HWC string fields
   */

  /** Parses the index field in layer entries. */
  bool ParseHwclLayerIndex(const std::string& str, uint32_t& val);

  /** Parses the buffer handle field in layer entries. */
  bool ParseHwclLayerHandle(const std::string& str, uint64_t& val);

  /** Parses the blending field in layer entries. */
  bool ParseHwclLayerBlending(const std::string& str, uint32_t& val);

  /** Parses the transform field in layer entries. */
  bool ParseHwclLayerTransform(const std::string& str, uint32_t& val);

  /** Parses the colour space field in layer entries. */
  bool ParseHwclLayerColourSpace(const std::string& str, uint32_t& val);

  /*
   * Predicates for testing HWC layer properties
   */

  /** Tests whether a string matches the pattern for a HWC layer entry */
  bool IsHwclLayer(const std::string& line);

  /** Tests if a layer is an HWC_FRAMEBUFFER_TARGET */
  bool IsHwclLayerFramebufferTarget(const std::string& line);

  /** Tests if a layer is a skip layer */
  bool IsHwclLayerSkip(const std::string& line);

  /** Tests if a layer has an unsupported colour space */
  bool IsHwclLayerUnsupported(const std::string& line);

  /*
   * Functions for operating on dumpsys files
   */

  /** Parses the display, width and height from a dumpsys line. */
  bool ParseDSDisplay(const std::string& line, int32_t& disp, int32_t& width,
                      int32_t& height);

  /** Parses the layer lines in a dumpsys file. */
  bool ParseDSLayer(const std::string& line, Layer& layer);

  /** Parses and extracts the 'profile' field at the end of the layer. */
  bool ParseDSProfile(const std::string& line, std::string& profile);

  /*
   * Predicates for testing HWC layer properties
   */

  /** Tests if a line matches the pattern for a dumpsys layer. */
  bool IsDSLayer(const std::string& line);

  /** Tests if a layer is an HWC_FRAMEBUFFER_TARGET. */
  bool IsDSLayerFramebufferTarget(const std::string& line);

  /*
   * Functions for parsing special events
   */

  /** Parses hot plug events in the log. The parameter 'connected' returns
      'true' if the hot plug event is a connection and 'false' otherwise. */
  bool ParseHotPlug(const std::string& line, bool& connected);

  /** Parses blanking (i.e. blank / unblank) requests */
  bool ParseBlanking(const std::string& line, bool& blank, int32_t& disp);

  /*
   * Unit tests
   */

  /** Unit-tests to prevent regressions in the regular expressions. */
  bool RunParserUnitTests();
};
}

#endif  // __HwchReplayParser_h__
