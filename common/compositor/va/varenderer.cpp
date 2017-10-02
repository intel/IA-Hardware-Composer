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

#include "varenderer.h"
#include "platformdefines.h"

#include <xf86drm.h>
#include <drm_fourcc.h>
#include <va/va.h>
#include <va/va_vpp.h>
#include <va/va_drmcommon.h>
#ifdef ANDROID
#include <va/va_android.h>
#else
#include <va/va_drm.h>
#endif

#include "hwctrace.h"
#include "nativesurface.h"
#include "overlaylayer.h"

#define ANDROID_DISPLAY_HANDLE 0x18C34078
#define UNUSED(x) (void*)(&x)

namespace hwcomposer {

class ScopedVASurfaceID {
 public:
  ScopedVASurfaceID(VADisplay display) : display_(display) {
  }

  ~ScopedVASurfaceID() {
    if (surface_ != VA_INVALID_ID) {
      vaDestroySurfaces(display_, &surface_, 1);
    }
  }

  bool CreateSurface(int format, VASurfaceAttribExternalBuffers& external) {
    VASurfaceAttrib attribs[2];
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;

    VAStatus ret = vaCreateSurfaces(display_, format, external.width,
                                    external.height, &surface_, 1, attribs, 2);
    return ret == VA_STATUS_SUCCESS ? true : false;
  }

  operator VASurfaceID() const {
    return surface_;
  }

  VASurfaceID surface() const {
    return surface_;
  }

 private:
  VADisplay display_;
  VASurfaceID surface_ = VA_INVALID_ID;
};

class ScopedVAConfigID {
 public:
  ScopedVAConfigID(VADisplay display) : display_(display) {
  }
  ~ScopedVAConfigID() {
    if (config_ != VA_INVALID_ID) {
      vaDestroyConfig(display_, config_);
    }
  }

  bool CreateConfig(int format) {
    VAConfigAttrib config_attrib;
    config_attrib.type = VAConfigAttribRTFormat;
    config_attrib.value = format;
    VAStatus ret =
        vaCreateConfig(display_, VAProfileNone, VAEntrypointVideoProc,
                       &config_attrib, 1, &config_);
    return ret == VA_STATUS_SUCCESS ? true : false;
  }

  operator VAConfigID() const {
    return config_;
  }

  VAConfigID config() const {
    return config_;
  }

 private:
  VADisplay display_;
  VAConfigID config_ = VA_INVALID_ID;
};

class ScopedVAContextID {
 public:
  ScopedVAContextID(VADisplay display) : display_(display) {
  }
  ~ScopedVAContextID() {
    if (context_ != VA_INVALID_ID) {
      vaDestroyContext(display_, context_);
    }
  }

  bool CreateContext(VAConfigID config, int width, int height,
                     VASurfaceID render_target) {
    VAStatus ret = vaCreateContext(display_, config, width, height, 0x00,
                                   &render_target, 1, &context_);
    return ret == VA_STATUS_SUCCESS ? true : false;
  }

  operator VAContextID() const {
    return context_;
  }

  VAContextID context() const {
    return context_;
  }

 private:
  VADisplay display_;
  VAContextID context_ = VA_INVALID_ID;
};

class ScopedVABufferID {
 public:
  ScopedVABufferID(VADisplay display) : display_(display) {
  }
  ~ScopedVABufferID() {
    if (buffer_ != VA_INVALID_ID)
      vaDestroyBuffer(display_, buffer_);
  }

  bool CreateBuffer(VAContextID context, VABufferType type, uint32_t size,
                    uint32_t num, void* data) {
    VAStatus ret =
        vaCreateBuffer(display_, context, type, size, num, data, &buffer_);
    return ret == VA_STATUS_SUCCESS ? true : false;
  }

  operator VABufferID() const {
    return buffer_;
  }

  VABufferID buffer() const {
    return buffer_;
  }

  VABufferID& buffer() {
    return buffer_;
  }

