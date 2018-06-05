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

#include "nativebufferhandler.h"

#include "pixeluploader.h"

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

class IAPixelUploaderCallback : public hwcomposer::RawPixelUploadCallback {
 public:
  IAPixelUploaderCallback(iahwc_callback_data_t data, iahwc_function_ptr_t hook,
                          uint32_t display_id)
      : data_(data), hook_(hook), display_(display_id) {
  }

  void Callback(bool start_access, void* call_back_data) {
    if (hook_ != NULL) {
      auto hook = reinterpret_cast<IAHWC_PFN_PIXEL_UPLOADER>(hook_);
      hook(data_, display_, start_access ? 1 : 0, call_back_data);
    }
  }

 private:
  iahwc_callback_data_t data_;
  iahwc_function_ptr_t hook_;
  uint32_t display_;
};

class IAHWCHotPlugEventCallback : public hwcomposer::HotPlugCallback {
 public:
  IAHWCHotPlugEventCallback(iahwc_callback_data_t data,
                            iahwc_function_ptr_t hook,
                            IAHWC::IAHWCDisplay* display)
      : data_(data), hook_(hook), display_(display) {
  }

  void Callback(uint32_t display, bool connected) {
    auto hook = reinterpret_cast<IAHWC_PFN_HOTPLUG>(hook_);
    uint32_t status;
    if (connected) {
      status = static_cast<uint32_t>(IAHWC_DISPLAY_STATUS_CONNECTED);
      if (display_)
        display_->RunPixelUploader(true);
    } else {
      status = static_cast<uint32_t>(IAHWC_DISPLAY_STATUS_DISCONNECTED);
      if (display_)
        display_->RunPixelUploader(false);
    }

    if (hook)
      hook(data_, display, status);
  }

 private:
  iahwc_callback_data_t data_;
  iahwc_function_ptr_t hook_;
  IAHWC::IAHWCDisplay* display_;
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

  const std::vector<hwcomposer::NativeDisplay*>& displays =
      device_.GetAllDisplays();

  for (hwcomposer::NativeDisplay* display : displays) {
    displays_.emplace_back(new IAHWCDisplay());
    IAHWCDisplay* iahwc_display = displays_.back();
    iahwc_display->Init(display, device_.GetFD());
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
    case IAHWC_FUNC_DISPLAY_SET_POWER_MODE:
      return ToHook<IAHWC_PFN_DISPLAY_SET_POWER_MODE>(
          DisplayHook<decltype(&IAHWCDisplay::SetPowerMode),
                      &IAHWCDisplay::SetPowerMode, uint32_t>);
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
    case IAHWC_FUNC_DISABLE_OVERLAY_USAGE:
      return ToHook<IAHWC_PFN_DISABLE_OVERLAY_USAGE>(
          DisplayHook<decltype(&IAHWCDisplay::DisableOverlayUsage),
                      &IAHWCDisplay::DisableOverlayUsage>);
    case IAHWC_FUNC_ENABLE_OVERLAY_USAGE:
      return ToHook<IAHWC_PFN_ENABLE_OVERLAY_USAGE>(
          DisplayHook<decltype(&IAHWCDisplay::EnableOverlayUsage),
                      &IAHWCDisplay::EnableOverlayUsage>);
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
    case IAHWC_FUNC_LAYER_SET_PLANE_ALPHA:
      return ToHook<IAHWC_PFN_LAYER_SET_PLANE_ALPHA>(
          LayerHook<decltype(&IAHWCLayer::SetLayerPlaneAlpha),
                    &IAHWCLayer::SetLayerPlaneAlpha, float>);
    case IAHWC_FUNC_LAYER_SET_INDEX:
      return ToHook<IAHWC_PFN_LAYER_SET_INDEX>(
          LayerHook<decltype(&IAHWCLayer::SetLayerIndex),
                    &IAHWCLayer::SetLayerIndex, uint32_t>);
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
    case IAHWC_CALLBACK_PIXEL_UPLOADER: {
      if (display_id >= displays_.size())
        return IAHWC_ERROR_BAD_DISPLAY;

      IAHWCDisplay* display = displays_.at(display_id);
      display->RegisterPixelUploaderCallback(data, hook);
      return IAHWC_ERROR_NONE;
    }
    case IAHWC_CALLBACK_HOTPLUG: {
      if (display_id >= displays_.size())
        return IAHWC_ERROR_BAD_DISPLAY;
      for (auto display : displays_)
        display->RegisterHotPlugCallback(data, hook);
      return IAHWC_ERROR_NONE;
    }

    default:
      return IAHWC_ERROR_BAD_PARAMETER;
  }
}

IAHWC::IAHWCDisplay::IAHWCDisplay() : native_display_(NULL) {
}

