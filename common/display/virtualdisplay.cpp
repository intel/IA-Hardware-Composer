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

#include "virtualdisplay.h"

#include <drm_fourcc.h>

#include <hwclayer.h>
#include <nativebufferhandler.h>

#include <sstream>
#include <vector>

#include "hwctrace.h"
#include "overlaylayer.h"

#include "hwcutils.h"

namespace hwcomposer {

VirtualDisplay::VirtualDisplay(uint32_t gpu_fd,
                               NativeBufferHandler *buffer_handler,
                               uint32_t /*pipe_id*/, uint32_t /*crtc_id*/)
    : output_handle_(0), acquire_fence_(-1), width_(0), height_(0) {
  resource_manager_.reset(new ResourceManager(buffer_handler));
  if (!resource_manager_) {
    ETRACE("Failed to construct hwc layer buffer manager");
  }

  compositor_.Init(resource_manager_.get(), gpu_fd, fb_manager_);
}

VirtualDisplay::~VirtualDisplay() {
  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }

  if (handle_) {
    ResourceHandle temp;
    temp.handle_ = handle_;
    resource_manager_->MarkResourceForDeletion(temp, false);
  }

  delete output_handle_;
  std::vector<OverlayLayer>().swap(in_flight_layers_);

  resource_manager_->PurgeBuffer();
  compositor_.Reset();
}

void VirtualDisplay::InitVirtualDisplay(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
}

bool VirtualDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 1;
  return true;
}

bool VirtualDisplay::SetActiveConfig(uint32_t /*config*/) {
  return true;
}

bool VirtualDisplay::Present(std::vector<HwcLayer *> &source_layers,
                             int32_t *retire_fence,
                             PixelUploaderCallback * /*call_back*/,
                             bool handle_constraints) {
  CTRACE();
  std::vector<OverlayLayer> layers;
  std::vector<HwcRect<int>> layers_rects;
  std::vector<size_t> index;
  int ret = 0;
  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  bool frame_changed = (size != previous_size);
  bool layers_changed = frame_changed;
  *retire_fence = -1;
  uint32_t z_order = 0;

  resource_manager_->RefreshBufferCache();
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer *layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;

    layers.emplace_back();
    OverlayLayer &overlay_layer = layers.back();
    OverlayLayer *previous_layer = NULL;
    if (previous_size > z_order) {
      previous_layer = &(in_flight_layers_.at(z_order));
    }

    overlay_layer.InitializeFromHwcLayer(
        layer, resource_manager_.get(), previous_layer, z_order, layer_index,
        height_, kIdentity, handle_constraints, fb_manager_);
    index.emplace_back(z_order);
    layers_rects.emplace_back(layer->GetDisplayFrame());
    z_order++;

    if (frame_changed) {
      layer->Validate();
      continue;
    }

    if (!previous_layer || overlay_layer.HasLayerContentChanged() ||
        overlay_layer.HasDimensionsChanged()) {
      layers_changed = true;
    }

    layer->Validate();
  }

  if (layers_changed) {
    compositor_.BeginFrame(false);

    // Prepare for final composition.
    if (!compositor_.DrawOffscreen(
            layers, layers_rects, index, resource_manager_.get(), width_,
            height_, output_handle_, acquire_fence_, retire_fence)) {
      ETRACE("Failed to prepare for the frame composition ret=%d", ret);
      return false;
    }

    acquire_fence_ = 0;

    in_flight_layers_.swap(layers);
  }

  int32_t fence = *retire_fence;

  if (fence > 0) {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      HwcLayer *layer = source_layers.at(layer_index);
      layer->SetReleaseFence(dup(fence));
    }
  } else {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      const OverlayLayer &overlay_layer =
          in_flight_layers_.at(index.at(layer_index));
      HwcLayer *layer = source_layers.at(overlay_layer.GetLayerIndex());
      layer->SetReleaseFence(overlay_layer.ReleaseAcquireFence());
    }
  }

  compositor_.FreeResources();

  return true;
}

void VirtualDisplay::SetOutputBuffer(HWCNativeHandle buffer,
                                     int32_t acquire_fence) {
  if (!output_handle_ || output_handle_ != buffer) {
    const NativeBufferHandler *handler =
        resource_manager_->GetNativeBufferHandler();

    if (handle_) {
      ResourceHandle temp;
      temp.handle_ = handle_;
      resource_manager_->MarkResourceForDeletion(temp, false);
    }

    delete output_handle_;
    output_handle_ = buffer;
    handle_ = 0;

    if (output_handle_) {
      handler->CopyHandle(output_handle_, &handle_);
    }
  }

  if (acquire_fence_ > 0) {
    close(acquire_fence_);
    acquire_fence_ = -1;
  }

  if (acquire_fence > 0) {
    acquire_fence_ = dup(acquire_fence);
  }
}

bool VirtualDisplay::Initialize(NativeBufferHandler * /*buffer_manager*/,
                                FrameBufferManager *frame_buffer_manager) {
  fb_manager_ = frame_buffer_manager;
  return true;
}

bool VirtualDisplay::GetDisplayAttribute(uint32_t /*config*/,
                                         HWCDisplayAttribute attribute,
                                         int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = width_;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = height_;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *value = 16666666;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = 1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value = 1;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

bool VirtualDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                       uint32_t *configs) {
  *num_configs = 1;
  if (configs)
    configs[0] = 0;

  return true;
}

bool VirtualDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Virtual";
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length - 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

int VirtualDisplay::GetDisplayPipe() {
  return -1;
}

bool VirtualDisplay::SetPowerMode(uint32_t /*power_mode*/) {
  return true;
}

int VirtualDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> /*callback*/, uint32_t /*display_id*/) {
  return 0;
}

void VirtualDisplay::VSyncControl(bool /*enabled*/) {
}

bool VirtualDisplay::CheckPlaneFormat(uint32_t /*format*/) {
  // assuming that virtual display supports the format
  return true;
}

}  // namespace hwcomposer
