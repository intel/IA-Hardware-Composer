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

#include "HwcTestState.h"
#include "HwcTestKernel.h"

#if ANDROID_VERSION < 600
#include "re2/re2.h"
#include "re2/stringpiece.h"
#endif

#include "HwcvalLogParser.h"

#define CALL_PARSER_FUNC(name)         \
  {                                    \
    if ((name)(pid, timestamp, str)) { \
      return true;                     \
    }                                  \
  }

// Log validation
bool Hwcval::LogParser::DoParse(pid_t pid, int64_t timestamp, const char* str) {
  // pid and timestamp parameters for future use.
  HWCVAL_UNUSED(pid);

  if (ParseKernel(pid, timestamp, str)) {
    return true;
  }

  // See if the string has the HWC Service Api prefix
  if (strncmp("HwcService_", str, 11) == 0) {
    HWCCHECK(eCheckUnknownHWCAPICall);
    if (!ParseHWCServiceApi(pid, timestamp, str)) {
      HWCERROR(eCheckUnknownHWCAPICall, "Log parser could not parse: %s", str);
    };

    return true;
  }

  return false;
}

bool Hwcval::LogParser::ParseKernel(pid_t pid, int64_t timestamp,
                                    const char* str) {
  CALL_PARSER_FUNC(ParseBufferNotifications);
  CALL_PARSER_FUNC(ParseOptionSettings);
  CALL_PARSER_FUNC(ParseCompositionChoice);
  CALL_PARSER_FUNC(ParseRotationInProgress);

  return false;
}

bool Hwcval::LogParser::ParseHWCServiceApi(pid_t pid, int64_t timestamp,
                                           const char* str) {
  CALL_PARSER_FUNC(ParseDisplayModeGetAvailableModesEntry);
  CALL_PARSER_FUNC(ParseDisplayModeGetAvailableModesExit);
  CALL_PARSER_FUNC(ParseDisplayModeGetModeEntry);
  CALL_PARSER_FUNC(ParseDisplayModeGetModeExit);
  CALL_PARSER_FUNC(ParseDisplayModeSetModeEntry);
  CALL_PARSER_FUNC(ParseDisplayModeSetModeExit);
  CALL_PARSER_FUNC(ParseSetOptimizationModeEntry);
  CALL_PARSER_FUNC(ParseSetOptimizationModeExit);
  return false;
}

template <typename T, typename U>
bool Hwcval::LogParser::MatchRegex(const char* regex, const char* line,
                                   int32_t* num_fields_matched, T* match1_ptr,
                                   U* match2_ptr) {
  bool result = false;

  if (num_fields_matched != nullptr) {
    *num_fields_matched = 0;
  }

#if ANDROID_VERSION >= 600
  // M-Dessert or greater - use C++11 Regexs
  std::regex hwcsRegex(regex);
  std::cmatch match;
  result = std::regex_search(line, match, hwcsRegex);
  if (!result) {
    return false;  // No match
  }

  // We got a match - extract any captured fields
  HWCCHECK(eCheckLogParserError);
  if (match.empty() && (match1_ptr != nullptr || match2_ptr != nullptr)) {
    // Programming error - the caller is expecting to capture fields, but
    // the regex match has not produced any.
    HWCERROR(eCheckLogParserError,
             "Expecting to extract fields, but regex match is empty!");
    return false;
  }

  // Extract the field(s)
  if ((match.size() >= 2) && ExtractField(match[1], match1_ptr) &&
      num_fields_matched) {
    (*num_fields_matched)++;
  }

  if ((match.size() == 3) && ExtractField(match[2], match2_ptr) &&
      num_fields_matched) {
    (*num_fields_matched)++;
  }
#else
  // This is an L-Dessert build - use RE2. Note, you have to call FullMatch
  // with the correct number of parameters (i.e. its using variable arguments.)
  if (!match1_ptr && !match2_ptr) {
    result = RE2::PartialMatch(line, regex);
  } else if (match1_ptr && !match2_ptr) {
    result = RE2::PartialMatch(line, regex, match1_ptr);
    if (result && num_fields_matched) {
      *num_fields_matched = 1;
    }
  } else if (match1_ptr && match2_ptr) {
    result = RE2::PartialMatch(line, regex, match1_ptr, match2_ptr);
    if (result && num_fields_matched) {
      *num_fields_matched = 2;
    }
  }
#endif

  return result;
}