IAHWC::IAHWCDisplay::~IAHWCDisplay() {
  delete raw_data_uploader_;
}

int IAHWC::IAHWCDisplay::Init(hwcomposer::NativeDisplay* display,
                              uint32_t gpu_fd) {
  native_display_ = display;
  native_display_->InitializeLayerHashGenerator(4);
  raw_data_uploader_ =
      new PixelUploader(native_display_->GetNativeBufferHandler());
  return 0;
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

int IAHWC::IAHWCDisplay::SetPowerMode(uint32_t power_mode) {
  native_display_->SetPowerMode(power_mode);

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
  /*
   * Here the assumption is that the layer index set by the compositor
   * is numbered from bottom -> top, i.e. the bottom most layer has the
   * index of 0 and increases upwards.
   */
  uint32_t total_layers = layers_.size();
  layers.resize(total_layers);
  total_layers -= 1;

  for (std::pair<const iahwc_layer_t, IAHWCLayer>& l : layers_) {
    IAHWCLayer& temp = l.second;
    uint32_t layer_index = total_layers - temp.GetLayerIndex();
    layers[layer_index] = temp.GetLayer();
  }

  native_display_->Present(layers, release_fd, this);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::DisableOverlayUsage() {
  native_display_->SetExplicitSyncSupport(false);
  return 0;
}

int IAHWC::IAHWCDisplay::EnableOverlayUsage() {
  native_display_->SetExplicitSyncSupport(true);
  return 0;
}

void IAHWC::IAHWCDisplay::Synchronize() {
  raw_data_uploader_->Synchronize();
}

int IAHWC::IAHWCDisplay::RegisterHotPlugCallback(iahwc_callback_data_t data,
                                                 iahwc_function_ptr_t func) {
  auto callback = std::make_shared<IAHWCHotPlugEventCallback>(data, func, this);
  // TODO:XXX send proper handle
  native_display_->RegisterHotPlugCallback(std::move(callback),
                                           static_cast<int>(0));
  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCDisplay::RunPixelUploader(bool enable) {
  if (enable)
    raw_data_uploader_->Initialize();
  else
    raw_data_uploader_->ExitThread();
  return 0;
}

int IAHWC::IAHWCDisplay::CreateLayer(uint32_t* layer_handle) {
  *layer_handle = native_display_->AcquireId();
  layers_.emplace(*layer_handle, IAHWCLayer(raw_data_uploader_));

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

void IAHWC::IAHWCDisplay::RegisterPixelUploaderCallback(
    iahwc_callback_data_t data, iahwc_function_ptr_t hook) {
  auto callback = std::make_shared<IAPixelUploaderCallback>(data, hook, 0);
  raw_data_uploader_->RegisterPixelUploaderCallback(std::move(callback));
}

bool IAHWC::IAHWCDisplay::IsConnected() {
  return native_display_->IsConnected();
}

IAHWC::IAHWCLayer::IAHWCLayer(PixelUploader* uploader)
    : raw_data_uploader_(uploader) {
  layer_usage_ = IAHWC_LAYER_USAGE_NORMAL;
  layer_index_ = 0;
  memset(&hwc_handle_.import_data, 0, sizeof(hwc_handle_.import_data));
  memset(&hwc_handle_.meta_data_, 0, sizeof(hwc_handle_.meta_data_));
  iahwc_layer_.SetBlending(hwcomposer::HWCBlending::kBlendingPremult);
}

IAHWC::IAHWCLayer::~IAHWCLayer() {
  if (pixel_buffer_) {
    const NativeBufferHandler* buffer_handler =
        raw_data_uploader_->GetNativeBufferHandler();
    if (upload_in_progress_) {
      raw_data_uploader_->Synchronize();
    }
    buffer_handler->ReleaseBuffer(pixel_buffer_);
    buffer_handler->DestroyHandle(pixel_buffer_);
    pixel_buffer_ = NULL;
  } else {
    ClosePrimeHandles();
  }
}

int IAHWC::IAHWCLayer::SetBo(gbm_bo* bo) {
  int32_t width, height;

  if (pixel_buffer_) {
    const NativeBufferHandler* buffer_handler =
        raw_data_uploader_->GetNativeBufferHandler();
    if (upload_in_progress_) {
      raw_data_uploader_->Synchronize();
    }
    buffer_handler->ReleaseBuffer(pixel_buffer_);
    buffer_handler->DestroyHandle(pixel_buffer_);
    pixel_buffer_ = NULL;
  } else {
    ClosePrimeHandles();
  }

  width = gbm_bo_get_width(bo);
  height = gbm_bo_get_height(bo);

  hwc_handle_.import_data.fd_data.width = width;
  hwc_handle_.import_data.fd_data.height = height;
  hwc_handle_.import_data.fd_data.format = gbm_bo_get_format(bo);
  hwc_handle_.import_data.fd_data.fd = gbm_bo_get_fd(bo);
  hwc_handle_.import_data.fd_data.stride = gbm_bo_get_stride(bo);
  hwc_handle_.meta_data_.num_planes_ =
      drm_bo_get_num_planes(hwc_handle_.import_data.fd_data.format);

  hwc_handle_.bo = bo;
  hwc_handle_.hwc_buffer_ = true;
  hwc_handle_.gbm_flags = 0;

  iahwc_layer_.SetNativeHandle(&hwc_handle_);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetRawPixelData(iahwc_raw_pixel_data bo) {
  const NativeBufferHandler* buffer_handler =
      raw_data_uploader_->GetNativeBufferHandler();
  ClosePrimeHandles();
  if (pixel_buffer_ &&
      ((orig_height_ != bo.height) || (orig_stride_ != bo.stride))) {
    if (upload_in_progress_) {
      raw_data_uploader_->Synchronize();
    }

    buffer_handler->ReleaseBuffer(pixel_buffer_);
    buffer_handler->DestroyHandle(pixel_buffer_);
    pixel_buffer_ = NULL;
  }

  if (!pixel_buffer_) {
    int layer_type =
        layer_usage_ == IAHWC_LAYER_USAGE_CURSOR ? kLayerCursor : kLayerNormal;
    bool modifier_used = false;
    if (!buffer_handler->CreateBuffer(bo.width, bo.height, bo.format,
                                      &pixel_buffer_, layer_type,
                                      &modifier_used, 0, true)) {
      ETRACE("PixelBuffer: CreateBuffer failed");
      return -1;
    }

    if (!buffer_handler->ImportBuffer(pixel_buffer_)) {
      ETRACE("PixelBuffer: ImportBuffer failed");
      return -1;
    }

    if (pixel_buffer_->meta_data_.prime_fds_[0] <= 0) {
      ETRACE("PixelBuffer: prime_fd_ is invalid.");
      return -1;
    }

    orig_width_ = bo.width;
    orig_height_ = bo.height;
    orig_stride_ = bo.stride;
    iahwc_layer_.SetNativeHandle(pixel_buffer_);
  }

  upload_in_progress_ = true;
  raw_data_uploader_->UpdateLayerPixelData(
      pixel_buffer_, orig_width_, orig_height_, orig_stride_, bo.callback_data,
      (uint8_t*)bo.buffer, this, iahwc_layer_.GetSurfaceDamage());

  return IAHWC_ERROR_NONE;
}

void IAHWC::IAHWCLayer::UploadDone() {
  upload_in_progress_ = false;
}

int IAHWC::IAHWCLayer::SetAcquireFence(int32_t acquire_fence) {
  iahwc_layer_.SetAcquireFence(acquire_fence);

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerUsage(int32_t layer_usage) {
  if (layer_usage_ != layer_usage) {
    layer_usage_ = layer_usage;
    if (layer_usage_ == IAHWC_LAYER_USAGE_CURSOR) {
      iahwc_layer_.MarkAsCursorLayer();
    }

    if (pixel_buffer_) {
      const NativeBufferHandler* buffer_handler =
          raw_data_uploader_->GetNativeBufferHandler();
      buffer_handler->ReleaseBuffer(pixel_buffer_);
      buffer_handler->DestroyHandle(pixel_buffer_);
      pixel_buffer_ = NULL;
    }
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
      0, 0);

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

int IAHWC::IAHWCLayer::SetLayerPlaneAlpha(float alpha) {
  iahwc_layer_.SetAlpha(alpha);
  if (alpha != 1.0) {
    iahwc_layer_.SetBlending(HWCBlending::kBlendingPremult);
  }

  return IAHWC_ERROR_NONE;
}

int IAHWC::IAHWCLayer::SetLayerIndex(uint32_t layer_index) {
  layer_index_ = layer_index;

  return IAHWC_ERROR_NONE;
}

hwcomposer::HwcLayer* IAHWC::IAHWCLayer::GetLayer() {
  return &iahwc_layer_;
}

void IAHWC::IAHWCLayer::ClosePrimeHandles() {
  if (hwc_handle_.import_data.fd_data.fd > 0) {
    ::close(hwc_handle_.import_data.fd_data.fd);
    memset(&hwc_handle_.import_data, 0, sizeof(hwc_handle_.import_data));
    memset(&hwc_handle_.meta_data_, 0, sizeof(hwc_handle_.meta_data_));
  }
}

}  // namespace hwcomposer

iahwc_module_t IAHWC_MODULE_INFO = {
    .name = "IA Hardware Composer", .open = hwcomposer::IAHWC::HookOpen,
};
