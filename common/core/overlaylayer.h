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

#include <hwclayer.h>
#include <memory>

#include "overlaybuffer.h"

namespace hwcomposer {

struct HwcLayer;
class OverlayBuffer;
class ResourceManager;

struct OverlayLayer {
  enum LayerComposition {
    kGpu = 1 << 0,      // Needs GPU Composition.
    kDisplay = 1 << 1,  // Display Can scanout the layer directly.
    kAll = kGpu | kDisplay
  };

  OverlayLayer() = default;
  void SetAcquireFence(int32_t acquire_fence);

  int32_t GetAcquireFence() const;

  int32_t ReleaseAcquireFence() const;

  // Initialize OverlayLayer from layer.
  void InitializeFromHwcLayer(HwcLayer* layer, ResourceManager* buffer_manager,
                              OverlayLayer* previous_layer, uint32_t z_order,
                              uint32_t layer_index, uint32_t max_height,
                              uint32_t rotation, bool handle_constraints,
                              FrameBufferManager* frame_buffer_manager);

  void InitializeFromScaledHwcLayer(
      HwcLayer* layer, ResourceManager* buffer_manager,
      OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
      const HwcRect<int>& display_frame, uint32_t max_height, uint32_t rotation,
      bool handle_constraints, FrameBufferManager* frame_buffer_manager);
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

  // This represents the transform to
  // be applied to this layer without taking
  // into account any Display transform i.e.
  // GetPlaneTransform()
  uint32_t GetTransform() const {
    return transform_;
  }

  // This represents any transform applied
  // to this layer(i.e. GetTransform()) + overall
  // rotation applied to the display on which this
  // layer is being shown.
  uint32_t GetPlaneTransform() const {
    return plane_transform_;
  }

  // Applies transform to this layer before scanout.
  void SetTransform(uint32_t transform);

  OverlayBuffer* GetBuffer() const;

  void SetBuffer(HWCNativeHandle handle, int32_t acquire_fence,
                 ResourceManager* buffer_manager, bool register_buffer,
                 FrameBufferManager* frame_buffer_manager);

  std::shared_ptr<OverlayBuffer>& GetSharedBuffer() const;

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

  HwcRect<int>& GetSurfaceDamage() {
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

  // Returns true if content of the layer has
  // changed.
  bool HasLayerContentChanged() const {
    return state_ & kLayerContentChanged;
  }

  // Returns true if this layer is visible.
  bool IsVisible() const {
    return !(state_ & kInvisible);
  }

  // Value is the actual composition (i.e. GPU/Display)
  // being used for this layer ir-respective of the
  // actual supported composition.
  void SetLayerComposition(OverlayLayer::LayerComposition value) {
    actual_composition_ = value;
  }

  // value should indicate if the layer can be scanned out
  // by display directly or needs to go through gpu
  // composition pass or can handle both.
  void SupportedDisplayComposition(OverlayLayer::LayerComposition value) {
    supported_composition_ = value;
  }

  bool CanScanOut() const {
    return supported_composition_ & kDisplay;
  }

  bool IsCursorLayer() const {
    return type_ == kLayerCursor;
  }

  bool IsVideoLayer() const {
    return type_ == kLayerVideo;
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

  // Returns true if source rect has changed
  // from previous frame.
  bool HasSourceRectChanged() const {
    return state_ & kSourceRectChanged;
  }

  // Returns true if this layer attributes
  // have changed compared to last frame
  // and needs to be re-tested to ensure
  // we are able to show the layer on screen
  // correctly.
  bool NeedsRevalidation() const {
    return state_ & kNeedsReValidation;
  }

  bool NeedsPartialClear() const {
    return state_ & kForcePartialClear;
  }

  void CloneLayer(const OverlayLayer* layer, const HwcRect<int>& display_frame,
                  ResourceManager* resource_manager, uint32_t z_order,
                  FrameBufferManager* frame_buffer_manager);

  void Dump();

 private:
  enum LayerState {
    kLayerContentChanged = 1 << 0,
    kDimensionsChanged = 1 << 1,
    kInvisible = 1 << 2,
    kSourceRectChanged = 1 << 3,
    kNeedsReValidation = 1 << 4,
    kForcePartialClear = 1 << 5
  };

  struct ImportedBuffer {
   public:
    ImportedBuffer(std::shared_ptr<OverlayBuffer>& buffer,
                   int32_t acquire_fence);
    ~ImportedBuffer();

    std::shared_ptr<OverlayBuffer> buffer_;
    int32_t acquire_fence_ = -1;
  };

  // Validates current state with previous frame state of
  // layer at same z order.
  void ValidatePreviousFrameState(OverlayLayer* rhs, HwcLayer* layer);

  // Check if we want to use a separate overlay for this
  // layer.
  void ValidateForOverlayUsage();

  void ValidateTransform(uint32_t transform, uint32_t display_transform);

  void InitializeState(HwcLayer* layer, ResourceManager* buffer_manager,
                       OverlayLayer* previous_layer, uint32_t z_order,
                       uint32_t layer_index, uint32_t max_height,
                       uint32_t rotation, bool handle_constraints,
                       FrameBufferManager* frame_buffer_manager);

  uint32_t transform_ = 0;
  uint32_t plane_transform_ = 0;
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
  uint32_t state_ = kLayerContentChanged | kDimensionsChanged;
  std::unique_ptr<ImportedBuffer> imported_buffer_;
  LayerComposition supported_composition_ = kAll;
  LayerComposition actual_composition_ = kAll;
  HWCLayerType type_ = kLayerNormal;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_OVERLAYLAYER_H_
