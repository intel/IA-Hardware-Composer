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
#include <math.h>

#include "hwcutils.h"
#include "hwctrace.h"
#include "nativesurface.h"
#include "overlaybuffer.h"
#include "renderstate.h"

namespace hwcomposer {

VARenderer::~VARenderer() {
  DestroyContext();

  if (va_display_) {
    vaTerminate(va_display_);
  }
}

bool VARenderer::Init(int gpu_fd) {
  va_display_ = GetVADisplay(gpu_fd);
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
      colorbalance_caps_[mode].caps_ = caps[i];
      colorbalance_caps_[mode].value_ = caps[i].range.default_value;
    }
  }
  sharp_caps_.value_ = sharp_caps_.caps_.range.default_value;
  update_caps_ = true;
  return true;
}

bool VARenderer::SetVAProcFilterDeinterlaceDefaultMode() {
  if (deinterlace_caps_.mode_ != VAProcDeinterlacingNone) {
    deinterlace_caps_.mode_ = VAProcDeinterlacingNone;
    update_caps_ = true;
  }
  return true;
}

bool VARenderer::SetVAProcFilterColorValue(HWCColorControl mode,
                                           const HWCColorProp& prop) {
  if (mode == HWCColorControl::kColorHue ||
      mode == HWCColorControl::kColorSaturation ||
      mode == HWCColorControl::kColorBrightness ||
      mode == HWCColorControl::kColorContrast) {
    if (prop.use_default_) {
      if (!colorbalance_caps_[mode].use_default_) {
        colorbalance_caps_[mode].use_default_ = true;
        update_caps_ = true;
      }
    } else if (prop.value_ != colorbalance_caps_[mode].value_) {
      if (prop.value_ > colorbalance_caps_[mode].caps_.range.max_value ||
          prop.value_ < colorbalance_caps_[mode].caps_.range.min_value) {
        ETRACE("VA Filter value out of range. Mode %d range shoud be %f~%f\n",
               mode, colorbalance_caps_[mode].caps_.range.min_value,
               colorbalance_caps_[mode].caps_.range.max_value);
        return false;
      }
      colorbalance_caps_[mode].value_ = prop.value_;
      colorbalance_caps_[mode].use_default_ = false;
      update_caps_ = true;
    }
    return true;
  } else if (mode == HWCColorControl::kColorSharpness) {
    if (prop.use_default_) {
      if (!sharp_caps_.use_default_) {
        sharp_caps_.use_default_ = true;
        update_caps_ = true;
      }
    } else if (prop.value_ != sharp_caps_.value_) {
      if (prop.value_ > sharp_caps_.caps_.range.max_value ||
          prop.value_ < sharp_caps_.caps_.range.min_value) {
        ETRACE("VA Filter sharp value out of range. should be %f~%f\n",
               sharp_caps_.caps_.range.min_value,
               sharp_caps_.caps_.range.max_value);
        return false;
      }
      sharp_caps_.value_ = prop.value_;
      sharp_caps_.use_default_ = false;
      update_caps_ = true;
    }
    return true;
  } else {
    ETRACE("VA Filter undefined color mode\n");
    return false;
  }
}

bool VARenderer::GetVAProcDeinterlaceFlagFromVideo(HWCDeinterlaceFlag flag) {
  if (flag != HWCDeinterlaceFlag::kDeinterlaceFlagAuto) {
    return false;
  } else {
    // TODO:Need video buffer meta data to judge if the frame really need
    // Deinterlace.
  }
  return false;
}
bool VARenderer::SetVAProcFilterDeinterlaceMode(
    const HWCDeinterlaceProp& prop) {
  VAProcDeinterlacingType mode;
  if (prop.flag_ == HWCDeinterlaceFlag::kDeinterlaceFlagNone) {
    SetVAProcFilterDeinterlaceDefaultMode();
    return true;
  } else if (prop.flag_ == HWCDeinterlaceFlag::kDeinterlaceFlagForce ||
             GetVAProcDeinterlaceFlagFromVideo(prop.flag_)) {
    switch (prop.mode_) {
      case HWCDeinterlaceControl::kDeinterlaceNone:
        mode = VAProcDeinterlacingNone;
        break;
      case HWCDeinterlaceControl::kDeinterlaceBob:
        mode = VAProcDeinterlacingBob;
        break;
      case HWCDeinterlaceControl::kDeinterlaceWeave:
        mode = VAProcDeinterlacingWeave;
        break;
      case HWCDeinterlaceControl::kDeinterlaceMotionAdaptive:
        mode = VAProcDeinterlacingMotionAdaptive;
        break;
      case HWCDeinterlaceControl::kDeinterlaceMotionCompensated:
        mode = VAProcDeinterlacingMotionCompensated;
        break;
      default:
        ETRACE("Hwc unsupport deinterlace mode\n");
        return false;
    }
    for (int i = 0; i < VAProcDeinterlacingCount; i++) {
      if (deinterlace_caps_.caps_[i].type == mode) {
        if (deinterlace_caps_.mode_ != mode) {
          update_caps_ = true;
        }
        deinterlace_caps_.mode_ = mode;
        return true;
      }
    }
    ETRACE("VA Filter unsupport deinterlace mode\n");
    return false;
  }
  return false;
}

