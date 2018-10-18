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

#ifndef __HwcTestUtil_h__
#define __HwcTestUtil_h__
#include <drm_fourcc.h>

#include <sys/types.h>
#include <utils/Vector.h>

#if ANDROID_VERSION < 430
#include <sync.h>
#else
#if ANDROID_VERSION < 500
#include SW_SYNC_H_PATH
#else
#include <sync.h>
#include <sw_sync.h>
#endif
#endif

#include <utils/Atomic.h>
#include <ctype.h>

#include "HwcTestDefs.h"
#include "HwcvalDebug.h"

namespace android {
class String8;
}

/// General purpose variable swap
template <class T>
inline void swap(T& a, T& b) {
  T c = a;
  a = b;
  b = c;
}

template <class T>
inline T min(T a, T b) {
  return (a < b) ? a : b;
}

template <class T>
inline T max(T a, T b) {
  return (a > b) ? a : b;
}

inline int32_t android_atomic_swap(int32_t value, volatile int32_t* addr) {
  int32_t oldValue;
  do {
    oldValue = *addr;
  } while (android_atomic_cmpxchg(oldValue, value, addr));
  return oldValue;
}

class Trylock {
 public:
  inline Trylock(Hwcval::Mutex& mutex) : mLock(mutex) {
    mLock.tryLock();
  }

  inline ~Trylock() {
    if (mLock.isHeld()) {
      mLock.unlock();
    }
  }

  inline bool IsLocked() {
    return mLock.isHeld();
  }

 private:
  Hwcval::Mutex& mLock;
};

void CloseFence(int fence);

// Suppress warning on unused parameter
#define HWCVAL_UNUSED(x) ((void)&(x))

namespace Hwcval {
// Copied from HWC
class NonCopyable {
 protected:
  NonCopyable() {
  }

 private:  // do not implement!
  NonCopyable(NonCopyable const&);
  void operator=(NonCopyable const&);
};

template <typename TYPE>
class Singleton : NonCopyable {
 public:
  static TYPE& getInstance() {
    return sInstance;
  }

 private:
  static TYPE sInstance;
};

template <typename TYPE>
TYPE Singleton<TYPE>::sInstance;

// The purpose of using a class rather than a typedef here is so that the
// array can be copied as a whole. You can't do that with a native array
// (without using memcpy).
class FrameNums {
 private:
  uint32_t mFN[HWCVAL_MAX_CRTCS];

 public:
  uint32_t& operator[](std::size_t ix) {
    return mFN[ix];
  }

  uint32_t GetFrame(uint32_t d) const {
    return mFN[d];
  }

  explicit operator std::string() const;
};
}

// Return a string for a HAL format
const char* FormatToStr(uint32_t fmt);

// Format categorization functions
bool IsNV12(uint32_t format);
bool HasAlpha(uint32_t format);

template <class C>
std::vector<C>& operator+=(std::vector<C>& v1,
                               const std::vector<C>& v2) {
  for (uint32_t i = 0; i < v2.size(); ++i) {
    v1.push_back(v2.at(i));
  }

  return v1;
}

template <class C>
std::vector<C> operator+(const std::vector<C> v1,
                             const std::vector<C>& v2) {
  v1 += v2;
  return v1;
}

// Misc string functions
const char* strafter(const char* str, const char* search);
int strncmpinc(const char*& p, const char* search);
int atoiinc(const char*& p);
uintptr_t atoptrinc(const char*& p);
double atofinc(const char*& p);
void skipws(const char*& p);
std::string getWord(const char*& p);
bool ExpectChar(const char*& p, char c);
std::vector<std::string> splitString(std::string str);
std::vector<char*> splitString(char* str);

enum TriState { eFalse = 0, eTrue, eUndefined };

const char* TriStateStr(TriState ts);

// Strong OR: i.e. "defined" (whether true or false) always wins
inline TriState operator||(TriState a, TriState b) {
  switch (a) {
    case eTrue:
      return eTrue;
    case eFalse:
      if (b == eTrue) {
        return eTrue;
      } else {
        return eFalse;
      }
    default:
      return b;
  }
}

// Wrapped version of dlopen
void* dll_open(const char* filename, int flag);

// For monitoring memory when we think we have a leak
void DumpMemoryUsage();

// Stringification
// stringify usage: S(USER) or S(USER_VS) when you need the string form.
#define S(U) S_(U)
#define S_(U) #U

#endif  // __HwcTestUtil_h__