#if ANDROID_VERSION >= 600
// Field extraction overloads
template <typename T>
bool Hwcval::LogParser::ExtractField(const std::cmatch::value_type field,
                                     T* field_ptr) {
  if (field_ptr) {
    *field_ptr = std::stoi(field);
    return true;
  }

  return false;
}

bool Hwcval::LogParser::ExtractField(const std::cmatch::value_type field,
                                     char* field_ptr) {
  if (field_ptr) {
    // Callers responsibiility to make sure there is enough memory available
    std::strcpy(field_ptr, field.str().c_str());
    return true;
  }

  return false;
}
#endif

// Common parsing functionality (i.e. patterns which match across multiple
// functions)
bool Hwcval::LogParser::ParseCommonExit(const char* str, const char* fn,
                                        status_t* ret) {
  // Functions can either return 'OK' or 'ERROR' (plus a return value).
  std::string regex_ok("HwcService_" + std::string(fn) + " OK <--");
  if (MatchRegex(regex_ok.c_str(), str)) {
    if (ret) {
      *ret = 0;
    }

    HWCLOGD_COND(eLogParse, "PARSED MATCHED %s - %s exit (return code: %d)",
                 str, fn, 0);
    return true;
  }

  int32_t ret_val = 0, num_fields_matched = 0;

  std::string regex_error("HwcService_" + std::string(fn) +
                          " ERROR (-?\\d+) <--");
  if (MatchRegex(regex_error.c_str(), str, &num_fields_matched, &ret_val)) {
    if (ret) {
      *ret = ret_val;
    }

    HWCLOGD_COND(eLogParse, "PARSED MATCHED %s - %s exit (return code: %d)",
                 str, fn, ret_val);
    return true;
  }

  return false;
}

bool Hwcval::LogParser::ParseDisplayModeGetAvailableModesEntry(
    pid_t pid, int64_t timestamp, const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  int32_t num_fields_matched = 0, display = -1;

  if (MatchRegex("HwcService_DisplayMode_GetAvailableModes D(\\d) -->", str,
                 &num_fields_matched, &display)) {
    HWCCHECK(eCheckLogParserError);
    if (num_fields_matched == 1) {
      HWCLOGD_COND(eLogParse, "PARSED MATCHED %s - got available modes for D%d",
                   str, display);
      return true;
    } else {
      HWCERROR(eCheckLogParserError, "%s: Failed to extract one field!",
               __func__);
    }
  }

  return false;
}

bool Hwcval::LogParser::ParseDisplayModeGetAvailableModesExit(pid_t pid,
                                                              int64_t timestamp,
                                                              const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  if (MatchRegex("HwcService_DisplayMode_GetAvailableModes .* <--", str)) {
    HWCLOGD_COND(eLogParse, "PARSED MATCHED %s - exiting GetAvailableModes",
                 str);
    return true;
  }

  return false;
}

bool Hwcval::LogParser::ParseDisplayModeGetModeEntry(pid_t pid,
                                                     int64_t timestamp,
                                                     const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  int32_t num_fields_matched = 0, display = -1;

  if (MatchRegex("HwcService_DisplayMode_GetMode D(\\d) -->", str,
                 &num_fields_matched, &display)) {
    HWCCHECK(eCheckLogParserError);
    if (num_fields_matched == 1) {
      HWCLOGD_COND(eLogParse, "PARSED MATCHED %s - got mode for D%d", str,
                   display);
      return true;
    } else {
      HWCERROR(eCheckLogParserError, "%s: Failed to extract one field!",
               __func__);
    }
  }

  return false;
}

bool Hwcval::LogParser::ParseDisplayModeGetModeExit(pid_t pid,
                                                    int64_t timestamp,
                                                    const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  if (MatchRegex("HwcService_DisplayMode_GetMode .* <--", str)) {
    return true;
  }

  return false;
}

