/*
 * Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "linux_frontend.h"
#include <commondrmutils.h>
#include <hwcrect.h>

namespace hwcomposer {

class IAHWCVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  IAHWCVsyncCallback(iahwc_callback_data_t data, iahwc_function_ptr_t hook)
      : data_(data), hook_(hook) {
  }

  void Callback(uint32_t display, int64_t timestamp) {
    if (hook_ != NULL) {
      auto hook = reinterpret_cast<IAHWC_PFN_VSYNC>(hook_);
      hook(data_, display, timestamp);
    }
  }

 private:
  iahwc_callback_data_t data_;
  iahwc_function_ptr_t hook_;
};

IAHWC::IAHWC() {
  getFunctionPtr = HookGetFunctionPtr;
  close = HookClose;
}

int32_t IAHWC::Init() {
  if (!device_.Initialize()) {
    fprintf(stderr, "Unable to initialize GPU DEVICE");
    return IAHWC_ERROR_NO_RESOURCES;
  }

  std::vector<hwcomposer::NativeDisplay*> displays = device_.GetAllDisplays();

  for (hwcomposer::NativeDisplay* display : displays) {
    displays_.emplace_back(new IAHWCDisplay());
    IAHWCDisplay* iahwc_display = displays_.back();
    iahwc_display->Init(display);
  }

  return IAHWC_ERROR_NONE;
}

int IAHWC::HookOpen(const iahwc_module_t* module, iahwc_device_t** device) {
  IAHWC* iahwc = new IAHWC();
  iahwc->Init();
  *device = iahwc;

  return IAHWC_ERROR_NONE;
}

iahwc_function_ptr_t IAHWC::HookGetFunctionPtr(iahwc_device_t* /* device */,
                                               int func_descriptor) {
  switch (func_descriptor) {
    case IAHWC_FUNC_GET_NUM_DISPLAYS:
      return ToHook<IAHWC_PFN_GET_NUM_DISPLAYS>(
          DeviceHook<int32_t, decltype(&IAHWC::GetNumDisplays),
                     &IAHWC::GetNumDisplays, int*>);
    case IAHWC_FUNC_REGISTER_CALLBACK:
      return ToHook<IAHWC_PFN_REGISTER_CALLBACK>(
          DeviceHook<int32_t, decltype(&IAHWC::RegisterCallback),
                     &IAHWC::RegisterCallback, int, uint32_t,
                     iahwc_callback_data_t, iahwc_function_ptr_t>);
    case IAHWC_FUNC_DISPLAY_GET_INFO:
      return ToHook<IAHWC_PFN_DISPLAY_GET_INFO>(
          DisplayHook<decltype(&IAHWCDisplay::GetDisplayInfo),
                      &IAHWCDisplay::GetDisplayInfo, uint32_t, int, int32_t*>);
    case IAHWC_FUNC_DISPLAY_GET_NAME:
      return ToHook<IAHWC_PFN_DISPLAY_GET_NAME>(
          DisplayHook<decltype(&IAHWCDisplay::GetDisplayName),
                      &IAHWCDisplay::GetDisplayName, uint32_t*, char*>);
    case IAHWC_FUNC_DISPLAY_GET_CONFIGS:
      return ToHook<IAHWC_PFN_DISPLAY_GET_CONFIGS>(
          DisplayHook<decltype(&IAHWCDisplay::GetDisplayConfigs),
                      &IAHWCDisplay::GetDisplayConfigs, uint32_t*, uint32_t*>);
    case IAHWC_FUNC_DISPLAY_SET_GAMMA:
      return ToHook<IAHWC_PFN_DISPLAY_SET_GAMMA>(
          DisplayHook<decltype(&IAHWCDisplay::SetDisplayGamma),
                      &IAHWCDisplay::SetDisplayGamma, float, float, float>);
    case IAHWC_FUNC_DISPLAY_SET_CONFIG:
      return ToHook<IAHWC_PFN_DISPLAY_SET_CONFIG>(
          DisplayHook<decltype(&IAHWCDisplay::SetDisplayConfig),
                      &IAHWCDisplay::SetDisplayConfig, uint32_t>);
    case IAHWC_FUNC_DISPLAY_GET_CONFIG:
      return ToHook<IAHWC_PFN_DISPLAY_GET_CONFIG>(
          DisplayHook<decltype(&IAHWCDisplay::GetDisplayConfig),
                      &IAHWCDisplay::GetDisplayConfig, uint32_t*>);
    case IAHWC_FUNC_DISPLAY_CLEAR_ALL_LAYERS:
      return ToHook<IAHWC_PFN_DISPLAY_CLEAR_ALL_LAYERS>(
          DisplayHook<decltype(&IAHWCDisplay::ClearAllLayers),
                      &IAHWCDisplay::ClearAllLayers>);
    case IAHWC_FUNC_PRESENT_DISPLAY:
      return ToHook<IAHWC_PFN_PRESENT_DISPLAY>(
          DisplayHook<decltype(&IAHWCDisplay::PresentDisplay),
                      &IAHWCDisplay::PresentDisplay, int32_t*>);
    case IAHWC_FUNC_CREATE_LAYER:
      return ToHook<IAHWC_PFN_CREATE_LAYER>(
          DisplayHook<decltype(&IAHWCDisplay::CreateLayer),
                      &IAHWCDisplay::CreateLayer, uint32_t*>);
    case IAHWC_FUNC_DESTROY_LAYER:
      return ToHook<IAHWC_PFN_DESTROY_LAYER>(
          DisplayHook<decltype(&IAHWCDisplay::DestroyLayer),
                      &IAHWCDisplay::DestroyLayer, uint32_t>);
    case IAHWC_FUNC_LAYER_SET_BO:
      return ToHook<IAHWC_PFN_LAYER_SET_BO>(
          LayerHook<decltype(&IAHWCLayer::SetBo), &IAHWCLayer::SetBo, gbm_bo*>);
    case IAHWC_FUNC_LAYER_SET_RAW_PIXEL_DATA:
      return ToHook<IAHWC_PFN_LAYER_SET_RAW_PIXEL_DATA>(
          LayerHook<decltype(&IAHWCLayer::SetRawPixelData),
                    &IAHWCLayer::SetRawPixelData, iahwc_raw_pixel_data>);
    case IAHWC_FUNC_LAYER_SET_ACQUIRE_FENCE:
      return ToHook<IAHWC_PFN_LAYER_SET_ACQUIRE_FENCE>(
          LayerHook<decltype(&IAHWCLayer::SetAcquireFence),
                    &IAHWCLayer::SetAcquireFence, int32_t>);
    case IAHWC_FUNC_LAYER_SET_USAGE:
      return ToHook<IAHWC_PFN_LAYER_SET_USAGE>(
          LayerHook<decltype(&IAHWCLayer::SetLayerUsage),
                    &IAHWCLayer::SetLayerUsage, int32_t>);
    case IAHWC_FUNC_LAYER_SET_TRANSFORM:
      return ToHook<IAHWC_PFN_LAYER_SET_TRANSFORM>(
          LayerHook<decltype(&IAHWCLayer::SetLayerTransform),
                    &IAHWCLayer::SetLayerTransform, int32_t>);
    case IAHWC_FUNC_LAYER_SET_SOURCE_CROP:
      return ToHook<IAHWC_PFN_LAYER_SET_SOURCE_CROP>(
          LayerHook<decltype(&IAHWCLayer::SetLayerSourceCrop),
                    &IAHWCLayer::SetLayerSourceCrop, iahwc_rect_t>);
    case IAHWC_FUNC_LAYER_SET_DISPLAY_FRAME:
      return ToHook<IAHWC_PFN_LAYER_SET_DISPLAY_FRAME>(
          LayerHook<decltype(&IAHWCLayer::SetLayerDisplayFrame),
                    &IAHWCLayer::SetLayerDisplayFrame, iahwc_rect_t>);
    case IAHWC_FUNC_LAYER_SET_SURFACE_DAMAGE:
      return ToHook<IAHWC_PFN_LAYER_SET_SURFACE_DAMAGE>(
          LayerHook<decltype(&IAHWCLayer::SetLayerSurfaceDamage),
                    &IAHWCLayer::SetLayerSurfaceDamage, iahwc_region_t>);
    case IAHWC_FUNC_INVALID:
    default:
      return NULL;
  }
}

