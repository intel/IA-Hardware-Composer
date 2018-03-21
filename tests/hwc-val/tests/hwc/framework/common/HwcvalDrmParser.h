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

#ifndef __Hwcval_DrmParser_h__
#define __Hwcval_DrmParser_h__

#include "HwcTestState.h"
#include "HwcvalLogIntercept.h"

class DrmShimCrtc;

namespace Hwcval {
class DrmParser : public Hwcval::LogChecker {
 private:
  // Pointers and references to internal objects
  DrmShimChecks* mChecks;

 public:
  // Constructor
  DrmParser(DrmShimChecks* checks,
            Hwcval::LogChecker* nextChecker);

  // Parse "...drm releaseTo..."
  bool ParseDrmReleaseTo(const char* str);

  // Parse "...issuing DRM updates..."
  bool ParseDrmUpdates(const char* str);

  // Parse ESD recovery events
  bool ParseEsdRecovery(const char* str);

  // Parse HWC self-teardown
  bool ParseSelfTeardown(const char* str);

  // Parse logical to physical display mapping
  bool ParseDisplayMapping(const char* str);
  bool ParseDisplayUnmapping(const char* str);

  // Parse drop frame
  bool ParseDropFrame1(const char* str, DrmShimCrtc*& crtc, uint32_t& f);
  bool ParseDropFrame2(const char* str, DrmShimCrtc*& crtc, uint32_t& f);
  bool ParseDropFrame(const char* str);

  // Log parser entry point
  virtual bool DoParse(pid_t pid, int64_t timestamp, const char* str);
};
}

#endif  // __Hwcval_DrmParser_h__
