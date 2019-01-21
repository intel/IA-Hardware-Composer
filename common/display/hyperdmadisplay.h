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

#ifndef COMMON_DISPLAY_HYPERDMABUF_H_
#define COMMON_DISPLAY_HYPERDMABUF_H_

#include <memory>
#include <vector>

#include <linux/hyper_dmabuf.h>
#include <map>
#include "drmbuffer.h"
#include "hwctrace.h"
#define SURFACE_NAME_LENGTH 64
#define HYPER_DMABUF_PATH "/dev/hyper_dmabuf"

namespace hwcomposer {

struct vm_header {
  int32_t version;
  int32_t output;
  int32_t counter;
  int32_t n_buffers;
  int32_t disp_w;
  int32_t disp_h;
};

struct vm_buffer_info {
  int32_t surf_index;
  int32_t width, height;
  int32_t format;
  int32_t pitch[3];
  int32_t offset[3];
  int32_t tile_format;
  int32_t rotation;
  int32_t status;
  int32_t counter;
  union {
    hyper_dmabuf_id_t hyper_dmabuf_id;
    unsigned long ggtt_offset;
  };
  char surface_name[SURFACE_NAME_LENGTH];
  uint64_t surface_id;
  int32_t bbox[4];
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_HYPERDMABUF_H_
