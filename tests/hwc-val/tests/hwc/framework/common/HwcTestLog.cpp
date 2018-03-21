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

#include <string>
#include <stdlib.h>
#include "HwcTestLog.h"
#include <string>
#include "HwcTestConfig.h"
#include "HwcTestUtil.h"
#ifndef HWCVAL_IN_TEST
#include "HwcTestState.h"
#endif


std::string formatV(const char* fmt, va_list args)
{
    char buf[1024];
    vsnprintf(buf, 1024, fmt, args);
    return std::string(buf);
}



// General HWC validation message logger
// Normal function - log to HWC log viewer if available (i.e. normally if in SF
// process), log to Android log otherwise.
// Define HWCVAL_LOG_ANDROID to ensure that BOTH logs are used whenever
// possible.
// Define HWCVAL_LOG_ANDROIDONLY to ensure that only Android log is used.
int HwcValLogVerbose(int priority, const char* context, int line,
                     const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  std::string logStr = formatV(fmt, args);

  // std::string fmt2 = format("%s(%d) ",context, line);
  std::string fmt2("%s(%d) %s");
  HwcValLog(priority, fmt2.c_str(), context, line, logStr.c_str());

  va_end(args);
  return priority;
}

static const char* sPriorities = "A-VDIWEFS";  // Matches Android priorities.
// except we have Always instead of Unknown.

// Log to the HWC logger at the stated priority level
int HwcValLog(int priority, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  HwcValLogVA(priority, fmt, args);

  va_end(args);
  return priority;
}

// Log varargs to the HWC logger
int HwcValLogVA(int priority, const char* fmt, va_list& args) {
// Get a pointer to HWC's real logger function.
// Note that HWC validation intercepts everything that HWC itself writes to the
// log for parsing
// before forwarding it to the real log.
//
// These are HWC validation messages, so they go straight to the "real log" or
// else we would
// have an infinite loop.
#ifndef HWCVAL_LOG_ANDROIDONLY
// This is the old version, to retain compatibility with any HWC that does not
// support log interception.
#define _HWCLOG (*pLog)
  HwcTestState::HwcLogAddPtr pLog =
      HwcTestState::getInstance()->GetHwcLogFunc();

  if (pLog) {
    // We have obtained a pointer to the logger.
    // Construct the prefix to the log message.
    char prefix[10];
    strcpy(prefix, "HWCVAL:  ");
    if (priority > 0 && priority < (int)strlen(sPriorities)) {
      prefix[7] = sPriorities[priority];
    }

#if __x86_64__
//
// If HWCVAL_LOG_HWC_ANDROID is defined, OR if the priority is "ALWAYS", "ERROR"
// or "FATAL",
// then DUPLICATE the log entry to LOGCAT.
//
#if !defined(HWCVAL_LOG_HWC_ANDROID)
    // HWCLOGA, HWCLOGE and HWCLOGF always go both to HWCLOG and to logcat.
    if ((priority == ANDROID_LOG_UNKNOWN) || (priority >= ANDROID_LOG_ERROR))
#endif
    {
      std::string formattedString = formatV(fmt, args);
      std::string hwclogString(prefix);
      hwclogString += formattedString;

      // Send the combined log string to HwcLogViewer
      _HWCLOG(hwclogString.c_str());

// Debugging option to duplicate the log entry to standard out.
#ifdef HWCVAL_PRINT_LOG
      printf("TID: %d%s\n", gettid(), hwclogString.c_str());
#endif

      // Send the combined log string to Android
      LOG_PRI(priority, "HWCVAL", "%s", formattedString.c_str());
      va_end(args);

      // ** Early return for the "log to hwclog and logcat" case.
      return priority;
    }
#endif  // __x86_64__

#if !__x86_64__ || !defined(HWCVAL_LOG_HWC_ANDROID)

// Prefix the format buffer with the string "HWCVAL:x"
#define FMT_BUF_SIZE 1024
    char fmt3[FMT_BUF_SIZE];
    strcpy(fmt3, prefix);
    strncat(fmt3, fmt, FMT_BUF_SIZE - 10);

#ifdef HWCVAL_PRINT_LOG
    // For DEBUG only: create the log entry as a string8 and output it to logcat
    // and standard out.
    std::string hwcLogStr = formatV(fmt3, args);
    _HWCLOG(hwcLogStr.c_str());
    printf("TID:%d %s\n", gettid(), hwcLogStr.c_str());
#else
    // Legacy form for HWC without log interception.
    _HWCLOG(formatV(fmt3, args).c_str());
#endif

#endif
  }

// This last part is fallback.
// If we can't obtain a pointer to the logger (generally because this is very
// early on and it hasn't been
// created yet) then log to logcat instead.
#ifndef HWCVAL_LOG_HWC_ANDROID
  if ((pLog == 0) || (priority == ANDROID_LOG_UNKNOWN))
#endif  // HWCVAL_LOG_HWC_ANDROID
#endif  // !HWCVAL_LOG_ANDROIDONLY
  {
    LOG_PRI_VA(priority, "HWCVAL", fmt, args);
  }

  return priority;
}

//
// Log an ERROR
// This has two lines:
// 1. HWCVAL:E, followed by the message obtained by looking up the enum "check"
// in HwcTestConfig::mCheckDescriptions
//    (this is populated by the data in HwcTestCheckList.h).
// 2. The formatted string constructed from fmt and the other parameters, which
// is indented by "  -- ".
//
int HwcValError(HwcTestCheckType check, HwcTestConfig* config,
                HwcTestResult* result, const char* fmt, ...) {
  if (!config) {
    return ANDROID_LOG_ERROR;
  }

  result->SetFail(check);
  int priority = config->mCheckConfigs[check].priority;
  std::string msg(HwcTestConfig::mCheckDescriptions[check]);
  msg += "\n  -- ";

  va_list args;
  va_start(args, fmt);
  msg += fmt;
  HwcValLogVA(priority, msg.c_str(), args);
  va_end(args);

  if (priority == ANDROID_LOG_FATAL) {
    Hwcval::ValCallbacks::DoExit();
  }

  return priority;
}
