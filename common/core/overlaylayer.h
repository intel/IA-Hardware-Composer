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

#include "overlaybuffer.h"

namespace hwcomposer {

struct HwcLayer;
class OverlayBuffer;
class NativeBufferHandler;

struct OverlayLayer {
  void SetAcquireFence(int32_t acquire_fence);

  int32_t GetAcquireFence() const;

  void SetIndex(uint32_t index);

  uint32_t GetIndex() const {
    return index_;
  }

  void SetTransform(int32_t sf_transform);

  uint32_t GetTransform() const {
    return transform_;
  }

  void SetAlpha(uint8_t alpha);

  uint8_t GetAlpha() const {
    return alpha_;
  }

  void SetBlending(HWCBlending blending);

  HWCBlending GetBlending() const {
    return blending_;
  }

  uint32_t GetRotation() const {
    return rotation_;
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

  // Validates current state with previous frame state of
  // layer at same z order.
  void ValidatePreviousFrameState(const OverlayLayer& rhs, HwcLayer* layer);

  // Check if we want to use a separate overlay for this
  // layer.
  void ValidateForOverlayUsage();

  // Returns true if position of layer has
  // changed from previous frame.
  bool HasLayerPositionChanged() const {
    return state_ & kLayerPositionChanged;
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

  void GPURenderedCursor() {
    gpu_rendered_cursor_ = true;
  }

  // Returns true if we should prefer
  // a separate plane for this layer
  // when validating layers in
  // DisplayPlaneManager.
  bool PreferSeparatePlane() const {
    return prefer_separate_plane_;
  }

  void Dump();

 private:
  enum LayerState {
    kLayerAttributesChanged = 1 << 0,
    kLayerPositionChanged = 1 << 1,
    kLayerContentChanged = 1 << 2,
    kLayerAcquireFenceSignalled = 1 << 3
  };

  struct ImportedBuffer {
   public:
    ImportedBuffer(OverlayBuffer* buffer, int32_t acquire_fence);
    ~ImportedBuffer();

    std::unique_ptr<OverlayBuffer> buffer_;
    int32_t acquire_fence_ = -1;
  };

  uint32_t transform_;
  uint32_t rotation_;
  uint32_t index_;
  uint32_t source_crop_width_;
  uint32_t source_crop_height_;
  uint32_t display_frame_width_;
  uint32_t display_frame_height_;
  uint8_t alpha_ = 0xff;
  HwcRect<float> source_crop_;
  HwcRect<int> display_frame_;
  HWCBlending blending_ = HWCBlending::kBlendingNone;
  uint32_t state_ =
      kLayerAttributesChanged | kLayerPositionChanged | kLayerContentChanged;
  std::unique_ptr<ImportedBuffer> imported_buffer_;
  bool gpu_rendered_cursor_ = false;
  bool prefer_separate_plane_ = false;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_OVERLAYLAYER_H_