bool Hwcval::LogParser::ParseDisplayModeSetModeEntry(pid_t pid,
                                                     int64_t timestamp,
                                                     const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  int32_t num_fields_matched = 0, display = -1;
  std::string mode_str;

  if (MatchRegex("HwcService_DisplayMode_SetMode D(\\d) (.*) -->", str,
                 &num_fields_matched, &display, &mode_str)) {
    HWCCHECK(eCheckLogParserError);
    if (num_fields_matched == 2) {
      HWCLOGD_COND(eLogParse, "PARSED MATCHED %s - set mode for D%d: %s", str,
                   display, mode_str.c_str());
      HWCVAL_LOCK(_l, mTestKernel->GetMutex());

      // Extract the width, height, refresh rate, flags and aspect ratio
      HWCCHECK(eCheckLogParserError);
      if (!MatchRegex("(\\d+)x(\\d+)", str, &num_fields_matched, &mSetModeWidth,
                      &mSetModeHeight) ||
          !MatchRegex("@(\\d+)", str, &num_fields_matched, &mSetModeRefresh) ||
          !MatchRegex("F:(\\d+), A:(\\d+)", str, &num_fields_matched,
                      &mSetModeFlags, &mSetModeAspectRatio)) {
        HWCERROR(eCheckLogParserError, "%s: Failed to parse mode string!",
                 __func__);
      }

      HWCLOGD_COND(eLogParse,
                   "PARSED MATCHED %s - width %d height %d refresh %d flags %d "
                   "aspect ratio %d",
                   mode_str.c_str(), mSetModeWidth, mSetModeHeight,
                   mSetModeRefresh, mSetModeFlags, mSetModeAspectRatio);

      HwcTestCrtc* crtc = mTestKernel->GetHwcTestCrtcByDisplayIx(display, true);
      if (crtc) {
        mSetModeDisplay = display;
        crtc->SetUserModeStart();
      } else {
        HWCLOGW("Can't set user mode for display %d as no CRTC defined",
                display);
      }

      return true;
    } else {
      HWCERROR(eCheckLogParserError, "%s: Failed to extract two fields!",
               __func__);
    }
  }

  return false;
}

bool Hwcval::LogParser::ParseDisplayModeSetModeExit(pid_t pid,
                                                    int64_t timestamp,
                                                    const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  int ret_val = -1;

  if (ParseCommonExit(str, "DisplayMode_SetMode", &ret_val)) {
    HWCVAL_LOCK(_l, mTestKernel->GetMutex());
    HWCLOGD_COND(eLogParse,
                 "PARSED MATCHED %s - set mode exit (return code: %d)", str,
                 ret_val);

    HwcTestCrtc* crtc =
        mTestKernel->GetHwcTestCrtcByDisplayIx(mSetModeDisplay, true);
    if (crtc) {
      crtc->SetUserModeFinish(ret_val, mSetModeWidth, mSetModeWidth,
                              mSetModeRefresh, mSetModeFlags,
                              mSetModeAspectRatio);
    } else {
      HWCLOGW("Can't set user mode finish for display %d as no CRTC defined",
              mSetModeDisplay);
    }

    return true;
  }

  return false;
}

bool Hwcval::LogParser::ParseSetOptimizationModeEntry(pid_t pid,
                                                      int64_t timestamp,
                                                      const char* str) {
  return false;
}

bool Hwcval::LogParser::ParseSetOptimizationModeExit(pid_t pid,
                                                     int64_t timestamp,
                                                     const char* str) {
  return false;
}

bool Hwcval::LogParser::ParseBufferNotifications(pid_t pid, int64_t timestamp,
                                                 const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  const char* p =
      strafter(str, "BufferManager: Notification free buffer handle ");

  if (p == 0) {
    return false;
  }

  uintptr_t h = atoptrinc(p);

  if (h != 0) {
    HWCNativeHandle handle = (HWCNativeHandle)h;

    HWCLOGD_COND(eLogParse, "PARSED MATCHED %s Freeing %p", str, handle);
    mTestKernel->GetWorkQueue().Push(std::shared_ptr<Hwcval::Work::Item>(new Hwcval::Work::BufferFreeItem(handle)));
  }

  return true;
}

