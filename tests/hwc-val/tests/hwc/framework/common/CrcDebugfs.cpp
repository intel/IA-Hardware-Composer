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

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <utils/Timers.h>

//#include <Debug.h>
#include "HwcTestState.h"
#include "HwcTestLog.h"
#include "CrcDebugfs.h"

const char *const pipe_crc_sources[] = {
    "none", "plane1", "plane2", "pf",     "pipe",   "TV",
    "DP-B", "DP-C",   "DP-D",   "HDMI-B", "HDMI-C", "auto",
};

Debugfs::Debugfs() {
  struct stat st;

  memset(mDebugfsRoot, 0, sizeof mDebugfsRoot);
  memset(mDebugfsPath, 0, sizeof mDebugfsPath);

  if (stat("/d/dri", &st) == 0) {
    // debugfs mounted under /d
    strcpy(mDebugfsRoot, "/d");
  } else {
    if (stat("/sys/kernel/debug/dri", &st)) {
      // debugfs isn't mounted
      if (stat("/sys/kernel/debug", &st)) {
        HWCLOGE("Debugfs::Debugfs - /sys/kernel/debug error %d", errno);
        return;
      }
      if (mount("debug", "/sys/kernel/debug", "debugfs", 0, 0)) {
        HWCLOGE("Debugfs::Debugfs - can't mount /sys/kernel/debug error %d",
                errno);
        return;
      }
    }
    // debugfs mounted under /sys/kernel/debug
    strcpy(mDebugfsRoot, "/sys/kernel/debug");
  }

  for (int n = 0; n < 16; ++n) {
    int len = snprintf(mDebugfsPath, sizeof mDebugfsPath - 1, "%s/dri/%d",
                       mDebugfsRoot, n);
    strcat(&mDebugfsPath[len], "/i915_error_state");
    if (stat(mDebugfsPath, &st) == 0) {
      mDebugfsPath[len] = '\0';
      return;
    }
  }

  HWCLOGE("Debugfs::Debugfs - can't find debugfs");
  mDebugfsPath[0] = '\0';
}

void Debugfs::MakePath(char *buf, size_t bufsize, const char *filename) {
  snprintf(buf, bufsize - 1, "%s/%s", mDebugfsPath, filename);
}

CRCCtlFile::CRCCtlFile(Debugfs &dgfs) : mDbgfs(dgfs), mFd(-1) {
}

CRCCtlFile::~CRCCtlFile() {
}

bool CRCCtlFile::OpenPipe() {
  char filename[1024];

  HWCLOGD("CRCCtlFile::OpenPipe - called");
  mDbgfs.MakePath(filename, sizeof filename, "i915_display_crc_ctl");
  mFd = open(filename, O_WRONLY);
  if (mFd == -1) {
    HWCLOGE("CRCCtlFile::OpenPipe - ERROR can't open %s, error %d", filename,
            errno);
    return false;
  }
  return true;
}

void CRCCtlFile::ClosePipe() {
  HWCLOGI("CRCCtlFile::ClosePipe - called");
  if (mFd != -1) {
    close(mFd);
    mFd = -1;
  }
}

bool CRCCtlFile::EnablePipe(enum pipe pipe, enum intel_pipe_crc_source source) {
  char buf[64];

  ATRACE_CALL();
  HWCLOGD("CRCCtlFile::EnablePipe(%c, %s) - called", pipe_name(pipe),
          pipe_crc_sources[source]);

  OpenPipe();
  if (mFd == -1) {
    HWCLOGE("CRCCtlFile::EnablePipe - invalid mFd");
    return false;
  } else {
    int n = snprintf(buf, sizeof buf - 1, "pipe %c %s", pipe_name(pipe),
                     pipe_crc_sources[source]);
    if (n > 0) {
      write(mFd, buf, n);
    }
  }
  HWCLOGD("CRCCtlFile::EnablePipe - returning");
  return true;
}

