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

#include "drmbuffer.h"

#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <hwcdefs.h>
#include <nativebufferhandler.h>

#include "hwctrace.h"
#include "resourcemanager.h"

namespace hwcomposer {

DrmBuffer::~DrmBuffer() {
  if (resource_manager_)
    resource_manager_->MarkResourceForDeletion(image_);

  ReleaseFrameBuffer();
}

void DrmBuffer::Initialize(const HwcBuffer& bo) {
  width_ = bo.width_;
  height_ = bo.height_;
  for (uint32_t i = 0; i < 4; i++) {
    pitches_[i] = bo.pitches_[i];
    offsets_[i] = bo.offsets_[i];
    gem_handles_[i] = bo.gem_handles_[i];
  }

  format_ = bo.format_;
  if (format_ == DRM_FORMAT_NV12_Y_TILED_INTEL || format_ == DRM_FORMAT_NV21)
    format_ = DRM_FORMAT_NV12;
  else if (format_ == DRM_FORMAT_YVU420_ANDROID)
    format_ = DRM_FORMAT_YUV420;

  prime_fd_ = bo.prime_fd_;
  usage_ = bo.usage_;

  if (usage_ & hwcomposer::kLayerCursor) {
    // We support DRM_FORMAT_ARGB8888 for cursor.
    frame_buffer_format_ = DRM_FORMAT_ARGB8888;
  } else {
    frame_buffer_format_ = format_;
  }

  switch (format_) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_P010:
      total_planes_ = 2;
      is_yuv_ = true;
      break;
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YUV444:
      is_yuv_ = true;
      total_planes_ = 3;
      break;
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_AYUV:
      is_yuv_ = true;
      total_planes_ = 1;
      break;
    default:
      is_yuv_ = false;
      total_planes_ = 1;
  }
}

void DrmBuffer::InitializeFromNativeHandle(HWCNativeHandle handle,
                                           ResourceManager* resource_manager) {
  const NativeBufferHandler* handler =
      resource_manager->GetNativeBufferHandler();
  handler->CopyHandle(handle, &image_.handle_);
  if (!handler->ImportBuffer(image_.handle_)) {
    ETRACE("Failed to Import buffer.");
    return;
  }

  resource_manager_ = resource_manager;
  Initialize(image_.handle_->meta_data_);
}

