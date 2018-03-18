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

#include "platformdefines.h"

#include <drm_fourcc.h>

int CreateFrameBuffer(
    const uint32_t &iwidth, const uint32_t &iheight, const uint64_t &modifier,
    const uint32_t &iframe_buffer_format, const uint32_t &num_planes,
    const uint32_t (&igem_handles)[4], const uint32_t (&ipitches)[4],
    const uint32_t (&ioffsets)[4], uint32_t gpu_fd, uint32_t *fb_id) {
  int ret = 0;
  uint32_t *m_igem_handles = (uint32_t *)igem_handles;
  uint32_t *m_ipitches = (uint32_t *)ipitches;
  uint32_t *m_ioffsets = (uint32_t *)ioffsets;
  if (modifier > 0) {
    uint64_t modifiers[4];
    for (uint32_t i = 0; i < num_planes; i++) {
      modifiers[i] = modifier;
    }

    for (uint32_t i = num_planes; i < 4; i++) {
      modifiers[i] = DRM_FORMAT_MOD_NONE;
    }

    ret = drmModeAddFB2WithModifiers(
        gpu_fd, iwidth, iheight, iframe_buffer_format, m_igem_handles,
        m_ipitches, m_ioffsets, modifiers, fb_id, DRM_MODE_FB_MODIFIERS);
  } else {
    ret = drmModeAddFB2(gpu_fd, iwidth, iheight, iframe_buffer_format,
                        m_igem_handles, m_ipitches, m_ioffsets, fb_id, 0);
  }

  if (ret) {
    ETRACE("%s error (%dx%d, %c%c%c%c, handle %d pitch %d) (%s)",
           (modifier == 0) ? "drmModeAddFB2" : "drmModeAddFB2WithModifiers",
           iwidth, iheight, iframe_buffer_format, iframe_buffer_format >> 8,
           iframe_buffer_format >> 16, iframe_buffer_format >> 24,
           m_igem_handles[0], m_ipitches[0], strerror(-ret));
    *fb_id = 0;
  }

  return ret;
}
