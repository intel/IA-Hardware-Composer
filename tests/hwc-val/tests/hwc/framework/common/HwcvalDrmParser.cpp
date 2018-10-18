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
#include "DrmShimChecks.h"

#include "HwcvalDrmParser.h"

#define CALL_PARSER_FUNC(name) \
  {                            \
    if ((name)(str)) {         \
      return true;             \
    }                          \
  }

Hwcval::DrmParser::DrmParser(DrmShimChecks* checks,
                             Hwcval::LogChecker* nextChecker)
    : mChecks(checks), Hwcval::LogChecker(nextChecker){};

// Log validation
bool Hwcval::DrmParser::DoParse(pid_t pid, int64_t timestamp, const char* str) {
  // pid and timestamp parameters for future use.
  HWCVAL_UNUSED(pid);
  HWCVAL_UNUSED(timestamp);

  CALL_PARSER_FUNC(ParseDrmUpdates)
  CALL_PARSER_FUNC(ParseDrmReleaseTo)
  CALL_PARSER_FUNC(ParseEsdRecovery)
  CALL_PARSER_FUNC(ParseSelfTeardown)
  CALL_PARSER_FUNC(ParseDisplayMapping)
  CALL_PARSER_FUNC(ParseDisplayUnmapping)
  CALL_PARSER_FUNC(ParseDropFrame)

  return false;
}

bool Hwcval::DrmParser::ParseDrmReleaseTo(const char* str) {
  const char* p = strafter(str, "drm releaseTo");
  if (p == 0) {
    return false;
  }

  uint32_t dropFrame = atoi(p);
  HWCVAL_UNUSED(dropFrame);

  p = strafter(str, "DrmConnector ");
  if (p == 0) {
    return false;
  }

  uint32_t connector = atoi(p);

  mChecks->ValidateDrmReleaseTo(connector);

  return true;
}

bool Hwcval::DrmParser::ParseDrmUpdates(const char* str) {
  // Parse string such as: DrmPageFlip Drm Crtc 3 issuing drm updates for frame
  // frame:20 [timeline:21]
  // Parse string such or: DrmPageFlip Fence: Drm Crtc 3 issuing drm updates for
  // frame frame:20 [timeline:21]
  const char* p = strafter(str, "DrmPageFlip ");
  if (p == 0) {
    return false;
  }

  const char* searchStr = " issuing drm updates for ";
  p = strafter(str, searchStr);
  if (p == 0) {
    return false;
  }

  const char* pCrtcStr = strafter(str, "Crtc ");
  if (pCrtcStr == 0) {
    return false;
  }

  uint32_t crtcId = atoi(pCrtcStr);

  searchStr = "frame:";
  p = strafter(p, searchStr);

  if (p != 0) {
    uint32_t nextFrameNo = atoi(p);
    mChecks->SetDrmFrameNo(nextFrameNo);

    HWCLOGD_COND(eLogParse, "PARSED MATCHED %s", str);
    mChecks->ValidateFrame(crtcId, nextFrameNo);
  } else {
    // "No valid frame"
    // Happens on start and after DPMS
    // Allows us to validate previous frame and ensure the blanking
    // frame to follow does not get validated.
    HWCLOGD_COND(eLogParse, "PARSED MATCHED %s", str);
    mChecks->ValidateFrame(crtcId, 0);
  }

  return true;
}

bool Hwcval::DrmParser::ParseEsdRecovery(const char* str) {
  const char* p = strafter(str, "Drm ESDEvent to D");
  if (p == 0) {
    return false;
  }

  uint32_t d = atoi(p);

  mChecks->ValidateEsdRecovery(d);

  return true;
}

bool Hwcval::DrmParser::ParseSelfTeardown(const char* str) {
  if (strstr(str, "DRM Display Self Teardown")) {
//Removed code for self tear down incase of protected content
    return true;
  }

  if (strstr(str, "Drm HotPlugEvent to hotpluggable")) {
    // HWC is still processing the hot plugs
    // Reset the frame counter if it's running
    //mProtChecker.RestartSelfTeardown();

    return true;
  }

  return false;
}

