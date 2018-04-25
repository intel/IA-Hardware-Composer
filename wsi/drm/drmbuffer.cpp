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

#include "framebuffermanager.h"
#include "hwctrace.h"
#include "hwcutils.h"
#include "resourcemanager.h"
#include "vautils.h"

#include <va/va_drmcommon.h>

#ifndef EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#endif

#ifndef EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#endif

#ifndef EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT 0x3445
#endif

#ifndef EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT 0x3446
#endif

namespace hwcomposer {

DrmBuffer::~DrmBuffer() {
  bool texture_initialized = false;
#if USE_GL
  texture_initialized = image_.texture_ > 0;
#elif USE_VK
  texture_initialized = image_.texture_ != VK_NULL_HANDLE;
#endif

  if (media_image_.surface_ == VA_INVALID_ID) {
    resource_manager_->MarkResourceForDeletion(image_, texture_initialized);
  } else {
    if (texture_initialized) {
      image_.handle_ = 0;
      image_.drm_fd_ = 0;
      resource_manager_->MarkResourceForDeletion(image_, true);
    }

    resource_manager_->MarkMediaResourceForDeletion(media_image_);
  }
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

  tiling_mode_ = bo.tiling_mode_;
  usage_ = bo.usage_;

  if (usage_ == hwcomposer::kLayerCursor) {
    // We support DRM_FORMAT_ARGB8888 for cursor.
    frame_buffer_format_ = DRM_FORMAT_ARGB8888;
  } else {
    frame_buffer_format_ = format_;
  }

  FrameBufferManager::GetInstance()->RegisterGemHandles(
      image_.handle_->meta_data_.num_planes_,
      image_.handle_->meta_data_.gem_handles_);
}

void DrmBuffer::InitializeFromNativeHandle(HWCNativeHandle handle,
                                           ResourceManager* resource_manager) {
  resource_manager_ = resource_manager;
  const NativeBufferHandler* handler =
      resource_manager_->GetNativeBufferHandler();

  handler->CopyHandle(handle, &image_.handle_);
  if (!handler->ImportBuffer(image_.handle_)) {
    ETRACE("Failed to Import buffer.");
    return;
  }

  media_image_.handle_ = image_.handle_;
  Initialize(image_.handle_->meta_data_);
  original_handle_ = handle;
}

const ResourceHandle& DrmBuffer::GetGpuResource(GpuDisplay egl_display,
                                                bool external_import) {
#if USE_GL
  if (image_.image_ == 0) {
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    uint32_t total_planes = image_.handle_->meta_data_.num_planes_;
    // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
    // target, the EGL will take a reference to the dma_buf.
    if ((usage_ == kLayerVideo) && total_planes > 1) {
      if (total_planes == 2) {
        const EGLint attr_list_nv12[] = {
            EGL_WIDTH,
            static_cast<EGLint>(width_),
            EGL_HEIGHT,
            static_cast<EGLint>(height_),
            EGL_LINUX_DRM_FOURCC_EXT,
            static_cast<EGLint>(format_),
            EGL_DMA_BUF_PLANE0_FD_EXT,
            static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[0]),
            EGL_DMA_BUF_PLANE0_PITCH_EXT,
            static_cast<EGLint>(pitches_[0]),
            EGL_DMA_BUF_PLANE0_OFFSET_EXT,
            static_cast<EGLint>(offsets_[0]),
            EGL_DMA_BUF_PLANE1_FD_EXT,
            static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[1]),
            EGL_DMA_BUF_PLANE1_PITCH_EXT,
            static_cast<EGLint>(pitches_[1]),
            EGL_DMA_BUF_PLANE1_OFFSET_EXT,
            static_cast<EGLint>(offsets_[1]),
            EGL_NONE,
            0};
        image = eglCreateImageKHR(
            egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
            static_cast<EGLClientBuffer>(nullptr), attr_list_nv12);
      } else {
        const EGLint attr_list_yv12[] = {
            EGL_WIDTH,
            static_cast<EGLint>(width_),
            EGL_HEIGHT,
            static_cast<EGLint>(height_),
            EGL_LINUX_DRM_FOURCC_EXT,
            static_cast<EGLint>(format_),
            EGL_DMA_BUF_PLANE0_FD_EXT,
            static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[0]),
            EGL_DMA_BUF_PLANE0_PITCH_EXT,
            static_cast<EGLint>(pitches_[0]),
            EGL_DMA_BUF_PLANE0_OFFSET_EXT,
            static_cast<EGLint>(offsets_[0]),
            EGL_DMA_BUF_PLANE1_FD_EXT,
            static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[1]),
            EGL_DMA_BUF_PLANE1_PITCH_EXT,
            static_cast<EGLint>(pitches_[1]),
            EGL_DMA_BUF_PLANE1_OFFSET_EXT,
            static_cast<EGLint>(offsets_[1]),
            EGL_DMA_BUF_PLANE2_FD_EXT,
            static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[2]),
            EGL_DMA_BUF_PLANE2_PITCH_EXT,
            static_cast<EGLint>(pitches_[2]),
            EGL_DMA_BUF_PLANE2_OFFSET_EXT,
            static_cast<EGLint>(offsets_[2]),
            EGL_NONE,
            0};
        image = eglCreateImageKHR(
            egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
            static_cast<EGLClientBuffer>(nullptr), attr_list_yv12);
      }
    } else if (image_.handle_->meta_data_.fb_modifiers_[0] > 0 &&
               total_planes == 2) {
      EGLint modifier_low =
          static_cast<EGLint>(image_.handle_->meta_data_.fb_modifiers_[1]);
      EGLint modifier_high =
          static_cast<EGLint>(image_.handle_->meta_data_.fb_modifiers_[0]);
      const EGLint image_attrs[] = {
          EGL_WIDTH,
          static_cast<EGLint>(width_),
          EGL_HEIGHT,
          static_cast<EGLint>(height_),
          EGL_LINUX_DRM_FOURCC_EXT,
          static_cast<EGLint>(format_),
          EGL_DMA_BUF_PLANE0_FD_EXT,
          static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[0]),
          EGL_DMA_BUF_PLANE0_PITCH_EXT,
          static_cast<EGLint>(pitches_[0]),
          EGL_DMA_BUF_PLANE0_OFFSET_EXT,
          static_cast<EGLint>(offsets_[0]),
          EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
          modifier_low,
          EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
          modifier_high,
          EGL_DMA_BUF_PLANE1_FD_EXT,
          static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[1]),
          EGL_DMA_BUF_PLANE1_PITCH_EXT,
          static_cast<EGLint>(pitches_[1]),
          EGL_DMA_BUF_PLANE1_OFFSET_EXT,
          static_cast<EGLint>(offsets_[1]),
          EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
          modifier_low,
          EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
          modifier_high,
          EGL_NONE,
      };

      image =
          eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                            static_cast<EGLClientBuffer>(nullptr), image_attrs);
    } else {
      const EGLint attr_list[] = {
          EGL_WIDTH,
          static_cast<EGLint>(width_),
          EGL_HEIGHT,
          static_cast<EGLint>(height_),
          EGL_LINUX_DRM_FOURCC_EXT,
          static_cast<EGLint>(format_),
          EGL_DMA_BUF_PLANE0_FD_EXT,
          static_cast<EGLint>(image_.handle_->meta_data_.prime_fds_[0]),
          EGL_DMA_BUF_PLANE0_PITCH_EXT,
          static_cast<EGLint>(pitches_[0]),
          EGL_DMA_BUF_PLANE0_OFFSET_EXT,
          0,
          EGL_NONE,
          0};
      image =
          eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                            static_cast<EGLClientBuffer>(nullptr), attr_list);
    }

    if (image == EGL_NO_IMAGE_KHR) {
      ETRACE("eglCreateKHR failed to create image for DrmBuffer");
    }
    image_.image_ = image;

  }

  GLenum target = GL_TEXTURE_EXTERNAL_OES;
  if (!external_import) {
    target = GL_TEXTURE_2D;
  }

  if (!image_.texture_) {
    GLuint texture;
    glGenTextures(1, &texture);
    image_.texture_ = texture;
  }

  glBindTexture(target, image_.texture_);
  glEGLImageTargetTexture2DOES(target, (GLeglImageOES)image_.image_);

  glBindTexture(target, 0);

  if (!external_import && image_.fb_ == 0) {
    glGenFramebuffers(1, &image_.fb_);
  }