bool Hwcval::LogParser::ParseOption(const char*& p) {
  skipws(p);
  std::string optionName = getWord(p);
  skipws(p);

  if (*p++ != ':') {
    return false;
  }

  if (*p++ != ' ') {
    return false;
  }

  // The value string is not quoted, so we have to use some logic to work out
  // where it ends.
  const char* pWords[256];
  std::string words[256];
  const char* endOfValue = 0;

  for (uint32_t nw = 0; nw < 256 && *p && (*p != '\n'); ++nw) {
    pWords[nw] = p;
    words[nw] = getWord(p);
    skipws(p);

    if ((nw > 3) && (words[nw] == "Changable") &&
        (words[nw - 1] == "Int" || words[nw - 1] == "Str")) {
      for (const char* p2 = pWords[nw - 2]; p2 != pWords[nw - 3]; --p2) {
        if (*p2 == '(') {
          endOfValue = p2;
          break;
        }
      }
    }
  }

  std::string value;
  if (endOfValue) {
    HWCLOGV_COND(eLogOptionParse, "%s pWords[0] %p %s", optionName.c_str(),
                 pWords[0], pWords[0]);
    HWCLOGV_COND(eLogOptionParse, "endOfValue %p %s", endOfValue, endOfValue);
    value = std::string(pWords[0], endOfValue - pWords[0]);
    HWCLOGV_COND(eLogOptionParse, "value: %s", value.c_str());
  } else {
    value = words[0];
    HWCLOGV_COND(eLogOptionParse, "ParseOption: %s = %s", optionName.c_str(),
                 value.c_str());
  }

  mTestKernel->SetHwcOption(optionName, value);
  return true;
}

bool Hwcval::LogParser::ParseOptionSettings(pid_t pid, int64_t timestamp,
                                            const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  const char* p = str;
  if (strncmpinc(p, "Option ") == 0) {
    if (strncmpinc(p, "Values:") == 0) {
      while (*p++ == '\n') {
        if (!ParseOption(p)) {
          return false;
        }

        while (*p && (*p != '\n')) {
          p++;
        }
      }
    } else if ((strncmpinc(p, "Default ") == 0) ||
               (strncmpinc(p, "Forced ") == 0)) {
      return ParseOption(p);
    }
  }

  return false;
}

static Hwcval::Statistics::Counter numSfFallbackCompositions(
    "sf_fallback_compositions");
static Hwcval::Statistics::Counter numTwoStageFallbackCompositions(
    "two_stage_fallback_compositions");
static Hwcval::Statistics::Counter numLowlossCompositions(
    "lowloss_compositions");

bool Hwcval::LogParser::ParseCompositionChoice(pid_t pid, int64_t timestamp,
                                               const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  // Fallbacks are highly undesirable, ultimately we may log these as errors.
  const char* p = strafter(str, "fallbackToSurfaceFlinger!");

  if (p == 0) {
    p = str;
    if (strncmpinc(p, "TwoStageFallbackComposer") == 0) {
      ++numTwoStageFallbackCompositions;
      return true;
    } else if (strncmpinc(p, "LowlossComposer") == 0) {
      ++numLowlossCompositions;
      return true;
    }

    return false;
  }

  p = str;
  if (*p != 'D') {
    return false;
  }

  // uint32_t d = atoi(p);

  ++numSfFallbackCompositions;

  // HWCCHECK count will be set at end of test when we know how many
  // compositions there were
  HWCERROR(eCheckSfFallback, "Not required, TwoStageFallback should be used");
  return true;
}

bool Hwcval::LogParser::ParseRotationInProgress(pid_t pid, int64_t timestamp,
                                                const char* str) {
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  const char* p = str;
  if (strncmpinc(p, "Rotation in progress") != 0) {
    return false;
  }

  const char* p2 = strafter(p, "FrameKeepCnt: ");
  if (p2 == 0)
    return false;

  uint32_t f = atoi(p2);

  p2 = strafter(p, "SnapshotLayerHandle: ");
  if (p2 == 0)
    return false;
  uintptr_t h = atoptrinc(p2);

  if (h == 0)
    return false;

  HWCNativeHandle handle = (HWCNativeHandle)h;
  HWCLOGD_COND(eLogParse,
               "PARSED MATCHED: Rotation in progress FrameKeepCnt %d handle %p",
               f, handle);

  mTestKernel->SetSnapshot(handle, f);

  return true;
}
