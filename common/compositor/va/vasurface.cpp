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

#include "vasurface.h"

#include "hwctrace.h"
#include "overlaybuffer.h"

#ifdef ANDROID
#include <va/va_android.h>
#else
#include <va/va_drm.h>
#endif

#include <va/va_drmcommon.h>

#include <drm_fourcc.h>

#include "vautils.h"

namespace hwcomposer {

VASurface::VASurface(uint32_t width, uint32_t height)
    : NativeSurface(width, height) {
  memset(&output_region_, 0, sizeof(output_region_));
}

VASurface::~VASurface() {
  if (surface_ != VA_INVALID_ID) {
    vaDestroySurfaces(display_, &surface_, 1);
  }
}

bool VASurface::MakeCurrent() {
  return true;
}

bool VASurface::CreateVASurface(void* va_display) {
  const OverlayLayer* layer = GetLayer();
  uint32_t width = layer->GetSourceCropWidth();
  uint32_t height = layer->GetSourceCropHeight();
  if ((surface_ != VA_INVALID_ID) &&
      ((previous_width_ != width) || (previous_height_ != height))) {
    vaDestroySurfaces(display_, &surface_, 1);
    surface_ = VA_INVALID_ID;
  }

  previous_width_ = width;
  previous_height_ = height;

  if (surface_ == VA_INVALID_ID) {
    display_ = va_display;
    VASurfaceAttribExternalBuffers external_out;
    memset(&external_out, 0, sizeof(external_out));
    OverlayBuffer* buffer_out = GetLayer()->GetBuffer();
    unsigned long prime_fd_out = buffer_out->GetPrimeFD();
    int rt_format = DrmFormatToRTFormat(buffer_out->GetFormat());
    external_out.pixel_format = DrmFormatToVAFormat(buffer_out->GetFormat());
    external_out.width = previous_width_;
    external_out.height = previous_height_;
    external_out.num_planes = buffer_out->GetTotalPlanes();
    const uint32_t* pitches = buffer_out->GetPitches();
    const uint32_t* offsets = buffer_out->GetOffsets();
    for (unsigned int i = 0; i < external_out.num_planes; i++) {
      external_out.pitches[i] = pitches[i];
      external_out.offsets[i] = offsets[i];
    }
    external_out.num_buffers = 1;
    external_out.buffers = &prime_fd_out;

    VASurfaceAttrib attribs[2];
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external_out;

    VAStatus ret =
        vaCreateSurfaces(display_, rt_format, external_out.width,
                         external_out.height, &surface_, 1, attribs, 2);

    output_region_.x = 0;
    output_region_.y = 0;
    output_region_.width = previous_width_;
    output_region_.height = previous_height_;

    return ret == VA_STATUS_SUCCESS ? true : false;
  }

  return true;
}

}  // namespace hwcomposer