#elif USE_VK
  if (image_.image_ == VK_NULL_HANDLE) {
    VkDevice dev = egl_display;
    VkResult res;

    PFN_vkCreateDmaBufImageINTEL vkCreateDmaBufImageINTEL =
        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(
            dev, "vkCreateDmaBufImageINTEL");
    if (vkCreateDmaBufImageINTEL == NULL) {
      ETRACE("vkGetDeviceProcAddr(\"vkCreateDmaBufImageINTEL\") failed\n");
    }

    VkFormat vk_format = NativeToVkFormat(format_);
    if (vk_format == VK_FORMAT_UNDEFINED) {
      ETRACE("Failed DRM -> Vulkan format conversion\n");
    }

    VkExtent3D image_extent = {};
    image_extent.width = width_;
    image_extent.height = height_;
    image_extent.depth = 1;

    VkDmaBufImageCreateInfo image_create = {};
    image_create.sType =
        (enum VkStructureType)VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL;
    image_create.fd =
        static_cast<int>(image_.handle_->meta_data_.prime_fds_[0]);
    image_create.format = vk_format;
    image_create.extent = image_extent;
    image_create.strideInBytes = pitches_[0];

    res = vkCreateDmaBufImageINTEL(dev, &image_create, NULL, &image_.memory_,
                                   &image_.image_);
    if (res != VK_SUCCESS) {
      ETRACE("vkCreateDmaBufImageINTEL failed\n");
    }
  }
