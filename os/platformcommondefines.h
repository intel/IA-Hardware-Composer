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

#ifndef OS_LINUX_PLATFORMCOMMONDEFINES_H_
#define OS_LINUX_PLATFORMCOMMONDEFINES_H_

#ifdef USE_VK
#include <vulkan/vulkan.h>

VkFormat NativeToVkFormat(int native_format);
#endif

#include <hwcmeta.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRM_FORMAT_NONE fourcc_code('0', '0', '0', '0')

#define DRM_FORMAT_NV12_Y_TILED_INTEL fourcc_code('9', '9', '9', '6')
// minigbm specific DRM_FORMAT_YVU420_ANDROID enum
#define DRM_FORMAT_YVU420_ANDROID fourcc_code('9', '9', '9', '7')

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [10:6] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [10:6:10:6] little endian
 */
#define DRM_FORMAT_P010      \
  fourcc_code('P', '0', '1', \
              '0') /* 2x2 subsampled Cr:Cb plane 10 bits per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [12:4] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [12:4:12:4] little endian
 */
#define DRM_FORMAT_P012      \
  fourcc_code('P', '0', '1', \
              '2') /* 2x2 subsampled Cr:Cb plane 12 bits per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:Cb [16:16] little endian
 */
#define DRM_FORMAT_P016      \
  fourcc_code('P', '0', '1', \
              '6') /* 2x2 subsampled Cr:Cb plane 16 bits per channel */

/* 64 bpp RGB */
#define DRM_FORMAT_XRGB161616                                                 \
  fourcc_code('X', 'R', '4', '8') /* [63:0] x:R:G:B 16:16:16:16 little endian \
                                     */
#define DRM_FORMAT_XBGR161616                                                 \
  fourcc_code('X', 'B', '4', '8') /* [63:0] x:B:G:R 16:16:16:16 little endian \
                                     */

#define HWC_UNUSED(x) ((void)&(x))

struct drm_color_ctm_post_offset {
  /* Data is U0.16 fixed point format. */
  __u16 red;
  __u16 green;
  __u16 blue;
};

inline void hash_combine_hwc(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

typedef struct FBKey {
  uint32_t gem_handles_[4];
  uint32_t num_planes_;

  FBKey(const uint32_t &num_planes, const uint32_t (&igem_handles)[4]) {
    for (uint32_t i = 0; i < 4; i++) {
      gem_handles_[i] = igem_handles[i];
    }
    num_planes_ = num_planes;
  }
} FBKey;

int CreateFrameBuffer(
    const uint32_t &iwidth, const uint32_t &iheight, const uint64_t &modifier,
    const uint32_t &iframe_buffer_format, const uint32_t &num_planes,
    const uint32_t (&igem_handles)[4], const uint32_t (&ipitches)[4],
    const uint32_t (&ioffsets)[4], uint32_t gpu_fd, uint32_t *fb_id);

int ReleaseFrameBuffer(const FBKey &key, uint32_t fd, uint32_t gpu_fd);

void *GetVADisplay(uint32_t gpu_fd);

#endif  // OS_LINUX_PLATFORMCOMMONDEFINES_H_
