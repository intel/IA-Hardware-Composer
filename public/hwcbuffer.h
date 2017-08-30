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

#ifndef PUBLIC_HWCBUFFER_H_
#define PUBLIC_HWCBUFFER_H_

#include <stdint.h>
#include <unistd.h>

struct HwcBuffer {
  HwcBuffer() = default;

  HwcBuffer(const HwcBuffer &rhs) = delete;
  HwcBuffer &operator=(const HwcBuffer &rhs) = delete;

  uint32_t width;
  uint32_t height;
  uint32_t format;
  uint32_t pitches[4];
  uint32_t offsets[4];
  uint32_t gem_handles[4];
  uint32_t prime_fd = 0;
  uint32_t usage;
};

// Buffer usage is defined similar with GBM usage
// To avoid directly include gbm.h 

#define HW_BUFFER_USE_SCANOUT		(1 << 0)
#define HW_BUFFER_USE_CURSOR		(1 << 1)
#define HW_BUFFER_USE_RENDERING	    (1 << 2)
#define HW_BUFFER_USE_WRITE	        (1 << 3)
#define HW_BUFFER_USE_LINEAR	    (1 << 4)
#define HW_BUFFER_USE_TEXTURING	    (1 << 5)
#define HW_BUFFER_USE_CAMERA_WRITE  (1 << 6)
#define HW_BUFFER_USE_CAMERA_READ   (1 << 7)
#define HW_BUFFER_USE_X_TILED       (1 << 8)
#define HW_BUFFER_USE_Y_TILED          (1 << 9)


#endif  // PUBLIC_HWCBUFFER_H_