#endif
  return image_;
}

const MediaResourceHandle& DrmBuffer::GetMediaResource(MediaDisplay display,
                                                       uint32_t width,
                                                       uint32_t height) {
  if (media_image_.surface_ != VA_INVALID_ID) {
    if ((previous_width_ == width) && (previous_height_ == height)) {
      return media_image_;
    }

    MediaResourceHandle media_resource;
    media_resource.surface_ = media_image_.surface_;
    media_image_.surface_ = VA_INVALID_ID;
    resource_manager_->MarkMediaResourceForDeletion(media_resource);
  }

  previous_width_ = width;
  previous_height_ = height;

  VASurfaceAttribExternalBuffers external;
  memset(&external, 0, sizeof(external));
  uint32_t rt_format = DrmFormatToRTFormat(format_);
  uint32_t total_planes = image_.handle_->meta_data_.num_planes_;
  external.pixel_format = DrmFormatToVAFormat(format_);
  external.width = width_;
  external.height = height_;
  external.num_planes = total_planes;
#if VA_MAJOR_VERSION < 1
  unsigned long prime_fds[total_planes];
#else
  uintptr_t prime_fds[total_planes];
#endif
  for (unsigned int i = 0; i < total_planes; i++) {
    external.pitches[i] = pitches_[i];
    external.offsets[i] = offsets_[i];
    prime_fds[i] = image_.handle_->meta_data_.prime_fds_[i];
  }

  external.num_buffers = total_planes;
  external.buffers = prime_fds;

  VASurfaceAttrib attribs[2];
  attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs[0].type = VASurfaceAttribMemoryType;
  attribs[0].value.type = VAGenericValueTypeInteger;
  attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

  attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  attribs[1].value.type = VAGenericValueTypePointer;
  attribs[1].value.value.p = &external;

  VAStatus ret =
      vaCreateSurfaces(display, rt_format, external.width, external.height,
                       &media_image_.surface_, 1, attribs, 2);
  if (ret != VA_STATUS_SUCCESS)
    ETRACE("Failed to create VASurface from drmbuffer with ret %x", ret);

  return media_image_;
}

const ResourceHandle& DrmBuffer::GetGpuResource() {
  return image_;
}

bool DrmBuffer::CreateFrameBuffer() {
  if (image_.drm_fd_) {
    return true;
  }

  image_.drm_fd_ = 0;
  media_image_.drm_fd_ = 0;

  image_.drm_fd_ = FrameBufferManager::GetInstance()->FindFB(
      width_, height_, 0, frame_buffer_format_,
      image_.handle_->meta_data_.num_planes_, gem_handles_, pitches_, offsets_);
  media_image_.drm_fd_ = image_.drm_fd_;
  return true;
}

bool DrmBuffer::CreateFrameBufferWithModifier(uint64_t modifier) {
  if (image_.drm_fd_) {
    return true;
  }

  image_.drm_fd_ = 0;
  media_image_.drm_fd_ = 0;

  image_.drm_fd_ = FrameBufferManager::GetInstance()->FindFB(
      width_, height_, modifier, frame_buffer_format_,
      image_.handle_->meta_data_.num_planes_, gem_handles_, pitches_, offsets_);
  media_image_.drm_fd_ = image_.drm_fd_;
  return true;
}

void DrmBuffer::SetOriginalHandle(HWCNativeHandle handle) {
  original_handle_ = handle;
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
  DUMPTRACE("Fb: %d", image_.drm_fd_);
  DUMPTRACE("Prime Handle: %d", image_.handle_->meta_data_.prime_fds_[0]);
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
