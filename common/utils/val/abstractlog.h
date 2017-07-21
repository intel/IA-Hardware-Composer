/*
// Copyright (c) 2017 Intel Corporation
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

#ifndef COMMON_UTILS_VAL_ABSTRACTLOG_H_
#define COMMON_UTILS_VAL_ABSTRACTLOG_H_

#include <utils/Timers.h>
#include <string>

namespace hwcomposer {

#define HWCLOG_STRING_RESERVATION_SIZE 1024

#define OS_ANDROID_HWC_TIMESTAMP_STR "%" PRIi64 "s %03" PRIi64 "ms"
#define OS_ANDROID_HWC_TIMESTAMP_PARAM(T) \
  (T) / 1000000000, ((T) % 1000000000) / 1000000

// This is primarily a debug logging class expected to generate data thats
// expected
// to be used by the validation team to check that the HWC is operating
// correctly.
class AbstractLogWrite {
 public:
  virtual char* reserve(uint32_t maxSize) = 0;
  virtual void log(char* endPtr) = 0;

  static const size_t cStrOffset = sizeof(pid_t) + sizeof(uint64_t);

 private:
  // Helper template to write the specified value to the byte buffer and advance
  // the pointer to the next byte after that value
  template <typename T>
  void serialize(char*& pBuffer, const T value) {
    *(reinterpret_cast<T*>(pBuffer)) = value;
    pBuffer += (sizeof(T));
  }

  // Helper template to read the specified value from the byte buffer, return
  // the value and advance the pointer to the next byte after that value
  template <typename T>
  const T& unserialize(const char*& ptr) {
    T* data = (T*)ptr;
    ptr += sizeof(T);
    return *data;
  }

 public:
  virtual ~AbstractLogWrite() {
  }

  // Defined here rather than in .cpp so they can be used by validation
  // subclass.
  // The alternative would be a library.
  const char* addV(const char* description, va_list& args) {
    // Determine space requirement to flatten the layers
    size_t logAllocSize = sizeof(int);

    // ... and an amount for the tid, time, and a fixed allocation for the
    // string
    logAllocSize += sizeof(pid_t) + sizeof(uint64_t) + strlen(description) +
                    HWCLOG_STRING_RESERVATION_SIZE;

    // Allocate a log entry with enough space for all that
    char* entry = reserve(logAllocSize);

    if (entry == 0) {
      return "";
    }

    char* ptr = entry;

    // Write the tid
    pid_t threadid = gettid();
    serialize(ptr, threadid);

    // Write the time
    nsecs_t timestamp = systemTime(CLOCK_MONOTONIC);
    serialize(ptr, timestamp);

    // Write the formatted string
    int maxlen = entry + logAllocSize - ptr;
    int len = vsnprintf(ptr, maxlen, description, args) + 1;
    // NB string will be null-terminated within maxlen characters

    if (len > maxlen) {
      len = maxlen;
    }

    log(ptr + len);
    return ptr;
  }

  // Defined here rather than in .cpp so they can be used by validation
  // subclass.
  // The alternative would be a library.
  const char* add(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char* ptr = addV(fmt, args);
    va_end(args);
    return ptr;
  }

  const char* unpack(const char* ptr, pid_t& pid, int64_t& timestamp) {
    pid = unserialize<pid_t>(ptr);
    timestamp = unserialize<int64_t>(ptr);
    return ptr;
  }

 protected:
  void logToLogcat(const char* ptr) {
    pid_t threadid;
    nsecs_t timestamp;
    const char* str = unpack(ptr, threadid, timestamp);
    // Long multi line strings to logcat get truncated when they are too large.
    // Hence, break up the output into one logcat entry per line of text.
    std::string inputstr(str);
    std::string delimiter("\n");
    size_t pos = 0;
    std::string token;
    while ((pos = inputstr.find(delimiter)) != std::string::npos) {
      token = inputstr.substr(0, pos);
      ALOGI("%s", token.c_str());
      inputstr.erase(0, pos + delimiter.length());
    }
  }
};

class AbstractLogRead {
 public:
  virtual ~AbstractLogRead() {
  }

  virtual char* read(uint32_t& size, bool& lost) = 0;
};

};  // namespace hwcomposer

#endif  // COMMON_UTILS_VAL_ABSTRACTLOG_H_