 private:
  VADisplay display_;
  VABufferID buffer_ = VA_INVALID_ID;
};

VARenderer::~VARenderer() {
  if (va_display_) {
    vaTerminate(va_display_);
  }
}

bool VARenderer::Init(int gpu_fd) {
#ifdef ANDROID
  unsigned int native_display = ANDROID_DISPLAY_HANDLE;
  va_display_ = vaGetDisplay(&native_display);
  UNUSED(gpu_fd);
#else
  va_display_ = vaGetDisplayDRM(gpu_fd);
#endif
  if (!va_display_) {
    ETRACE("vaGetDisplay failed\n");
    return false;
  }
  VAStatus ret = VA_STATUS_SUCCESS;
  int major, minor;
  ret = vaInitialize(va_display_, &major, &minor);
  return ret == VA_STATUS_SUCCESS ? true : false;
}

bool VARenderer::Draw(OverlayLayer* layer, NativeSurface* surface) {
  VASurfaceAttribExternalBuffers external_in;
  memset(&external_in, 0, sizeof(external_in));
  OverlayBuffer* buffer_in = layer->GetBuffer();
  unsigned long prime_fd_in = buffer_in->GetPrimeFD();
  int rt_format = DrmFormatToRTFormat(buffer_in->GetFormat());
  external_in.pixel_format = DrmFormatToVAFormat(buffer_in->GetFormat());
  external_in.width = buffer_in->GetWidth();
  external_in.height = buffer_in->GetHeight();
  external_in.num_planes = buffer_in->GetTotalPlanes();
  const uint32_t* pitches = buffer_in->GetPitches();
  const uint32_t* offsets = buffer_in->GetOffsets();
  for (unsigned int i = 0; i < external_in.num_planes; i++) {
    external_in.pitches[i] = pitches[i];
    external_in.offsets[i] = offsets[i];
  }
  external_in.num_buffers = 1;
  external_in.buffers = &prime_fd_in;

  ScopedVASurfaceID surface_in(va_display_);
  if (!surface_in.CreateSurface(rt_format, external_in)) {
    DTRACE("Create Input surface failed\n");
    return false;
  }

  VASurfaceAttribExternalBuffers external_out;
  memset(&external_out, 0, sizeof(external_out));
  OverlayBuffer* buffer_out = surface->GetLayer()->GetBuffer();
  int dest_width = buffer_out->GetWidth();
  int dest_height = buffer_out->GetHeight();
  unsigned long prime_fd_out = buffer_out->GetPrimeFD();
  rt_format = DrmFormatToRTFormat(buffer_out->GetFormat());
  external_out.pixel_format = DrmFormatToVAFormat(buffer_out->GetFormat());
  external_out.width = dest_width;
  external_out.height = dest_height;
  external_out.num_planes = buffer_out->GetTotalPlanes();
  pitches = buffer_out->GetPitches();
  offsets = buffer_out->GetOffsets();
  for (unsigned int i = 0; i < external_out.num_planes; i++) {
    external_out.pitches[i] = pitches[i];
    external_out.offsets[i] = offsets[i];
  }
  external_out.num_buffers = 1;
  external_out.buffers = &prime_fd_out;

  ScopedVASurfaceID surface_out(va_display_);
  if (!surface_out.CreateSurface(rt_format, external_out)) {
    DTRACE("Create Output surface failed\n");
    return false;
  }

  ScopedVAConfigID va_config(va_display_);
  if (!va_config.CreateConfig(rt_format)) {
    return false;
  }

  ScopedVAContextID va_context(va_display_);
  if (!va_context.CreateContext(va_config, dest_width, dest_height,
                                surface_out)) {
    return false;
  }

  VAProcPipelineParameterBuffer param;
  memset(&param, 0, sizeof(VAProcPipelineParameterBuffer));

  VARectangle surface_region, output_region;
  surface_region.x = 0;
  surface_region.y = 0;
  surface_region.width = layer->GetSourceCropWidth();
  surface_region.height = layer->GetSourceCropHeight();
  param.surface_region = &surface_region;

  output_region.x = 0;
  output_region.y = 0;
  output_region.width = dest_width;
  output_region.height = dest_height;
  param.output_region = &output_region;

  param.surface = surface_in;
  param.surface_color_standard = VAProcColorStandardBT601;
  param.output_color_standard = VAProcColorStandardBT601;
  param.num_filters = 0;
  param.filters = nullptr;
  param.filter_flags = VA_FRAME_PICTURE;

  ScopedVABufferID pipeline_buffer(va_display_);
  if (!pipeline_buffer.CreateBuffer(
          va_context, VAProcPipelineParameterBufferType,
          sizeof(VAProcPipelineParameterBuffer), 1, &param)) {
    return false;
  }

  VAStatus ret = VA_STATUS_SUCCESS;
  ret = vaBeginPicture(va_display_, va_context, surface_out);
  ret |= vaRenderPicture(va_display_, va_context, &pipeline_buffer.buffer(), 1);
  ret |= vaEndPicture(va_display_, va_context);
  ret |= vaSyncSurface(va_display_, surface_out);

  return ret == VA_STATUS_SUCCESS ? true : false;
}

int VARenderer::DrmFormatToVAFormat(int format) {
  switch (format) {
    case DRM_FORMAT_NV12:
      return VA_FOURCC_NV12;
    case DRM_FORMAT_YVU420:
      return VA_FOURCC_YV12;
    case DRM_FORMAT_YUV420:
      return VA_FOURCC('I', '4', '2', '0');
    case DRM_FORMAT_YUV422:
      return VA_FOURCC_YUY2;
    case DRM_FORMAT_UYVY:
      return VA_FOURCC_UYVY;
    case DRM_FORMAT_YUYV:
      return VA_FOURCC_YUY2;
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_AYUV:
    default:
      break;
  }
  return 0;
}

int VARenderer::DrmFormatToRTFormat(int format) {
  switch (format) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
      return VA_RT_FORMAT_YUV420;
    case DRM_FORMAT_YUV422:
      return VA_RT_FORMAT_YUV422;
    case DRM_FORMAT_YUV444:
      return VA_RT_FORMAT_YUV444;
    default:
      break;
  }
  return 0;
}

}  // namespace hwcomposer
