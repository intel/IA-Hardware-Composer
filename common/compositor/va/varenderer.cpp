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

#include "hwctrace.h"
#include "nativesurface.h"
#include "overlaybuffer.h"
#include "renderstate.h"

#include "vasurface.h"

#define ANDROID_DISPLAY_HANDLE 0x18C34078
#define UNUSED(x) (void*)(&x)

namespace hwcomposer {

VARenderer::~VARenderer() {
  DestroyContext();

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

bool VARenderer::QueryVAProcFilterCaps(VAContextID context,
                                       VAProcFilterType type, void* caps,
                                       uint32_t* num) {
  VAStatus ret =
      vaQueryVideoProcFilterCaps(va_display_, context, type, caps, num);
  if (ret != VA_STATUS_SUCCESS)
    ETRACE("Query Filter Caps failed\n");
  return ret == VA_STATUS_SUCCESS ? true : false;
}

bool VARenderer::MapVAProcFilterColorModetoHwc(HWCColorControl& vppmode,
                                               VAProcColorBalanceType vamode) {
  switch (vamode) {
    case VAProcColorBalanceHue:
      vppmode = HWCColorControl::kColorHue;
      break;
    case VAProcColorBalanceSaturation:
      vppmode = HWCColorControl::kColorSaturation;
      break;
    case VAProcColorBalanceBrightness:
      vppmode = HWCColorControl::kColorBrightness;
      break;
    case VAProcColorBalanceContrast:
      vppmode = HWCColorControl::kColorContrast;
      break;
    default:
      return false;
  }
  return true;
}

bool VARenderer::SetVAProcFilterColorDefaultValue(
    VAProcFilterCapColorBalance* caps) {
  HWCColorControl mode;
  for (int i = 0; i < VAProcColorBalanceCount; i++) {
    if (MapVAProcFilterColorModetoHwc(mode, caps[i].type)) {
      caps_[mode].caps = caps[i];
      caps_[mode].value = caps[i].range.default_value;
    }
  }

  update_caps_ = true;
  return true;
}

bool VARenderer::SetVAProcFilterColorValue(HWCColorControl mode, float value) {
  if (value > caps_[mode].caps.range.max_value ||
      value < caps_[mode].caps.range.min_value) {
    ETRACE("VAlue Filter value out of range\n");
    return false;
  }
  caps_[mode].value = value;
  update_caps_ = true;
  return true;
}

bool VARenderer::Draw(const MediaState& state, NativeSurface* surface) {
  CTRACE();
  OverlayBuffer* buffer_out = surface->GetLayer()->GetBuffer();
  int rt_format = DrmFormatToRTFormat(buffer_out->GetFormat());

  if (va_context_ == VA_INVALID_ID || render_target_format_ != rt_format) {
    render_target_format_ = rt_format;
    if (!CreateContext()) {
      ETRACE("Create VA context failed\n");
      return false;
    }
  }

  if (!UpdateCaps()) {
    ETRACE("Failed to update capabailities. \n");
    return false;
  }

  // Get Input Surface.
  OverlayBuffer* buffer_in = state.layer_->GetBuffer();
  const MediaResourceHandle& resource = buffer_in->GetMediaResource(
      va_display_, state.layer_->GetSourceCropWidth(),
      state.layer_->GetSourceCropHeight());
  VASurfaceID surface_in = resource.surface_;
  if (surface_in == VA_INVALID_ID) {
    ETRACE("Failed to create Va Input Surface. \n");
    return false;
  }

  // Get Output Surface.
  OverlayLayer* layer_out = surface->GetLayer();
  const MediaResourceHandle& out_resource =
      layer_out->GetBuffer()->GetMediaResource(
          va_display_, layer_out->GetSourceCropWidth(),
          layer_out->GetSourceCropHeight());
  VASurfaceID surface_out = out_resource.surface_;
  if (surface_out == VA_INVALID_ID) {
    ETRACE("Failed to create Va Output Surface. \n");
    return false;
  }

  VARectangle surface_region;
  OverlayLayer* layer_in = state.layer_;
  const HwcRect<float>& source_crop = layer_in->GetSourceCrop();
  surface_region.x = static_cast<int>(source_crop.left);
  surface_region.y = static_cast<int>(source_crop.top);
  surface_region.width = layer_in->GetSourceCropWidth();
  surface_region.height = layer_in->GetSourceCropHeight();

  VARectangle output_region;
  const HwcRect<float>& source_crop_out = layer_out->GetSourceCrop();
  output_region.x = static_cast<int>(source_crop_out.left);
  output_region.y = static_cast<int>(source_crop_out.top);
  output_region.width = layer_out->GetSourceCropWidth();
  output_region.height = layer_out->GetSourceCropHeight();

  param_.surface = surface_in;
  param_.surface_region = &surface_region;
  param_.output_region = &output_region;

  DUMPTRACE("surface_region: (%d, %d, %d, %d)\n", surface_region.x,
            surface_region.y, surface_region.width, surface_region.height);
  DUMPTRACE("Layer DisplayFrame:(%d,%d,%d,%d)\n", output_region.x,
            output_region.y, output_region.width, output_region.height);

  for (HWCColorMap::const_iterator itr = state.colors_.begin();
       itr != state.colors_.end(); itr++) {
    SetVAProcFilterColorValue(itr->first, itr->second);
  }

  ScopedVABufferID pipeline_buffer(va_display_);
  if (!pipeline_buffer.CreateBuffer(
          va_context_, VAProcPipelineParameterBufferType,
	  sizeof(VAProcPipelineParameterBuffer), 1, &param_)) {
    return false;
  }

  VAStatus ret = VA_STATUS_SUCCESS;
  ret = vaBeginPicture(va_display_, va_context_, surface_out);
  ret |=
      vaRenderPicture(va_display_, va_context_, &pipeline_buffer.buffer(), 1);
  ret |= vaEndPicture(va_display_, va_context_);

  return ret == VA_STATUS_SUCCESS ? true : false;
}

bool VARenderer::DestroyMediaResources(
    std::vector<struct media_import>& resources) {
  size_t purged_size = resources.size();
  for (size_t i = 0; i < purged_size; i++) {
    MediaResourceHandle& handle = resources.at(i);
    if (handle.surface_ != VA_INVALID_ID) {
      vaDestroySurfaces(va_display_, &handle.surface_, 1);
    }
  }

  return true;
}

bool VARenderer::CreateContext() {
  DestroyContext();

  VAConfigAttrib config_attrib;
  config_attrib.type = VAConfigAttribRTFormat;
  config_attrib.value = render_target_format_;
  VAStatus ret =
      vaCreateConfig(va_display_, VAProfileNone, VAEntrypointVideoProc,
                     &config_attrib, 1, &va_config_);
  if (ret != VA_STATUS_SUCCESS) {
    ETRACE("Create VA Config failed\n");
    return false;
  }

  // These parameters are not used in vaCreateContext so just set them to dummy
  // values
  int width = 1;
  int height = 1;
  ret = vaCreateContext(va_display_, va_config_, width, height, 0x00, nullptr,
                        0, &va_context_);

  update_caps_ = true;
  return ret == VA_STATUS_SUCCESS ? true : false;
}

void VARenderer::DestroyContext() {
  if (va_context_ != VA_INVALID_ID) {
    vaDestroyContext(va_display_, va_context_);
    va_context_ = VA_INVALID_ID;
  }
  if (va_config_ != VA_INVALID_ID) {
    vaDestroyConfig(va_display_, va_config_);
    va_config_ = VA_INVALID_ID;
  }

  std::vector<VABufferID>().swap(filters_);
  std::vector<ScopedVABufferID>().swap(cb_elements_);
}

bool VARenderer::UpdateCaps() {
  if (!update_caps_) {
    return true;
  }

  update_caps_ = true;

  VAProcFilterCapColorBalance vacaps[VAProcColorBalanceCount];
  uint32_t vacaps_num = VAProcColorBalanceCount;

  if (caps_.empty()) {
    if (!QueryVAProcFilterCaps(va_context_, VAProcFilterColorBalance, vacaps,
                               &vacaps_num)) {
      return false;
    } else {
      SetVAProcFilterColorDefaultValue(&vacaps[0]);
    }
  } else {
    std::vector<ScopedVABufferID> cb_elements(VAProcColorBalanceCount,
                                              va_display_);
    std::vector<VABufferID>().swap(filters_);
    std::vector<ScopedVABufferID>().swap(cb_elements_);
    VAProcFilterParameterBufferColorBalance cbparam;
    cbparam.type = VAProcFilterColorBalance;
    cbparam.attrib = VAProcColorBalanceNone;

    for (ColorBalanceCapMapItr itr = caps_.begin(); itr != caps_.end(); itr++) {
      if (fabs(itr->second.value - itr->second.caps.range.default_value) >=
          itr->second.caps.range.step) {
        cbparam.value = itr->second.value;
        cbparam.attrib = itr->second.caps.type;
        if (!cb_elements[static_cast<int>(itr->first)].CreateBuffer(
                va_context_, VAProcFilterParameterBufferType,
                sizeof(VAProcFilterParameterBufferColorBalance), 1, &cbparam)) {
          return false;
        }
        filters_.push_back(cb_elements[static_cast<int>(itr->first)].buffer());
      }
    }

    cb_elements_.swap(cb_elements);
  }

  memset(&param_, 0, sizeof(VAProcPipelineParameterBuffer));
  param_.surface_color_standard = VAProcColorStandardBT601;
  param_.output_color_standard = VAProcColorStandardBT601;
  param_.num_filters = 0;
  param_.filters = nullptr;
  param_.filter_flags = VA_FRAME_PICTURE;

  if (filters_.size()) {
    param_.filters = &filters_[0];
    param_.num_filters = static_cast<unsigned int>(filters_.size());
  }

  return true;
}

}  // namespace hwcomposer
