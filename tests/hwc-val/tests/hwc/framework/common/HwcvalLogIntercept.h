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

#ifndef __Hwcval_LogIntercept_h__
#define __Hwcval_LogIntercept_h__

#include <cstdio>
#include <cstdarg>
#include <utils/Log.h>

#include "abstractcompositionchecker.h"
#include "abstractlog.h"

namespace Hwcval {
// Our abstract definition of a log checker.
// Note that each checker has a DoParse function that must be implemented by the
// concrete subclass.
// Log checkers can be chained together by passing a pointer to a log checker in
// the constructor to
// its antecedent. A log checker will call the next one if and only if it fails
// to find a match
// within its DoParse function.
class LogChecker {
 public:
  LogChecker(LogChecker* next = 0) : mNext(next) {
  }

  virtual ~LogChecker() {
  }

  // To be implemented by the concrete subclass.
  // Must return true if it matches and consumes the log message, false
  // otherwise.
  virtual bool DoParse(pid_t pid, int64_t timestamp, const char* str) = 0;

  // Call the DoParse function in this, and all following log checkers, until
  // one of them matches the string.
  bool Parse(pid_t pid, int64_t timestamp, const char* str) {
    if (mNext) {
      if (mNext->Parse(pid, timestamp, str)) {
        return true;
      }
    }

    return DoParse(pid, timestamp, str);
  }

 private:
  LogChecker* mNext;
};

// Our implementation of HWC's abstract log class.
// We supply this to HWC so that we can intercept (and parse) its log entries.
class LogIntercept : public ::hwcomposer::AbstractLogWrite {
 private:
  ::hwcomposer::AbstractLogWrite* mRealLog;
  char* mInterceptedEntry;
  Hwcval::LogChecker* mChecker;

 public:
  // Implementations of AbstractLog functions
  virtual char* reserve(uint32_t maxSize);

  virtual void log(char* endPtr);

  // Control functions
  void Register(Hwcval::LogChecker* logChecker,
                ::hwcomposer::AbstractCompositionChecker* compositionChecker,
                uint32_t compositionVersionsSupported);

  ::hwcomposer::AbstractLogWrite* GetRealLog();
};

inline ::hwcomposer::AbstractLogWrite* LogIntercept::GetRealLog() {
  return mRealLog;
}

typedef ::hwcomposer::AbstractLogWrite* (*SetLogValPtr)(
    ::hwcomposer::AbstractLogWrite* logVal,
    ::hwcomposer::AbstractCompositionChecker* checkComposition,
    uint32_t& versionSupportMask);
}

#endif  // __Hwcval_LogIntercept_h__