bool CRCCtlFile::DisablePipe(enum pipe pipe) {
  char buf[64];

  ATRACE_CALL();
  HWCLOGI("CRCCtlFile::DisablePipe - called");
  if (mFd == -1) {
    HWCLOGE("CRCCtlFile::DisablePipe - invalid mFd");
    return false;
  } else {
    snprintf(buf, sizeof buf - 1, "pipe %c none", pipe_name(pipe));
    HWCLOGI("CRCCtlFile::DisablePipe - sending command '%s'", buf);
    write(mFd, buf, strlen(buf));
  }
  ClosePipe();
  HWCLOGI("CRCCtlFile::DisablePipe - returning");
  return true;
}

CRCDataFile::CRCDataFile(Debugfs &dgfs)
    : mDbgfs(dgfs),
      mPipe(PIPE_A),
      mLineLength(PIPE_CRC_LINE_LEN),
      mBufferLength(PIPE_CRC_LINE_LEN + 1),
      mFd(-1) {
}

CRCDataFile::~CRCDataFile() {
  Close();
}

bool CRCDataFile::Open(enum pipe pipe) {
  char buf[64];
  char filename[1024];

  ATRACE_CALL();
  HWCLOGD("CRCDataFile::Open(%c) - called", pipe_name(pipe));
  mPipe = pipe;
  snprintf(buf, sizeof buf - 1, "i915_pipe_%c_crc", pipe_name(pipe));
  mDbgfs.MakePath(filename, sizeof filename, buf);
  mFd = open(filename, O_RDONLY);
  if (mFd == -1) {
    HWCLOGE("CRCDataFile::Open - ERROR can't open %s, error %d", filename,
            errno);
    return false;
  }
  HWCLOGD("CRCDataFile::Open - returning");
  return true;
}

void CRCDataFile::Close() {
  ATRACE_CALL();
  HWCLOGD("CRCDataFile::Close - called");
  if (mFd != -1) {
    close(mFd);
    mFd = -1;
  }
  HWCLOGD("CRCDataFile::Close - returning");
}

bool CRCDataFile::IsOpen() const {
  return mFd != -1;
}

enum pipe CRCDataFile::Pipe() const {
  return mPipe;
}

bool CRCDataFile::Read(crc_t &crc) {
  char buf[mBufferLength];
  ssize_t bytes;
  int items;

  if (mFd == -1) {
    HWCLOGE("CRCDataFile::Read - ERROR, mFd invalid");
    return false;
  } else {
    bytes = read(mFd, buf, mLineLength);
    if (bytes != mLineLength) {
      HWCLOGE(
          "CRCDataFile::Read - pipe(%c) - ERROR expected %d bytes, only read "
          "%d",
          pipe_name(mPipe), mLineLength, bytes);
      return false;
    }
  }

  buf[bytes] = '\0';
  crc.nWords = 5;

  ATRACE_BEGIN("CRCDataFile::Read - Applying timestamp [delta]");
  items = sscanf(buf, CRC_RES_FMT_STRING, &crc.frame, &crc.crc[0], &crc.crc[1],
                 &crc.crc[2], &crc.crc[3], &crc.crc[4]);

  // Timestamp the CRC res. This is less than ideal, as there is an inevitable
  // delay between
  // debugfs writing to the CRC results file and the CRC reader thread reading
  // from it. However,
  // it's better than nothing and will hopefully be accurate enough to give us a
  // reasonable idea
  // of the order of set/vsync events.
  //
  int64_t ns = systemTime(SYSTEM_TIME_MONOTONIC);
  int64_t rem;

  crc.time_ns = ns;
  crc.seconds = ns / seconds_to_nanoseconds(1);
  rem = ns - (crc.seconds * seconds_to_nanoseconds(1));
  crc.microseconds = nanoseconds_to_microseconds(rem);

  ATRACE_END();

  if (items != PIPE_RESULT_WORDS) {
    // this suggests debugfs is outputting the data in a different format from
    // that we're expecting
    HWCLOGE("CRCDataFile::Read - ERROR expected %d items, only read %d",
            PIPE_RESULT_WORDS, items);
    return false;
  }

  HWCLOGI_IF(PIPE_CRC_DEBUG_CRC_IFACE,
             "CRCDataFile::Read - pipe(%c) crc = %08x-%08x-%08x-%08x-%08x",
             pipe_name(mPipe), crc.crc[0], crc.crc[1], crc.crc[2], crc.crc[3],
             crc.crc[4]);
  return true;
}