unsigned int VARenderer::GetVAProcFilterScalingMode(uint32_t mode) {
  switch (mode) {
    case 1:
      return VA_FILTER_SCALING_FAST;
    case 2:
      return VA_FILTER_SCALING_HQ;
    default:
      return VA_FILTER_SCALING_HQ;
  }
}

bool VARenderer::Draw(const MediaState& state, NativeSurface* surface) {
  CTRACE();
  // TODO: Clear surface ?
  surface->SetClearSurface(NativeSurface::kNone);
  OverlayBuffer* buffer_out = surface->GetLayer()->GetBuffer();
  int rt_format = DrmFormatToRTFormat(buffer_out->GetFormat());
  if (va_context_ == VA_INVALID_ID || render_target_format_ != rt_format) {
    render_target_format_ = rt_format;
    if (!CreateContext()) {
      ETRACE("Create VA context failed\n");
      return false;
    }
  }

  // Get Output Surface.
  OverlayLayer* layer_out = surface->GetLayer();
  HwcRect<int> layer_out_disp_frame = layer_out->GetDisplayFrame();
  int xtranslation = layer_out_disp_frame.left;
  int ytranslation = layer_out_disp_frame.top;

  const MediaResourceHandle& out_resource =
    layer_out->GetBuffer()->GetMediaResource(
      va_display_, layer_out->GetDisplayFrameWidth(),
      layer_out->GetDisplayFrameWidth());
  VASurfaceID surface_out = out_resource.surface_;
  if (surface_out == VA_INVALID_ID) {
    ETRACE("Failed to create Va Output Surface. \n");
    return false;
  }

  layer_out->SetProtected(false);

  VAStatus ret = VA_STATUS_SUCCESS;
  ret = vaBeginPicture(va_display_, va_context_, surface_out);

  OverlayLayer* layer_in = NULL;
  uint32_t total_layers = state.layers_.size();
  std::vector<ScopedVABufferID> pipeline_buffers(total_layers, va_display_);

  for (uint32_t i = 0; i < total_layers; i++) {
    layer_in = state.layers_.at(i);
    ScopedVABufferID& pipeline_buffer = pipeline_buffers.at(i);
    // Get Input Surface.
    OverlayBuffer* buffer_in = layer_in->GetBuffer();
    const MediaResourceHandle& resource = buffer_in->GetMediaResource(
      va_display_, layer_in->GetSourceCropWidth(),
      layer_in->GetSourceCropHeight());
    VASurfaceID surface_in = resource.surface_;
    if (surface_in == VA_INVALID_ID) {
      ETRACE("Failed to create Va Input Surface. \n");
      return false;
    }

    // Set the protected status to output layer if input layer is protected
    if (layer_in->IsProtected()) {
      layer_out->SetProtected(true);
    }


    VARectangle surface_region;
    const HwcRect<float>& source_crop = layer_in->GetSourceCrop();
    surface_region.x = static_cast<int>(source_crop.left);
    surface_region.y = static_cast<int>(source_crop.top);
    surface_region.width = layer_in->GetSourceCropWidth();
    surface_region.height = layer_in->GetSourceCropHeight();

    VARectangle output_region;
    HwcRect<int> display_frame = layer_in->GetDisplayFrame();
    display_frame = TranslateRect(display_frame, -xtranslation, -ytranslation);
    output_region.x = display_frame.left;
    output_region.y = display_frame.top;
    output_region.width = layer_in->GetDisplayFrameWidth();
    output_region.height = layer_in->GetDisplayFrameHeight();


    VABlendState bs = {};
    bs.flags = VA_BLEND_PREMULTIPLIED_ALPHA;

    VAProcPipelineParameterBuffer pipe_param = {};
    pipe_param.surface = surface_in;
    pipe_param.surface_region = &surface_region;
    pipe_param.surface_color_standard = VAProcColorStandardBT601;
    pipe_param.output_region = &output_region;
    pipe_param.output_color_standard = VAProcColorStandardBT601;
    pipe_param.blend_state = &bs;

    DUMPTRACE("surface_region: (%d, %d, %d, %d)\n", surface_region.x,
              surface_region.y, surface_region.width, surface_region.height);
    DUMPTRACE("Layer DisplayFrame:(%d,%d,%d,%d)\n", output_region.x,
              output_region.y, output_region.width, output_region.height);

#if VA_MAJOR_VERSION >= 1
    // currently rotation is only supported by VA on Android.
    uint32_t rotation = 0, mirror = 0;
    HWCTransformToVA(layer_in->GetTransform(), rotation, mirror);
    pipe_param.rotation_state = rotation;
    pipe_param.mirror_state = mirror;
#endif

    for (auto itr = state.colors_.begin(); itr != state.colors_.end(); itr++) {
      SetVAProcFilterColorValue(itr->first, itr->second);
    }
    SetVAProcFilterDeinterlaceMode(state.deinterlace_);

    if (!UpdateCaps()) {
      ETRACE("Failed to update capabailities. \n");
      return false;
    }

    pipe_param.filter_flags = GetVAProcFilterScalingMode(state.scaling_mode_);
    if (filters_.size()) {
      pipe_param.filters = filters_.data();
    }
    pipe_param.num_filters = static_cast<unsigned int>(filters_.size());

    if (!pipeline_buffer.CreateBuffer(
          va_context_, VAProcPipelineParameterBufferType,
          sizeof(VAProcPipelineParameterBuffer), 1, &pipe_param)) {
      return false;
    }

    ret |=
      vaRenderPicture(va_display_, va_context_, &pipeline_buffer.buffer(), 1);

  }

  ret |= vaEndPicture(va_display_, va_context_);

  // reviewer: How to resolve this?
  // if (surface_region.width == 1920 && surface_region.height == 1080) {
  //   // FIXME: WA for OAM-63127. Not sure why this is needed but seems
  //   // to ensure we have consistent 60 fps.
  //   vaSyncSurface(va_display_, surface_out);
  // }

  surface->ResetDamage();

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

bool VARenderer::LoadCaps() {
  VAProcFilterCapColorBalance colorbalancecaps[VAProcColorBalanceCount];
  uint32_t colorbalance_num = VAProcColorBalanceCount;
  uint32_t sharp_num = 1;
  uint32_t deinterlace_num = VAProcDeinterlacingCount;
  memset(colorbalancecaps, 0,
         sizeof(VAProcFilterCapColorBalance) * VAProcColorBalanceCount);
  if (!QueryVAProcFilterCaps(va_context_, VAProcFilterColorBalance,
                             colorbalancecaps, &colorbalance_num)) {
    return false;
  }
  if (!QueryVAProcFilterCaps(va_context_, VAProcFilterSharpening,
                             &sharp_caps_.caps_, &sharp_num)) {
    return false;
  }
  if (!QueryVAProcFilterCaps(va_context_, VAProcFilterDeinterlacing,
                             &deinterlace_caps_.caps_, &deinterlace_num)) {
    return false;
  }

  SetVAProcFilterColorDefaultValue(&colorbalancecaps[0]);
  SetVAProcFilterDeinterlaceDefaultMode();

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
  if (ret == VA_STATUS_SUCCESS) {
    if (!LoadCaps() || !UpdateCaps())
      return false;
  }
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
  std::vector<ScopedVABufferID>().swap(sharp_);
}

bool VARenderer::UpdateCaps() {
  if (!update_caps_) {
    return true;
  }

  update_caps_ = false;

  std::vector<ScopedVABufferID> cb_elements(1, va_display_);
  std::vector<ScopedVABufferID> sharp(1, va_display_);
  std::vector<ScopedVABufferID> deinterlace(1, va_display_);

  std::vector<VABufferID>().swap(filters_);
  std::vector<ScopedVABufferID>().swap(cb_elements_);
  std::vector<ScopedVABufferID>().swap(sharp_);
  std::vector<ScopedVABufferID>().swap(deinterlace_);

  VAProcFilterParameterBufferColorBalance cbparam[VAProcColorBalanceCount];
  VAProcFilterParameterBuffer sharpparam;
  VAProcFilterParameterBufferDeinterlacing deinterlaceparam;
  memset(cbparam, 0, VAProcColorBalanceCount *
                         sizeof(VAProcFilterParameterBufferColorBalance));
  int index = 0;
  for (auto itr = colorbalance_caps_.begin(); itr != colorbalance_caps_.end();
       itr++) {
    if (itr->second.use_default_) {
      itr->second.value_ = itr->second.caps_.range.default_value;
    }
    if (fabs(itr->second.value_ - itr->second.caps_.range.default_value) >=
        itr->second.caps_.range.step) {
      cbparam[index].type = VAProcFilterColorBalance;
      cbparam[index].value = itr->second.value_;
      cbparam[index].attrib = itr->second.caps_.type;
      index++;
    }
  }

  if (index) {
    if (!cb_elements[0].CreateBuffer(
            va_context_, VAProcFilterParameterBufferType,
            sizeof(VAProcFilterParameterBufferColorBalance), index, cbparam)) {
      ETRACE("Create color fail\n");
      return false;
    }
    filters_.push_back(cb_elements[0].buffer());
  }
  cb_elements_.swap(cb_elements);

  if (sharp_caps_.use_default_) {
    sharp_caps_.value_ = sharp_caps_.caps_.range.default_value;
  }
  if (fabs(sharp_caps_.value_ - sharp_caps_.caps_.range.default_value) >=
      sharp_caps_.caps_.range.step) {
    sharpparam.value = sharp_caps_.value_;
    sharpparam.type = VAProcFilterSharpening;
    if (!sharp[0].CreateBuffer(va_context_, VAProcFilterParameterBufferType,
                               sizeof(VAProcFilterParameterBuffer), 1,
                               &sharpparam)) {
      return false;
    }
    filters_.push_back(sharp[0].buffer());
  }
  sharp_.swap(sharp);

  if (deinterlace_caps_.mode_ != VAProcDeinterlacingNone) {
    deinterlaceparam.algorithm = deinterlace_caps_.mode_;
    deinterlaceparam.type = VAProcFilterDeinterlacing;
    if (!deinterlace[0].CreateBuffer(
            va_context_, VAProcFilterParameterBufferType,
            sizeof(VAProcFilterParameterBufferDeinterlacing), 1,
            &deinterlaceparam)) {
      return false;
    }
    filters_.push_back(deinterlace[0].buffer());
  }
  deinterlace_.swap(deinterlace);

  return true;
}

#if VA_MAJOR_VERSION >= 1
void VARenderer::HWCTransformToVA(uint32_t transform, uint32_t& rotation,
                                  uint32_t& mirror) {
  rotation = VA_ROTATION_NONE;
  mirror = VA_MIRROR_NONE;

  if (transform & kReflectX)
    mirror |= VA_MIRROR_HORIZONTAL;
  if (transform & kReflectY)
    mirror |= VA_MIRROR_VERTICAL;

  if (mirror == VA_MIRROR_NONE ||
      mirror == (VA_MIRROR_HORIZONTAL | VA_MIRROR_VERTICAL)) {
    transform &= ~kReflectX;
    transform &= ~kReflectY;
    switch (transform) {
      case kTransform270:
        rotation = VA_ROTATION_270;
      case kTransform180:
        rotation = VA_ROTATION_180;
      case kTransform90:
        rotation = VA_ROTATION_90;
      default:
        break;
    }
  } else {
    // Fixme? WA added. VA is using rotation then mirror order
    // CTS Cameration orientation is expecting mirror, then rotation
    // WA added to use inverse rotation to make the same result
    if (transform & kTransform180)
      rotation = VA_ROTATION_180;
    else if (transform & kTransform90)
      rotation = VA_ROTATION_270;
    else if (transform & kTransform270)
      rotation = VA_ROTATION_90;
  }
}
#endif

}  // namespace hwcomposer
