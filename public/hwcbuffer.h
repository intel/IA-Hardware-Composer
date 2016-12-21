/*
// Copyright (c) 2016 Intel Corporation
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

#ifndef HWC_BUFFER_H_
#define HWC_BUFFER_H_

#include <stdint.h>

struct HwcBuffer {
  uint32_t width;
  uint32_t height;
  uint32_t format;
  uint32_t pitches[4];
  uint32_t offsets[4];
  uint32_t gem_handles[4];
  uint32_t prime_fd;
  uint32_t usage;
};

#endif  // HWC_BUFFER_H_
