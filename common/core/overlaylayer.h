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

#ifndef COMMON_CORE_OVERLAYLAYER_H_
#define COMMON_CORE_OVERLAYLAYER_H_

#include <hwcdefs.h>
#include <platformdefines.h>

#include <memory>
#include <hwclayer.h>

#include "overlaybuffer.h"

namespace hwcomposer {

struct HwcLayer;
class OverlayBuffer;
class NativeBufferHandler;

struct OverlayLayer {
  OverlayLayer() = default;
  void SetAcquireFence(int32_t acquire_fence);

  int32_t GetAcquireFence() const;

  int32_t ReleaseAcquireFence() const;

  // Initialize OverlayLayer from layer.
  void InitializeFromHwcLayer(HwcLayer* layer,
                              NativeBufferHandler* buffer_handler,
                              OverlayLayer* previous_layer, uint32_t z_order,
                              uint32_t layer_index, uint32_t max_width,
                              uint32_t max_height, bool handle_constraints);

#ifdef ENABLE_IMPLICIT_CLONE_MODE
  void InitializeFromScaledHwcLayer(HwcLayer* layer,
                                    NativeBufferHandler* buffer_handler,
                                    OverlayLayer* previous_layer,
                                    uint32_t z_order, uint32_t layer_index,
                                    const HwcRect<int>& display_frame,
                                    uint32_t max_width, uint32_t max_height,
                                    bool handle_constraints);
#endif
  // Get z order of this layer.
  uint32_t GetZorder() const {
    return z_order_;
  }

  // Index of hwclayer which this layer
  // represents.
  uint32_t GetLayerIndex() const {
    return layer_index_;
  }

  uint8_t GetAlpha() const {
    return alpha_;
  }

  void SetBlending(HWCBlending blending);

  HWCBlending GetBlending() const {
    return blending_;
  }

  uint32_t GetTransform() const {
    return transform_;
  }

  OverlayBuffer* GetBuffer() const;

  void SetBuffer(NativeBufferHandler* buffer_handler, HWCNativeHandle handle,
                 int32_t acquire_fence);
  void ResetBuffer();

  void SetSourceCrop(const HwcRect<float>& source_crop);
  const HwcRect<float>& GetSourceCrop() const {
    return source_crop_;
  }

  void SetDisplayFrame(const HwcRect<int>& display_frame);
  const HwcRect<int>& GetDisplayFrame() const {
    return display_frame_;
  }

  const HwcRect<int>& GetSurfaceDamage() const {
    return surface_damage_;
  }

  uint32_t GetSourceCropWidth() const {
    return source_crop_width_;
  }

  uint32_t GetSourceCropHeight() const {
    return source_crop_height_;
  }

  uint32_t GetDisplayFrameWidth() const {
    return display_frame_width_;
  }

  uint32_t GetDisplayFrameHeight() const {
    return display_frame_height_;
  }

  // Returns true if any other attribute of layer
  // other than psotion has changed from previous
  // frame.
  bool HasLayerAttributesChanged() const {
    return state_ & kLayerAttributesChanged;
  }

  // Returns true if content of the layer has
  // changed.
  bool HasLayerContentChanged() const {
    return state_ & kLayerContentChanged;
  }

  void GPURendered() {
    gpu_rendered_ = true;
  }

  bool IsCursorLayer() const {
    return type_ == kLayerCursor;
  }

  bool IsVideoLayer() const {
    return type_ == kLayerVideo;
  }

  bool IsGpuRendered() const {
    return gpu_rendered_;
  }

  // Returns true if we should prefer
  // a separate plane for this layer
  // when validating layers in
  // DisplayPlaneManager.
  bool PreferSeparatePlane() const {
    // We set this to true only in case
    // of Media buffer. If this changes
    // in future, use appropriate checks.
    return type_ == kLayerVideo;
  }

  bool HasDimensionsChanged() const {
    return state_ & kDimensionsChanged;
  }

  /**
   * API for querying if Layer source position has
   * changed from last Present call to NativeDisplay.
   */
  bool NeedsToClearSurface() const {
    return state_ & kClearSurface;
  }

  void Dump();

 private:
  enum LayerState {
    kLayerAttributesChanged = 1 << 0,
    kLayerContentChanged = 1 << 1,
    kDimensionsChanged = 1 << 2,
    kClearSurface = 1 << 3
  };

  struct ImportedBuffer {
   public:
    ImportedBuffer(OverlayBuffer* buffer, int32_t acquire_fence);
    ~ImportedBuffer();

    std::unique_ptr<OverlayBuffer> buffer_;
    int32_t acquire_fence_ = -1;
  };

  // Validates current state with previous frame state of
  // layer at same z order.
  void ValidatePreviousFrameState(OverlayLayer* rhs, HwcLayer* layer);

  // Check if we want to use a separate overlay for this
  // layer.
  void ValidateForOverlayUsage(int32_t max_width, int32_t max_height,
                               bool handle_constraints);

  OverlayBuffer* ReleaseBuffer();

  void InitializeState(HwcLayer* layer, NativeBufferHandler* buffer_handler,
                       OverlayLayer* previous_layer, uint32_t z_order,
                       uint32_t layer_index, uint32_t max_width,
                       uint32_t max_height, bool handle_constraints);

  uint32_t transform_ = 0;
  uint32_t z_order_ = 0;
  uint32_t layer_index_ = 0;
  uint32_t source_crop_width_ = 0;
  uint32_t source_crop_height_ = 0;
  uint32_t display_frame_width_ = 0;
  uint32_t display_frame_height_ = 0;
  uint8_t alpha_ = 0xff;
  HwcRect<float> source_crop_;
  HwcRect<int> display_frame_;
  HwcRect<int> surface_damage_;
  HWCBlending blending_ = HWCBlending::kBlendingNone;
  uint32_t state_ =
      kLayerAttributesChanged | kLayerContentChanged | kDimensionsChanged;
  std::unique_ptr<ImportedBuffer> imported_buffer_;
  bool gpu_rendered_ = false;
  HWCLayerType type_ = kLayerNormal;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_OVERLAYLAYER_H_