const ResourceHandle& DrmBuffer::GetGpuResource(GpuDisplay egl_display,
                                                bool external_import) {
  if (image_.image_ == 0) {
#ifdef USE_GL
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
    // target, the EGL will take a reference to the dma_buf.
    if (is_yuv_ && total_planes_ > 1) {
      if (total_planes_ == 2) {
        const EGLint attr_list_nv12[] = {
            EGL_WIDTH,                     static_cast<EGLint>(width_),
            EGL_HEIGHT,                    static_cast<EGLint>(height_),
            EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(format_),
            EGL_DMA_BUF_PLANE0_FD_EXT,     static_cast<EGLint>(prime_fd_),
            EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(pitches_[0]),
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(offsets_[0]),
            EGL_DMA_BUF_PLANE1_FD_EXT,     static_cast<EGLint>(prime_fd_),
            EGL_DMA_BUF_PLANE1_PITCH_EXT,  static_cast<EGLint>(pitches_[1]),
            EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(offsets_[1]),
            EGL_NONE,                      0};
        image = eglCreateImageKHR(
            egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
            static_cast<EGLClientBuffer>(nullptr), attr_list_nv12);
      } else {
        const EGLint attr_list_yv12[] = {
            EGL_WIDTH,                     static_cast<EGLint>(width_),
            EGL_HEIGHT,                    static_cast<EGLint>(height_),
            EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(format_),
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
      }
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

    image_.image_ = image;
#elif USE_VK
    struct vk_import import;

    PFN_vkCreateDmaBufImageINTEL vkCreateDmaBufImageINTEL =
        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(
            egl_display, "vkCreateDmaBufImageINTEL");
    if (vkCreateDmaBufImageINTEL == NULL) {
      ETRACE("vkGetDeviceProcAddr(\"vkCreateDmaBufImageINTEL\") failed\n");
      import.res = VK_ERROR_INITIALIZATION_FAILED;
      return import;
    }

    VkFormat vk_format = NativeToVkFormat(format_);
    if (vk_format == VK_FORMAT_UNDEFINED) {
      ETRACE("Failed DRM -> Vulkan format conversion\n");
      import.res = VK_ERROR_FORMAT_NOT_SUPPORTED;
      return import;
    }

    VkExtent3D image_extent = {};
    image_extent.width = width_;
    image_extent.height = height_;
    image_extent.depth = 1;

    VkDmaBufImageCreateInfo image_create = {};
    image_create.sType =
        (enum VkStructureType)VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL;
    image_create.fd = static_cast<int>(prime_fd_);
    image_create.format = vk_format;
    image_create.extent = image_extent;
    image_create.strideInBytes = pitches_[0];

    import.res = vkCreateDmaBufImageINTEL(egl_display, &image_create, NULL,
                                          &import.memory, &import.image);

    image_ = import;
#endif
  }

#ifdef USE_GL
  GLenum target = GL_TEXTURE_EXTERNAL_OES;
  if (!external_import) {
    target = GL_TEXTURE_2D;
  }

  if (image_.texture_ != 0) {
    glBindTexture(target, image_.texture_);
    glEGLImageTargetTexture2DOES(target, (GLeglImageOES)image_.image_);
    glBindTexture(target, 0);
  } else {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glEGLImageTargetTexture2DOES(target, (GLeglImageOES)image_.image_);
    if (external_import) {
      glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glBindTexture(target, 0);
    image_.texture_ = texture;
  }
#elif USE_VK
  ETRACE("Missing implementation for Vulkan. \n");
#endif

  return image_;
}

const ResourceHandle& DrmBuffer::GetGpuResource() {
  return image_;
}

void DrmBuffer::SetRecommendedFormat(uint32_t format) {
  frame_buffer_format_ = format;
}

bool DrmBuffer::CreateFrameBuffer(uint32_t gpu_fd) {
  if (fb_id_) {
    // Has been created before
    return true;
  }

  int ret = drmModeAddFB2(gpu_fd, width_, height_, frame_buffer_format_,
                          gem_handles_, pitches_, offsets_, &fb_id_, 0);

  if (ret) {
    ETRACE("drmModeAddFB2 error (%dx%d, %c%c%c%c, handle %d pitch %d) (%s)",
           width_, height_, frame_buffer_format_, frame_buffer_format_ >> 8,
           frame_buffer_format_ >> 16, frame_buffer_format_ >> 24,
           gem_handles_[0], pitches_[0], strerror(-ret));

    fb_id_ = 0;
    return false;
  }

  gpu_fd_ = gpu_fd;
  return true;
}

void DrmBuffer::ReleaseFrameBuffer() {
  if (fb_id_ && gpu_fd_ && drmModeRmFB(gpu_fd_, fb_id_))
    ETRACE("Failed to remove fb %s", PRINTERROR());

  fb_id_ = 0;
}

void DrmBuffer::Dump() {
  DUMPTRACE("DrmBuffer Information Starts. -------------");
  if (usage_ == kLayerNormal)
    DUMPTRACE("BufferUsage: kLayerNormal.");
  if (usage_ == kLayerCursor)
    DUMPTRACE("BufferUsage: kLayerCursor.");
  if (usage_ == kLayerProtected)
    DUMPTRACE("BufferUsage: kLayerProtected.");
  if (usage_ == kLayerVideo)
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
  DUMPTRACE("DrmBuffer Information Ends. -------------");
}

std::shared_ptr<OverlayBuffer> OverlayBuffer::CreateOverlayBuffer() {
  return std::make_shared<DrmBuffer>();
}

}  // namespace hwcomposer