int IAHWC::HookClose(iahwc_device_t* dev) {
  delete dev;
  return 0;
}

// private function implementations

int IAHWC::GetNumDisplays(int* num_displays) {
  *num_displays = 0;
  for (IAHWCDisplay* display : displays_) {
    if (display->IsConnected())
      *num_displays += 1;
  }

  return IAHWC_ERROR_NONE;
}

int IAHWC::RegisterCallback(int32_t description, uint32_t display_id,
                            iahwc_callback_data_t data,
                            iahwc_function_ptr_t hook) {
  switch (description) {
    case IAHWC_CALLBACK_VSYNC: {
      if (display_id >= displays_.size())
        return IAHWC_ERROR_BAD_DISPLAY;
      IAHWCDisplay* display = displays_.at(display_id);
      return display->RegisterVsyncCallback(data, hook);
    }
    default:
      return IAHWC_ERROR_BAD_PARAMETER;
  }
}

IAHWC::IAHWCDisplay::IAHWCDisplay() : native_display_(NULL) {
}

int IAHWC::IAHWCDisplay::Init(hwcomposer::NativeDisplay* display) {
  native_display_ = display;
  native_display_->InitializeLayerHashGenerator(4);
}

int IAHWC::IAHWCDisplay::GetDisplayInfo(uint32_t config, int attribute,
                                        int32_t* value) {
  hwcomposer::HWCDisplayAttribute attrib =
      static_cast<hwcomposer::HWCDisplayAttribute>(attribute);

  bool ret = native_display_->GetDisplayAttribute(config, attrib, value);

  if (!ret)
    return IAHWC_ERROR_NO_RESOURCES;

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::GetDisplayName(uint32_t* size, char* name) {
  bool ret = native_display_->GetDisplayName(size, name);

  if (!ret)
    return IAHWC_ERROR_NO_RESOURCES;

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::GetDisplayConfigs(uint32_t* num_configs,
                                           uint32_t* configs) {
  bool ret = native_display_->GetDisplayConfigs(num_configs, configs);

  if (!ret)
    return IAHWC_ERROR_NO_RESOURCES;

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::SetDisplayGamma(float r, float b, float g) {
  native_display_->SetGamma(r, g, b);
  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::SetDisplayConfig(uint32_t config) {
  bool ret = native_display_->SetActiveConfig(config);

  if (!ret)
    return IAHWC_ERROR_NO_RESOURCES;

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::GetDisplayConfig(uint32_t* config) {
  bool ret = native_display_->GetActiveConfig(config);

  if (!ret)
    return IAHWC_ERROR_NO_RESOURCES;

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::ClearAllLayers() {
  layers_.clear();
  native_display_->ResetLayerHashGenerator();

  return IAHWC_ERROR_NONE;
}
int IAHWC::IAHWCDisplay::PresentDisplay(int32_t* release_fd) {
  std::vector<hwcomposer::HwcLayer*> layers;
  hwcomposer::HwcLayer* cursor_layer = NULL;

  for (std::pair<const iahwc_layer_t, IAHWCLayer>& l : layers_) {
    IAHWCLayer& temp = l.second;
    if (temp.GetLayer()->IsCursorLayer())
      cursor_layer = temp.GetLayer();
    else
      layers.emplace_back(temp.GetLayer());
  }

  if (cursor_layer)
    layers.emplace_back(cursor_layer);

  native_display_->Present(layers, release_fd);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::CreateLayer(uint32_t* layer_handle) {
  *layer_handle = native_display_->AcquireId();
  layers_.emplace(*layer_handle, IAHWCLayer());

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::DestroyLayer(uint32_t layer_handle) {
  if (layers_.empty())
    return IAHWC_ERROR_NONE;

  if (layers_.erase(layer_handle))
    native_display_->ReleaseId(layer_handle);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::RegisterVsyncCallback(iahwc_callback_data_t data,
                                               iahwc_function_ptr_t hook) {
  auto callback = std::make_shared<IAHWCVsyncCallback>(data, hook);
  native_display_->VSyncControl(true);
  int ret = native_display_->RegisterVsyncCallback(std::move(callback),
                                                   static_cast<int>(0));
  if (ret) {
    return IAHWC_ERROR_BAD_DISPLAY;
  }
  return IAHWC_ERROR_NONE;
}

bool IAHWC::IAHWCDisplay::IsConnected() {
  return native_display_->IsConnected();
}

IAHWC::IAHWCLayer::IAHWCLayer() {
  layer_usage_ = IAHWC_LAYER_USAGE_NORMAL;
  pixel_data_ = NULL;
}

IAHWC::IAHWCLayer::~IAHWCLayer() {
  ::close(hwc_handle_.import_data.fd);
  delete pixel_data_;
}

int IAHWC::IAHWCLayer::SetBo(gbm_bo* bo) {
  int32_t width, height;

  ::close(hwc_handle_.import_data.fd);

  width = gbm_bo_get_width(bo);
  height = gbm_bo_get_height(bo);

  hwc_handle_.import_data.width = width;
  hwc_handle_.import_data.height = height;
  hwc_handle_.import_data.format = gbm_bo_get_format(bo);
#if USE_MINIGBM
  size_t total_planes = gbm_bo_get_num_planes(bo);
  for (size_t i = 0; i < total_planes; i++) {
    hwc_handle_.import_data.fds[i] = gbm_bo_get_plane_fd(bo, i);
    hwc_handle_.import_data.offsets[i] = gbm_bo_get_plane_offset(bo, i);
    hwc_handle_.import_data.strides[i] = gbm_bo_get_plane_stride(bo, i);
  }
  temp->meta_data_.num_planes_ = total_planes;
#else
  hwc_handle_.import_data.fd = gbm_bo_get_fd(bo);
  hwc_handle_.import_data.stride = gbm_bo_get_stride(bo);
  hwc_handle_.meta_data_.num_planes_ =
      drm_bo_get_num_planes(hwc_handle_.import_data.format);
#endif

  hwc_handle_.bo = bo;
  hwc_handle_.hwc_buffer_ = true;
  hwc_handle_.gbm_flags = 0;

  iahwc_layer_.SetNativeHandle(&hwc_handle_);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetRawPixelData(iahwc_raw_pixel_data bo) {
  hwc_handle_.meta_data_.width_ = bo.width;
  hwc_handle_.meta_data_.height_ = bo.height;
  hwc_handle_.meta_data_.pitches_[0] = bo.stride;
  hwc_handle_.meta_data_.format_ = bo.format;
  hwc_handle_.gbm_flags = 0;
  hwc_handle_.is_raw_pixel_ = true;

  pixel_data_ = new char[bo.height * bo.stride];
  memcpy(pixel_data_, bo.buffer, bo.height * bo.stride);
  hwc_handle_.pixel_memory_ = pixel_data_;

  iahwc_layer_.SetNativeHandle(&hwc_handle_);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetAcquireFence(int32_t acquire_fence) {
  iahwc_layer_.SetAcquireFence(acquire_fence);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerUsage(int32_t layer_usage) {
  layer_usage_ = layer_usage;
  if (layer_usage_ == IAHWC_LAYER_USAGE_CURSOR) {
    iahwc_layer_.MarkAsCursorLayer();
  }

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerTransform(int32_t layer_transform) {
  // 270* and 180* cannot be combined with flips. More specifically, they
  // already contain both horizontal and vertical flips, so those fields are
  // redundant in this case. 90* rotation can be combined with either horizontal
  // flip or vertical flip, so treat it differently
  int32_t temp = 0;
  if (layer_transform == IAHWC_TRANSFORM_ROT_270) {
    temp = hwcomposer::HWCTransform::kTransform270;
  } else if (layer_transform == IAHWC_TRANSFORM_ROT_180) {
    temp = hwcomposer::HWCTransform::kTransform180;
  } else {
    if (layer_transform & IAHWC_TRANSFORM_FLIP_H)
      temp |= hwcomposer::HWCTransform::kReflectX;
    if (layer_transform & IAHWC_TRANSFORM_FLIP_V)
      temp |= hwcomposer::HWCTransform::kReflectY;
    if (layer_transform & IAHWC_TRANSFORM_ROT_90)
      temp |= hwcomposer::HWCTransform::kTransform90;
  }
  iahwc_layer_.SetTransform(temp);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerSourceCrop(iahwc_rect_t rect) {
  iahwc_layer_.SetSourceCrop(
      hwcomposer::HwcRect<float>(rect.left, rect.top, rect.right, rect.bottom));

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerDisplayFrame(iahwc_rect_t rect) {
  iahwc_layer_.SetDisplayFrame(
      hwcomposer::HwcRect<float>(rect.left, rect.top, rect.right, rect.bottom),
      0);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerSurfaceDamage(iahwc_region_t region) {
  uint32_t num_rects = region.numRects;
  hwcomposer::HwcRegion hwc_region;

  for (size_t rect = 0; rect < num_rects; ++rect) {
    hwc_region.emplace_back(region.rects[rect].left, region.rects[rect].top,
                            region.rects[rect].right,
                            region.rects[rect].bottom);
  }

  iahwc_layer_.SetSurfaceDamage(hwc_region);

  return IAHWC_ERROR_NONE;
}

hwcomposer::HwcLayer* IAHWC::IAHWCLayer::GetLayer() {
  return &iahwc_layer_;
}

} // namespace hwcomposer

iahwc_module_t IAHWC_MODULE_INFO = {
  .name = "IA Hardware Composer",
  .open = hwcomposer::IAHWC::HookOpen,
};
