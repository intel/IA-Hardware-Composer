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

#include "overlaybuffer.h"

#include <drm/drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <hwcdefs.h>
#include <nativebufferhandler.h>

#include "hwctrace.h"

namespace hwcomposer {

OverlayBuffer::~OverlayBuffer() {
  if (fb_id_ && drmModeRmFB(gpu_fd_, fb_id_))
    ETRACE("Failed to remove fb");
}

void OverlayBuffer::Initialize(const HwcBuffer& bo) {
  width_ = bo.width;
  height_ = bo.height;
  format_ = bo.format;
  for (uint32_t i = 0; i < 4; i++) {
    pitches_[i] = bo.pitches[i];
    offsets_[i] = bo.offsets[i];
    gem_handles_[i] = bo.gem_handles[i];
  }

  reset_framebuffer_ = ((prime_fd_ != bo.prime_fd) || fb_id_ == 0);
  prime_fd_ = bo.prime_fd;
  usage_ = bo.usage;
}

void OverlayBuffer::InitializeFromNativeHandle(
    HWCNativeHandle handle, NativeBufferHandler* buffer_handler) {
  struct HwcBuffer bo;
  if (!buffer_handler->ImportBuffer(handle, &bo)) {
    ETRACE("Failed to Import buffer.");
    return;
  }

  Initialize(bo);
}

bool OverlayBuffer::IsCompatible(const HwcBuffer& bo) const {
  if (prime_fd_ != bo.prime_fd)
    return false;

  if (height_ != bo.height)
    return false;

  if (format_ != bo.format)
    return false;

  if (width_ != bo.width)
    return false;

  if (usage_ != bo.usage)
    return false;

  for (uint32_t i = 0; i < 4; i++) {
    if (pitches_[i] != bo.pitches[i])
      return false;

    if (offsets_[i] != bo.offsets[i])
      return false;

    if (gem_handles_[i] != bo.gem_handles[i])
      return false;
  }

  return true;
}

GpuImage OverlayBuffer::ImportImage(GpuDisplay egl_display) {
#ifdef USE_GL
  EGLImageKHR image = EGL_NO_IMAGE_KHR;
  // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
  // target, the EGL will take a reference to the dma_buf.
  if (format_ == DRM_FORMAT_YUV420) {
    const EGLint attr_list_yv12[] = {
        EGL_WIDTH,                     static_cast<EGLint>(width_),
        EGL_HEIGHT,                    static_cast<EGLint>(height_),
        EGL_LINUX_DRM_FOURCC_EXT,      DRM_FORMAT_YUV420,
        EGL_DMA_BUF_PLANE0_FD_EXT,     static_cast<EGLint>(prime_fd_),
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(pitches_[0]),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(offsets_[0]),
        EGL_DMA_BUF_PLANE1_FD_EXT,     static_cast<EGLint>(prime_fd_),
        EGL_DMA_BUF_PLANE1_PITCH_EXT,  static_cast<EGLint>(pitches_[1]),
        EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(offsets_[1]),
        EGL_DMA_BUF_PLANE2_FD_EXT,     static_cast<EGLint>(prime_fd_),
        EGL_DMA_BUF_PLANE2_PITCH_EXT,  static_cast<EGLint>(pitches_[2]),
        EGL_DMA_BUF_PLANE2_OFFSET_EXT, static_cast<EGLint>(offsets_[2]),
        EGL_NONE,                      0};
    image = eglCreateImageKHR(
        egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
        static_cast<EGLClientBuffer>(nullptr), attr_list_yv12);
  } else {
    const EGLint attr_list[] = {
        EGL_WIDTH,                     static_cast<EGLint>(width_),
        EGL_HEIGHT,                    static_cast<EGLint>(height_),
        EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(format_),
        EGL_DMA_BUF_PLANE0_FD_EXT,     static_cast<EGLint>(prime_fd_),
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(pitches_[0]),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE,                      0};
    image =
        eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                          static_cast<EGLClientBuffer>(nullptr), attr_list);
  }

  return image;
#else
  return NULL;
#endif
}

void OverlayBuffer::SetRecommendedFormat(uint32_t format) {
  format_ = format;
}

bool OverlayBuffer::CreateFrameBuffer(uint32_t gpu_fd) {
  if (!reset_framebuffer_)
    return true;

  if (fb_id_ && gpu_fd_ && drmModeRmFB(gpu_fd_, fb_id_))
    ETRACE("Failed to remove fb");

  int ret = drmModeAddFB2(gpu_fd, width_, height_, format_, gem_handles_,
                          pitches_, offsets_, &fb_id_, 0);

  if (ret) {
    ETRACE("drmModeAddFB2 error (%dx%d, %c%c%c%c, handle %d pitch %d) (%s)",
           width_, height_, format_, format_ >> 8, format_ >> 16, format_ >> 24,
           gem_handles_[0], pitches_[0], strerror(-ret));

    fb_id_ = 0;
    return false;
  }

  reset_framebuffer_ = false;
  gpu_fd_ = gpu_fd;
  return true;
}

void OverlayBuffer::Dump() {
  DUMPTRACE("OverlayBuffer Information Starts. -------------");
  if (usage_ & kLayerNormal)
    DUMPTRACE("BufferUsage: kLayerNormal.");
  if (usage_ & kLayerCursor)
    DUMPTRACE("BufferUsage: kLayerCursor.");
  if (usage_ & kLayerProtected)
    DUMPTRACE("BufferUsage: kLayerProtected.");
  if (usage_ & kLayerVideo)
    DUMPTRACE("BufferUsage: kLayerVideo.");
  DUMPTRACE("Width: %d", width_);
  DUMPTRACE("Height: %d", height_);
  DUMPTRACE("Fb: %d", fb_id_);
  DUMPTRACE("Prime Handle: %d", prime_fd_);
  DUMPTRACE("Format: %4.4s", (char*)&format_);
  for (uint32_t i = 0; i < 4; i++) {
    DUMPTRACE("Pitch:%d value:%d", i, pitches_[i]);
    DUMPTRACE("Offset:%d value:%d", i, offsets_[i]);
    DUMPTRACE("Gem Handles:%d value:%d", i, gem_handles_[i]);
  }
  DUMPTRACE("OverlayBuffer Information Ends. -------------");
}

}  // namespace hwcomposer
