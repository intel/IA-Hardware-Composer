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

#include <vector>

#include "hwctrace.h"
#include "overlaylayer.h"

#include "hwcutils.h"

namespace hwcomposer {

VirtualDisplay::VirtualDisplay(uint32_t gpu_fd,
                               NativeBufferHandler *buffer_handler,
                               uint32_t pipe_id, uint32_t crtc_id)
    : Headless(gpu_fd, pipe_id, crtc_id),
      output_handle_(0),
      acquire_fence_(-1),
      buffer_handler_(buffer_handler),
      width_(0),
      height_(0) {
}

VirtualDisplay::~VirtualDisplay() {
  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }

  if (handle_) {
    buffer_handler_->DestroyHandle(handle_);
  }

  delete output_handle_;
}

void VirtualDisplay::InitVirtualDisplay(uint32_t width, uint32_t height) {
  compositor_.Init(nullptr);
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
                             int32_t *retire_fence) {
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

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;

    layers.emplace_back();
    OverlayLayer& overlay_layer = layers.back();
    OverlayLayer* previous_layer = NULL;
    if (previous_size > z_order) {
      previous_layer = &(in_flight_layers_.at(z_order));
    }

    overlay_layer.InitializeFromHwcLayer(layer, buffer_handler_, previous_layer,
                                         z_order, layer_index);
    index.emplace_back(z_order);
    layers_rects.emplace_back(layer->GetDisplayFrame());
    z_order++;

    if (frame_changed) {
      layer->Validate();
      continue;
    }

    if (overlay_layer.HasLayerAttributesChanged() ||
        overlay_layer.HasLayerContentChanged() ||
        overlay_layer.HasDimensionsChanged()) {
      layers_changed = true;
    }

    layer->Validate();
  }

  if (layers_changed) {
    if (!compositor_.BeginFrame(false)) {
      ETRACE("Failed to initialize compositor.");
      return false;
    }

    // Prepare for final composition.
    if (!compositor_.DrawOffscreen(layers, layers_rects, index, buffer_handler_,
                                   width_, height_, output_handle_,
                                   acquire_fence_, retire_fence)) {
      ETRACE("Failed to prepare for the frame composition ret=%d", ret);
      return false;
    }

    acquire_fence_ = 0;

    in_flight_layers_.swap(layers);
  }

  int32_t fence = *retire_fence;

  if (fence > 0) {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      HwcLayer* layer = source_layers.at(layer_index);
      layer->SetReleaseFence(dup(fence));
    }
  } else {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      const OverlayLayer& overlay_layer =
          in_flight_layers_.at(index.at(layer_index));
      HwcLayer* layer = source_layers.at(overlay_layer.GetLayerIndex());
      layer->SetReleaseFence(overlay_layer.ReleaseAcquireFence());
    }
  }

  return true;
}

void VirtualDisplay::SetOutputBuffer(HWCNativeHandle buffer,
                                     int32_t acquire_fence) {
  if (!output_handle_ || output_handle_ != buffer) {
    if (handle_) {
      buffer_handler_->DestroyHandle(handle_);
    }

    delete output_handle_;
    output_handle_ = buffer;
    handle_ = 0;

    if (output_handle_) {
      buffer_handler_->CopyHandle(output_handle_, &handle_);
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

}  // namespace hwcomposer
