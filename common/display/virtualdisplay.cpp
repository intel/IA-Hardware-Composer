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

#include <vector>

#include "hwctrace.h"
#include "overlaylayer.h"

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
}

void VirtualDisplay::InitVirtualDisplay(uint32_t width, uint32_t height) {
  compositor_.Init();
  width_ = width;
  height_ = height;
}

bool VirtualDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 1;
  return true;
}

bool VirtualDisplay::Present(std::vector<HwcLayer *> &source_layers,
                             int32_t *retire_fence) {
  CTRACE();
  std::vector<OverlayLayer> layers;
  std::vector<OverlayBuffer> buffers;
  std::vector<HwcRect<int>> layers_rects;
  std::vector<size_t> index;
  int ret = 0;
  size_t size = source_layers.size();
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer *layer = source_layers.at(layer_index);
    layers.emplace_back();
    OverlayLayer &overlay_layer = layers.back();
    overlay_layer.SetNativeHandle(layer->GetNativeHandle());
    overlay_layer.SetTransform(layer->GetTransform());
    overlay_layer.SetAlpha(layer->GetAlpha());
    overlay_layer.SetBlending(layer->GetBlending());
    overlay_layer.SetSourceCrop(layer->GetSourceCrop());
    overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    overlay_layer.SetIndex(layer_index);
    overlay_layer.SetAcquireFence(layer->acquire_fence.Release());
    layers_rects.emplace_back(layer->GetDisplayFrame());
    index.emplace_back(layer_index);
    buffers.emplace_back();
    OverlayBuffer *buffer = new OverlayBuffer();
    buffer->InitializeFromNativeHandle(layer->GetNativeHandle(),
                                       buffer_handler_);
    overlay_layer.SetBuffer(buffer);
  }

  if (!compositor_.BeginFrame()) {
    ETRACE("Failed to initialize compositor.");
    return false;
  }

  // Prepare for final composition.
  if (!compositor_.DrawOffscreen(layers, layers_rects, index, buffer_handler_,
                                 width_, height_, output_handle_,
                                 retire_fence)) {
    ETRACE("Failed to prepare for the frame composition ret=%d", ret);
    return false;
  }

  return true;
}

void VirtualDisplay::SetOutputBuffer(HWCNativeHandle buffer,
                                     int32_t acquire_fence) {
  output_handle_ = buffer;
  acquire_fence_ = acquire_fence;
}

}  // namespace hwcomposer
