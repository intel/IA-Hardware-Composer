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

#ifndef __Hwcval_LogParser_h__
#define __Hwcval_LogParser_h__

#include "HwcvalLogIntercept.h"

#if ANDROID_VERSION >= 600
#include <regex>
#endif

class HwcTestKernel;

namespace Hwcval {
class LogParser : public Hwcval::LogChecker {
 private:
  // Pointers and references to internal objects
  HwcTestKernel* mTestKernel = nullptr;

  int64_t mParsedEncEnableStartTime = 0;
  int64_t mParsedEncDisableStartTime = 0;
  uint32_t mParsedEncDisableSession = 0;
  uint32_t mParsedEncEnableSession = 0;
  uint32_t mParsedEncEnableInstance = 0;
  uint32_t mSetModeDisplay = 0;
  uint32_t mSetModeWidth = 0;
  uint32_t mSetModeHeight = 0;
  uint32_t mSetModeRefresh = 0;
  uint32_t mSetModeFlags = 0;
  uint32_t mSetModeAspectRatio = 0;

  // Parser functionaity
  bool ParseHWCServiceApi(pid_t pid, int64_t timestamp, const char* str);

  // Common functionality

  // Top-level function to match a regex to a string. This uses C++11 regexs
  // on M-Dessert and above, and RE2 on L-Dessert.
  template <typename T = void, typename U = T>
  bool MatchRegex(const char* regex, const char* line,
                  int32_t* num_fields_matched = nullptr,
                  T * match1_ptr = nullptr, U * match2_ptr = nullptr);

#if ANDROID_VERSION >= 600
  // Overloads for field extraction
  template <typename T>
  bool ExtractField(const std::cmatch::value_type field, T* field_ptr);
  bool ExtractField(const std::cmatch::value_type field, char* field_ptr);

  // Default overload (for when no match parameters are specified to MatchRegex)
  bool ExtractField(const std::cmatch::value_type field, void* dummy) {
    HWCVAL_UNUSED(field);
    HWCVAL_UNUSED(dummy);

    return false;
  };
#endif

  bool ParseCommonExit(const char* str, const char* fn,
                       int* ret = nullptr);

  // Functionality for individual functions
  bool ParseEnableEncryptedSessionEntry(pid_t pid, int64_t timestamp,
                                        const char* str);
  bool ParseEnableEncryptedSessionExit(pid_t pid, int64_t timestamp,
                                       const char* str);
  bool ParseDisableEncryptedSessionEntry(pid_t pid, int64_t timestamp,
                                         const char* str);
  bool ParseDisableEncryptedSessionExit(pid_t pid, int64_t timestamp,
                                        const char* str);
  bool ParseDisableAllEncryptedSessionsEntry(pid_t pid, int64_t timestamp,
                                             const char* str);
  bool ParseDisableAllEncryptedSessionsExit(pid_t pid, int64_t timestamp,
                                            const char* str);
  bool ParseDisplayModeGetAvailableModesEntry(pid_t pid, int64_t timestamp,
                                              const char* str);
  bool ParseDisplayModeGetAvailableModesExit(pid_t pid, int64_t timestamp,
                                             const char* str);
  bool ParseDisplayModeGetModeEntry(pid_t pid, int64_t timestamp,
                                    const char* str);
  bool ParseDisplayModeGetModeExit(pid_t pid, int64_t timestamp,
                                   const char* str);
  bool ParseDisplayModeSetModeEntry(pid_t pid, int64_t timestamp,
                                    const char* str);
  bool ParseDisplayModeSetModeExit(pid_t pid, int64_t timestamp,
                                   const char* str);
  bool ParseMDSUpdateVideoStateEntry(pid_t pid, int64_t timestamp,
                                     const char* str);
  bool ParseMDSUpdateVideoStateExit(pid_t pid, int64_t timestamp,
                                    const char* str);
  bool ParseMDSUpdateVideoFPSEntry(pid_t pid, int64_t timestamp,
                                   const char* str);
  bool ParseMDSUpdateVideoFPSExit(pid_t pid, int64_t timestamp,
                                  const char* str);
  bool ParseMDSUpdateInputStateEntry(pid_t pid, int64_t timestamp,
                                     const char* str);
  bool ParseMDSUpdateInputStateExit(pid_t pid, int64_t timestamp,
                                    const char* str);
  bool ParseSetOptimizationModeEntry(pid_t pid, int64_t timestamp,
                                     const char* str);
  bool ParseSetOptimizationModeExit(pid_t pid, int64_t timestamp,
                                    const char* str);

  bool ParseKernel(pid_t pid, int64_t timestamp, const char* str);
  bool ParseBufferNotifications(pid_t pid, int64_t timestamp, const char* str);
  bool ParseOption(const char*& p);
  bool ParseOptionSettings(pid_t pid, int64_t timestamp, const char* str);
  bool ParseCompositionChoice(pid_t pid, int64_t timestamp, const char* str);
  bool ParseRotationInProgress(pid_t pid, int64_t timestamp, const char* str);

 public:
  LogParser(HwcTestKernel* pKernel)
      : mTestKernel(pKernel){};

  // Log parser entry point
  virtual bool DoParse(pid_t pid, int64_t timestamp, const char* str);
};
}

#endif  // __Hwcval_LogParser_h__
