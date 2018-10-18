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

#ifndef __CrcDebugfs_h__
#define __CrcDebugfs_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include <sys/types.h>
#include <unistd.h>
#include "HwcTestDefs.h"

// PIPE_CRC_DEBUG_CRC_IFACE - set to 1 if you suspect problems with debugfs
//
#define PIPE_CRC_DEBUG_CRC_IFACE 0

//---------------------------------------------------------------------
// BEGIN: copied from $TOP/linux/kernel/drivers/gpu/drm/i915/i915_drv.h
//---------------------------------------------------------------------
enum intel_pipe_crc_source {
  INTEL_PIPE_CRC_SOURCE_NONE,
  INTEL_PIPE_CRC_SOURCE_PLANE1,
  INTEL_PIPE_CRC_SOURCE_PLANE2,
  INTEL_PIPE_CRC_SOURCE_PF,
  INTEL_PIPE_CRC_SOURCE_PIPE,
  /* TV/DP on pre-gen5/vlv can't use the pipe source. */
  INTEL_PIPE_CRC_SOURCE_TV,
  INTEL_PIPE_CRC_SOURCE_DP_B,
  INTEL_PIPE_CRC_SOURCE_DP_C,
  INTEL_PIPE_CRC_SOURCE_DP_D,
  INTEL_PIPE_CRC_SOURCE_HDMI_B,
  INTEL_PIPE_CRC_SOURCE_HDMI_C,
  INTEL_PIPE_CRC_SOURCE_AUTO,
  INTEL_PIPE_CRC_SOURCE_MAX,
};

enum pipe { PIPE_A = 0, PIPE_B, PIPE_C, I915_MAX_PIPES };
#define pipe_name(p) ((p) + 'A')

//-------------------------------------------------------------------
// END: copied from $TOP/linux/kernel/drivers/gpu/drm/i915/i915_drv.h
//-------------------------------------------------------------------

#define CRC_WORDS 5
#define PIPE_RESULT_WORDS (CRC_WORDS + 1)
#define CRC_RES_FMT_STRING "%8u %8x %8x %8x %8x %8x"

// PIPE_CRC_LINE_LEN: PIPE_RESULT_WORDS fields of 8 chars, space separated
// (PIPE_RESULT_WORDS - 1) + '\n'
#define PIPE_CRC_LINE_LEN (PIPE_RESULT_WORDS * 8 + (PIPE_RESULT_WORDS - 1) + 1)
#define PIPE_CRC_BUFFER_LEN (PIPE_CRC_LINE_LEN + 1)

struct crc_t {
  uint32_t frame;
  int nWords;
  int64_t time_ns;
  uint32_t seconds;
  uint32_t microseconds;
  uint32_t crc[5];
};

struct CRCRes {
  enum pipe pipe;
  enum intel_pipe_crc_source source;
  uint32_t vsync;
  int64_t time_ns;
  uint32_t seconds;
  uint32_t microseconds;
  uint32_t timestampDeltaMicroseconds;
  uint32_t crc[5];
};

inline uint64_t GetTimeDeltaMicroseconds(uint32_t startSeconds,
                                         uint32_t startMicroseconds,
                                         uint32_t nowSeconds,
                                         uint32_t nowMicroseconds) {
  uint64_t deltaMicroseconds = uint64_t(nowMicroseconds) +
                               (uint64_t(nowSeconds - startSeconds) * 1000000);
  deltaMicroseconds -= startMicroseconds;
  return deltaMicroseconds;
}

class Debugfs {
 public:
  Debugfs();
  virtual ~Debugfs() {
  }
  void MakePath(char *buf, size_t bufsize, const char *filename);

 private:
  char mDebugfsRoot[1024];
  char mDebugfsPath[1024];
};

class CRCCtlFile {
 public:
  CRCCtlFile(Debugfs &dgfs);
  virtual ~CRCCtlFile();
  bool EnablePipe(enum pipe pipe, enum intel_pipe_crc_source Source);
  bool DisablePipe(enum pipe pipe);
  bool OpenPipe();
  void ClosePipe();

 private:
  Debugfs mDbgfs;
  int mFd;
};

class CRCDataFile {
 public:
  CRCDataFile(Debugfs &dgfs);
  virtual ~CRCDataFile();

  bool Open(enum pipe pipe);
  void Close();
  bool IsOpen() const;
  enum pipe Pipe() const;
  bool Read(crc_t &crc);

 private:
  Debugfs mDbgfs;
  enum pipe mPipe;
  int mLineLength;
  int mBufferLength;
  int mFd;
};

extern const char *const pipe_crc_sources[];

#endif  //__CrcDebugfs_h__