bool Hwcval::DrmParser::ParseDisplayMapping(const char* str) {
  // Parse the logical/physical display mapping
  const char* p = str;
  if (strncmpinc(p, "DrmDisplay ") != 0) {
    return false;
  }

  p = strafter(p, "DrmConnector ");
  if (p == 0) {
    return false;
  }

  uint32_t connId = atoiinc(p);

  if (strncmpinc(p, "DRM New Connection Connector ") != 0) {
    return false;
  }

  p = strafter(p, "CrtcID ");
  if (p == 0) {
    return false;
  }

  uint32_t crtcId = atoi(p);
  HWCLOGD_COND(eLogParse, "PARSED MATCHED New Connection connId %d crtcId %d",
               connId, crtcId);

  mChecks->ValidateDisplayMapping(connId, crtcId);

  return true;
}

bool Hwcval::DrmParser::ParseDisplayUnmapping(const char* str) {
  const char* p = str;

  if (strncmpinc(p, "DRM Reset Connection Connector ") != 0) {
    return false;
  }

  p = strafter(p, "CrtcID ");
  if (p == 0) {
    return false;
  }

  uint32_t crtcId = atoi(p);

  HWCLOGD_COND(eLogParse,
               "PARSED MATCHED: DRM Reset Connection Connector ... CRTC %d",
               crtcId);

  mChecks->ValidateDisplayUnmapping(crtcId);

  return true;
}

bool Hwcval::DrmParser::ParseDropFrame1(const char* str, DrmShimCrtc*& crtc,
                                        uint32_t& f) {
  const char* p = str;
  if (strncmpinc(p, "Queue: ") != 0)
    return false;
  const char* qname = p;

  p = strafter(qname, "Drop WorkItem:");
  if (p == 0)
    return false;

  const char* p2 = strafter(qname, "Crtc ");
  if (p2 == 0)
    return false;
  uint32_t crtcId = atoi(p2);

  p = strafter(p, "frame:");
  if (p == 0)
    return false;

  f = atoi(p);

  crtc = mChecks->GetCrtc(crtcId);
  if (crtc == 0)
    return false;
  HWCLOGD_COND(eLogParse, "%s: PARSED MATCHED Drop frame:%d crtc %d", str, f,
               crtc->GetCrtcId());

  return true;
}

bool Hwcval::DrmParser::ParseDropFrame2(const char* str, DrmShimCrtc*& crtc,
                                        uint32_t& f) {
  const char* p = str;
  if (strncmpinc(p, "drm DrmDisplay ") != 0)
    return false;

  const char* qname = p;
  p = strafter(qname, "drop frame:");
  if (p == 0)
    return false;

  f = atoi(p);
  uint32_t d = atoi(qname);
  crtc = mChecks->GetCrtcByDisplayIx(d);
  if (crtc == 0)
    return false;

  HWCLOGD_COND(eLogParse, "%s: PARSED MATCHED Drop frame:%d crtc %d", str, f,
               crtc->GetCrtcId());
  return true;
}

bool Hwcval::DrmParser::ParseDropFrame(const char* str) {
  DrmShimCrtc* crtc;
  uint32_t f;

  if (!ParseDropFrame1(str, crtc, f)) {
    if (!ParseDropFrame2(str, crtc, f)) {
      return false;
    }
  }

  crtc->RecordDroppedFrames(1);

  // Throw away the frame in the LLQ
  uint32_t d = crtc->GetDisplayIx();
  Hwcval::LayerList* ll = mChecks->GetLLQ(d).GetFrame(f, false);

  if (ll) {
    HWCLOGD_COND(eLogFence,
                 "ParseDropFrame: D%d CRTC %d Drop frame:%d fence %d", d,
                 crtc->GetCrtcId(), f, ll->GetRetireFence());
  }

  return true;
}
