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
#include "va/sysdeps.h"
#include "varenderer.h"

#include <drm_fourcc.h>
#include <math.h>
#include <xf86drm.h>
#include <log/log.h>
#include "va/va_backend.h"
#include "va/va_internal.h"
#include "va/va_fool.h"
#include "va/va_android.h"
#include "va/va_drmcommon.h"
#include "va/drm/va_drm_utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include "cros_gralloc_handle.h"
#include "vautils.h"
#include <pthread.h>
#include "autolock.h"
#include <string.h>

namespace android {
#define ANDROID_DISPLAY_HANDLE 0x18C34078
#define CHECK_SYMBOL(func) { if (!func) printf("func %s not found\n", #func); return VA_STATUS_ERROR_UNKNOWN; }
#define DEVICE_NAME "/dev/dri/renderD128"

VARenderer::~VARenderer() {
  DestroyContext();
  if (va_display_) {
    vaTerminate(va_display_);
  }
  DRMHwcNativeHandle temp_handle;
  if(native_handles.size() == NATIVE_BUFFER_VECTOR_SIZE){
    for( int32_t i=0; i<NATIVE_BUFFER_VECTOR_SIZE; i++ ) {
      temp_handle = native_handles.at(i);
      buffer_handler_->ReleaseBuffer(temp_handle);
      buffer_handler_->DestroyHandle(temp_handle);
    }
    native_handles.clear();
  }
  if(native_rotation_handles.size() == NATIVE_BUFFER_VECTOR_SIZE){
    for( int32_t i=0; i<NATIVE_BUFFER_VECTOR_SIZE; i++ ) {
      temp_handle = native_rotation_handles.at(i);
      buffer_handler_->ReleaseBuffer(temp_handle);
      buffer_handler_->DestroyHandle(temp_handle);
    }
    native_rotation_handles.clear();
  }
  if(native_active_handles.size() == NATIVE_BUFFER_VECTOR_SIZE){
    for( int32_t i=0; i<NATIVE_BUFFER_VECTOR_SIZE; i++ ) {
      temp_handle = native_active_handles.at(i);
      buffer_handler_->ReleaseBuffer(temp_handle);
      buffer_handler_->DestroyHandle(temp_handle);
    }
    native_active_handles.clear();
  }
  if(va_surface_in_ != VA_INVALID_ID)
    vaDestroySurfaces(va_display_, &va_surface_out_, 1);
  if(va_surface_in_ != VA_INVALID_ID)
    vaDestroySurfaces(va_display_, &va_surface_in_, 1);

  std::vector<ScopedVABufferID>::iterator itr = pipeline_buffers.begin();
  while (itr!=pipeline_buffers.end())
  {
    pipeline_buffers.erase(itr);
    itr++;
  }
}

bool VARenderer::Init(uint32_t fd) {	
  unsigned int native_display = ANDROID_DISPLAY_HANDLE;
  buffer_handler_.reset(NativeBufferHandler::CreateInstance(fd));
  VAStatus ret = VA_STATUS_SUCCESS;
  va_display_ = vaGetDisplay(&native_display);
  if (!va_display_) {
    ALOGE("vaGetDisplay failed\n");
    return false;
  }
  ret = pthread_mutex_init(&lock_, NULL);
  if (ret)
    ALOGE("Failed to initialize the mutex lock %d\n", ret);
  // ret = VA_STATUS_SUCCESS;
  int major, minor;
  ret = vaInitialize(va_display_, &major, &minor);

  std::vector<ScopedVABufferID> temp_pipeline_buffers(NATIVE_BUFFER_VECTOR_SIZE, va_display_);
  pipeline_buffers.swap(temp_pipeline_buffers);
  return ret == VA_STATUS_SUCCESS ? true : false;
}

bool VARenderer::QueryVAProcFilterCaps(VAContextID context,
                                       VAProcFilterType type, void* caps,
                                       uint32_t* num) {
  VAStatus ret =
      vaQueryVideoProcFilterCaps(va_display_, context, type, caps, num);
  if (ret != VA_STATUS_SUCCESS)
    ALOGE("Query Filter Caps failed\n");
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
        ALOGE("VA Filter value out of range. Mode %d range shoud be %f~%f\n",
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
        ALOGE("VA Filter sharp value out of range. should be %f~%f\n",
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
    ALOGE("VA Filter undefined color mode\n");
    return false;
  }
}

unsigned int VARenderer::GetVAProcFilterScalingMode(uint32_t mode) {
  if (deinterlace_caps_.mode_ == VAProcDeinterlacingNone) {
    switch (mode) {
      case 1:
        return VA_FILTER_SCALING_FAST;
      case 2:
        return VA_FILTER_SCALING_HQ;
      default:
        return VA_FILTER_SCALING_HQ;
    }
  } else
    return VA_FILTER_SCALING_FAST;
}


//get vasurface by the buffer_hande_t from the layer 
int VARenderer::getSurfaceIn(buffer_handle_t bufferHandle, VADisplay display, VASurfaceID* surface, uint32_t format, uint32_t width, uint32_t height){
  if (NULL == bufferHandle) {
    ALOGE(" in varender.cpp %s,line %d bufferHandle==NULL\n",__FUNCTION__,__LINE__);
    return -1;
  }
  cros_gralloc_handle *gr_handle = (cros_gralloc_handle *)bufferHandle;
  if ((gr_handle->width == 0) || (gr_handle->height == 0)){
    ALOGE(" in varender.cpp %s,line %d bufferHandle is error\n",__FUNCTION__,__LINE__);
    return -1;
  }
  VASurfaceAttribExternalBuffers external;
  memset(&external, 0, sizeof(external));
  int32_t numplanes = gr_handle->base.numFds;
  uint32_t rt_format = DrmFormatToRTFormat(format);
  uint32_t total_planes = numplanes;
  external.pixel_format = DrmFormatToVAFormat(format);
  external.width = width;//gr_handle->width;
  external.height = height;//gr_handle->height;
  external.num_planes = total_planes;
  uintptr_t prime_fds[total_planes];
  for (unsigned int i = 0; i < total_planes; i++) {
    external.pitches[i] = gr_handle->strides[i];
    external.offsets[i] = gr_handle->offsets[i];
    prime_fds[i] = gr_handle->fds[i];
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
  VAStatus ret = vaCreateSurfaces(display, rt_format, external.width, external.height,
				 surface, 1, attribs, 2);
  if (ret != VA_STATUS_SUCCESS)
    ALOGE("AAAfail to create VASurface from drmbuffer with ret %x", ret);
  return ret;
}

bool VARenderer::startRender(DrmHwcLayer* layer,uint32_t format){
  cros_gralloc_handle *gr_handle = (cros_gralloc_handle *)layer->get_usable_handle();
  int64_t modifier = 0;
  uint32_t usage =3;
  VAStatus ret = VA_STATUS_SUCCESS;
  bool modifer_succeeded = false;
  DRMHwcNativeHandle temp_handle = 0;
  uint32_t input_layer_numer = 1;
  std::map<uint32_t, DrmHwcLayer *, std::greater<int>> va_layer_map = layer->getVaLayerMapData();
  input_layer_numer = va_layer_map.size();
  //format = DRM_FORMAT_ABGR8888;
  int rt_format = DrmFormatToRTFormat(format);
  if(render_target_format_ != rt_format)
    render_target_format_ = rt_format;
  if ((layer->transform == kHwcTransform270) || (layer->transform == kHwcTransform90))
    modifier = I915_FORMAT_MOD_Y_TILED;
  else
    modifier = 0;
  //if don't init the context , create the va context and gralloc buffer for the native_handles
  std::vector<DRMHwcNativeHandle> relese_handles;
  AutoLock lock(&lock_, __func__);
  ret = lock.Lock();
  if (va_context_ == VA_INVALID_ID ) {
    if (!CreateContext()) {
      ALOGE("AAAfail Create VA context failed\n");
      return false;
    }
    for( int32_t i=0; i<NATIVE_BUFFER_VECTOR_SIZE; i++ ) {
      buffer_handler_->CreateBuffer(/*gr_handle->width, gr_handle->height,*/1920,1080,format,//2560*1440
                                   &temp_handle,usage,&modifer_succeeded,modifier);
      if (modifier == 0) {
        native_handles.push_back(temp_handle);
        native_active_handles.push_back(temp_handle);
      }else{
        native_rotation_handles.push_back(temp_handle);
        native_active_handles.push_back(temp_handle);
      }
    }
    modifier_bak = modifier;
    current_handle_position = 0;
  }
  if (modifier_bak !=modifier) {
    if (modifier == I915_FORMAT_MOD_Y_TILED) {
      if (native_rotation_handles.size() == 0) {
        for( int32_t i=0; i<NATIVE_BUFFER_VECTOR_SIZE; i++ ) {
          buffer_handler_->CreateBuffer(/*gr_handle->width, gr_handle->height,*/1920,1080,format,
                                       &temp_handle,usage,&modifer_succeeded,modifier);
          native_rotation_handles.push_back(temp_handle);
        }
      }
      native_handles.swap(native_active_handles);
      native_active_handles.swap(native_rotation_handles);
    } else {
      if (native_handles.size() == 0) {
        for( int32_t i=0; i<NATIVE_BUFFER_VECTOR_SIZE; i++ ) {
          buffer_handler_->CreateBuffer(/*gr_handle->width, gr_handle->height,*/1920,1080,format,//2560*1440
                                       &temp_handle,usage,&modifer_succeeded,modifier);
          native_handles.push_back(temp_handle);
        }
      }
      native_rotation_handles.swap(native_active_handles);
      native_active_handles.swap(native_handles);
    }
    modifier_bak = modifier;
    current_handle_position = 0;
  }
  //create va output surface
  VASurfaceID surface_in = VA_INVALID_ID;
  VASurfaceID surface_out = VA_INVALID_ID;
  if (gr_handle->format == DRM_FORMAT_NV12_Y_TILED_INTEL)
    gr_handle->format = DRM_FORMAT_NV12;

  ScopedVABufferID& pipeline_buffer = pipeline_buffers.at(current_handle_position);

  for (int i =0; i<NATIVE_BUFFER_VECTOR_SIZE; i++) {
    ret = getSurfaceIn(native_active_handles.at(current_handle_position)->handle_, va_display_, &surface_out, format, 1920, 1080);//gr_handle->width, gr_handle->height);
    if (VA_STATUS_SUCCESS != ret) {
      ALOGE("in varender.cpp %s,line %d fail to get surface_in\n",__FUNCTION__,__LINE__);
      if (current_handle_position == (NATIVE_BUFFER_VECTOR_SIZE -1 ))
        current_handle_position = 0;
      else
        current_handle_position ++;
    }else
      break;
  }
  if (VA_STATUS_SUCCESS != ret)
    return false;

  va_surface_out_ = surface_out;
  ret = vaBeginPicture(va_display_, va_context_, va_surface_out_);
  for (std::map<uint32_t, DrmHwcLayer *>::reverse_iterator a = va_layer_map.rbegin(); a != va_layer_map.rend(); a++) {
    VAProcPipelineParameterBuffer pipe_param = {};
    cros_gralloc_handle *gr_handle_t = (cros_gralloc_handle *)a->second->get_usable_handle();
    hwc_frect_t source_crop = a->second->source_crop;
    VARectangle surface_region;
    //create va input surface
    surface_region.x = source_crop.left;//gr_handle->left;
    surface_region.y = source_crop.top;//gr_handle->top;
    surface_region.width = source_crop.right - source_crop.left;
    surface_region.height = source_crop.bottom - source_crop.top;
    ALOGE(" in varender.cpp %s,line %d surface_region(x=%d,y=%d,w=%d,h=%d)\n",__FUNCTION__,__LINE__,surface_region.x,surface_region.y,surface_region.width,surface_region.height);
    if ((0 == surface_region.width) || (0 == surface_region.height))
      return false;
    hwc_rect_t display_frame = a->second->display_frame;
    VARectangle output_region;
    output_region.x = display_frame.left;
    output_region.y = display_frame.top;
    output_region.width = display_frame.right - display_frame.left;
    output_region.height = display_frame.bottom - display_frame.top;
    ALOGE(" in varender.cpp %s,line %d output_region(x=%d,y=%d,w=%d,h=%d),zorder=%d\n",__FUNCTION__,__LINE__,output_region.x,output_region.y,output_region.width,output_region.height,a->first);
    ret = getSurfaceIn(a->second->get_usable_handle(), va_display_, &surface_in, gr_handle_t->format, gr_handle_t->width, gr_handle_t->height);
    if (VA_STATUS_SUCCESS != ret) {
      ALOGE("in varender.cpp %s,line %d fail to get surface_in\n",__FUNCTION__,__LINE__);
      return false;
    }
    va_surface_in_ = surface_in;
    pipe_param.surface = va_surface_in_;
    pipe_param.surface_region = &surface_region;
    pipe_param.surface_color_standard = VAProcColorStandardBT601;
    pipe_param.output_region = &output_region;
    pipe_param.output_color_standard = VAProcColorStandardBT601;
    VABlendState bs = {};
    bs.flags = VA_BLEND_PREMULTIPLIED_ALPHA;
    pipe_param.blend_state = &bs;
    pipe_param.filter_flags = GetVAProcFilterScalingMode(1);
    if (filters_.size())
      pipe_param.filters = filters_.data();
    pipe_param.num_filters = static_cast<unsigned int>(filters_.size());
#if VA_MAJOR_VERSION >= 1
    // currently rotation is only supported by VA on Android.
    uint32_t rotation = 0, mirror = 0;
    HWCTransformToVA(layer->transform, rotation, mirror);
    pipe_param.rotation_state = rotation;
    pipe_param.mirror_state = mirror;
#endif
#ifdef VA_SUPPORT_COLOR_RANGE
    uint32_t dataspace = layer->dataspace;
    if ((dataspace & HAL_DATASPACE_RANGE_FULL) != 0) {
      pipe_param.input_color_properties.color_range = VA_SOURCE_RANGE_FULL;
    }
#endif
    if (!pipeline_buffer.CreateBuffer(va_context_, VAProcPipelineParameterBufferType,
                                      sizeof(VAProcPipelineParameterBuffer), 1, &pipe_param)) {
      return false;
    }
    ret |= vaRenderPicture(va_display_, va_context_, &pipeline_buffer.buffer(), 1);
    if (ret != VA_STATUS_SUCCESS) {
      ALOGE(" in varender.cpp %s,line %d, fail to vaRenderPicture ,ret=%d\n",__FUNCTION__,__LINE__,ret);
      return false;
    }
  }
  ret |= vaEndPicture(va_display_, va_context_);
  if (ret != VA_STATUS_SUCCESS)
    ALOGE(" in varender.cpp %s,line %d, fail to vaEndPicture ,ret=%d\n",__FUNCTION__,__LINE__,ret);

  current_handle_position++;
  if (current_handle_position >= NATIVE_BUFFER_VECTOR_SIZE)
    current_handle_position = 0;

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
    ALOGE("Create VA Config failed\n");
    return false;
  }
  // These parameters are not used in vaCreateContext so just set them to dummy
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
      ALOGE("Create color fail\n");
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

  if (transform & kHwcReflectX)
    mirror |= VA_MIRROR_HORIZONTAL;
  if (transform & kHwcReflectY)
    mirror |= VA_MIRROR_VERTICAL;

  if (mirror == VA_MIRROR_NONE ||
      mirror == (VA_MIRROR_HORIZONTAL | VA_MIRROR_VERTICAL)) {
    transform &= ~kHwcReflectX;
    transform &= ~kHwcReflectY;
    switch (transform) {
      case kHwcTransform270:
        rotation = VA_ROTATION_270;
        break;
      case kHwcTransform180:
        rotation = VA_ROTATION_180;
        break;
      case kHwcTransform90:
        rotation = VA_ROTATION_90;
        break;
      default:
        break;
    }
  } else {
    // Fixme? WA added. VA is using rotation then mirror order
    // CTS Cameration orientation is expecting mirror, then rotation
    // WA added to use inverse rotation to make the same result
    if (transform & kHwcTransform180)
      rotation = VA_ROTATION_180;
    else if (transform & kHwcTransform90)
      rotation = VA_ROTATION_270;
    else if (transform & kHwcTransform270)
      rotation = VA_ROTATION_90;
  }
}
#endif

}  // namespace hwcomposer
