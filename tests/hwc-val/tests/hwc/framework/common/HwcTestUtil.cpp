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

#include "HwcTestUtil.h"
#include "HwcTestState.h"
#include <stdlib.h>
#include "hardware/hwcomposer2.h"

void CloseFence(int fence) {
  if (HwcTestState::getInstance()->IsLive()) {
    if (fence) {
      HWCLOGD_COND(eLogFence, "Close fence %d", fence);
      close(fence);
    } else {
      HWCLOGW_COND(eLogFence, "Skipped closing zero fence");
    }
  }
}

// Misc string functions
const char* strafter(const char* str, const char* search) {
  const char* p = strstr(str, search);
  if (p == 0) {
    return 0;
  }

  return p + strlen(search);
}

int strncmpinc(const char*& p, const char* search) {
  int len = strlen(search);

  int cmp = strncmp(p, search, len);

  if (cmp == 0) {
    p += len;
  }

  return cmp;
}

int atoiinc(const char*& p) {
  int ret = atoi(p);
  if ((*p == '-') || (*p == '+')) {
    ++p;
  }

  while (isdigit(*p)) {
    ++p;
  }

  return ret;
}

// Expecting a pointer of form 0xabcd01234567
// Will also accept without the 0x, but an error will be generated.

uintptr_t atoptrinc(const char*& p) {
  // Skip 0x if supplied.
  HWCCHECK(eCheckBadPointerFormat);
  if (strncmpinc(p, "0x") != 0) {
    HWCERROR(eCheckBadPointerFormat,
             "0x missing from value: pointer formatting should be used");
  }

  uintptr_t h = strtoll(p, 0, 16);

  while (isdigit(*p) || ('a' <= *p && *p <= 'f') || ('A' <= *p && *p <= 'F')) {
    ++p;
  }

  return h;
}

double atofinc(const char*& p) {
  double ret = atof(p);
  while (isdigit(*p) || (*p == '.') || (*p == '-') || (*p == '+')) {
    ++p;
  }

  return ret;
}

void skipws(const char*& p) {
  while (isblank(*p)) {
    ++p;
  }
}

std::string getWord(const char*& p) {
  uint32_t len = 0;
  while (p[len] && !isblank(p[len]) && (p[len] != '\n')) {
    ++len;
  }

  std::string ret(p, len);
  p += len;

  return ret;
}

bool ExpectChar(const char*& p, char c) {
  if (*p != c) {
    HWCLOGV_COND(eLogParse, "Expecting '%c': %s", c, p);
    return false;
  } else {
    p++;
    return true;
  }
}

const char* TriStateStr(TriState ts) {
  switch (ts) {
    case eTrue:
      return "TRUE";
    case eFalse:
      return "FALSE";
    case eUndefined:
      return "UNDEFINED";
    default:
      return "INVALID";
  }
}

const char* FormatToStr(uint32_t fmt) {
#define PRINT_FMT(FMT) \
  if (fmt == FMT) {    \
    return #FMT;       \
  }

  PRINT_FMT(DRM_FORMAT_ABGR8888)
  else PRINT_FMT(DRM_FORMAT_ARGB8888) else PRINT_FMT(DRM_FORMAT_XBGR8888) else PRINT_FMT(
      DRM_FORMAT_RGB565) else PRINT_FMT(DRM_FORMAT_NV12_Y_TILED_INTEL) else PRINT_FMT(DRM_FORMAT_NV12)

      else PRINT_FMT(DRM_FORMAT_YUYV) else {
    return "UNKNOWN";
  }
#undef PRINT_FMT
}

// Determines whether this buffer is a video format
bool IsNV12(uint32_t format) {
  return ((format == DRM_FORMAT_NV12_Y_TILED_INTEL) ||
          (format == DRM_FORMAT_NV12));
}

bool HasAlpha(uint32_t format) {
  return ((format == DRM_FORMAT_ABGR8888) ||
          (format == DRM_FORMAT_ARGB8888));
}

void* dll_open(const char* filename, int flag) {
  void* st = dlopen(filename, flag);

  if (st == 0) {
    ALOGE("dlopen failed to open %s, errno=%d/%s", filename, errno,
          strerror(errno));
    ALOGE("%s", dlerror());
  }

  return st;
}

void DumpMemoryUsage() {
  // /linux/kernl/fs/proc/array.c shows us how to read the data in
  // /proc/self/state

  if (HwcTestState::getInstance()->IsOptionEnabled(eLogResources)) {
    FILE* f = fopen("/proc/self/stat", "r");
    if (f) {
      uint32_t d;
      uint32_t vm;
      char s[HWCVAL_DEFAULT_STRLEN];
      fscanf(f,
             "%s %s %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
             "%d %d "
             "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
             "%d %d %d %d %d %d %d",
             s, s, s, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d,
             &d, &d, &d, &d, &d, &vm, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d,
             &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d,
             &d);
      fclose(f);

      HWCLOGA("VM USAGE: %4.1fMB", double(vm) / 1000000);
    } else {
      HWCLOGW("Can't open /proc/self/stat");
    }
  }
}

std::vector<std::string> splitString(std::string str) {

  std::vector<std::string> sv;
  size_t startpos = 0, endpos = 0;

  while ((endpos = str.find_first_of(" ", startpos)) != -1) {
    sv.push_back( str.substr( startpos, endpos - startpos));
    startpos = endpos + 1;
  }
  return sv;
}


std::vector<char*> splitString(char* str) {
  std::vector<char*> sv;
  char* s = str;
  while (*s) {
    sv.push_back(s);
    while (*s && (*s != ' ')) {
      ++s;
    }

    if (*s == ' ') {
      *s = '\0';
      ++s;
    }
  }

  return sv;
}


Hwcval::FrameNums::operator std::string() const {
  std::string str = std::string("frame:") + std::to_string( mFN[0]);

  for (uint32_t i = 1; i < HWCVAL_MAX_CRTCS; ++i) {
    str += std::string(".") + std::to_string(mFN[i]);
  }

  return str;
}
