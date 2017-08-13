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

/* rotation property bits */
#ifndef DRM_ROTATE_0
#define DRM_ROTATE_0 0
#endif

#ifndef DRM_ROTATE_90
#define DRM_ROTATE_90 1
#endif

#ifndef DRM_ROTATE_180
#define DRM_ROTATE_180 2
#endif

#ifndef DRM_ROTATE_270
#define DRM_ROTATE_270 3
#endif

#ifndef DRM_ROTATE_X
#define DRM_REFLECT_X 4
#endif

#ifndef DRM_ROTATE_Y
#define DRM_REFLECT_Y 5
#endif

struct OverlayLayer {
  OverlayLayer() = default;
  void SetAcquireFence(int32_t acquire_fence);

  int32_t GetAcquireFence() const;

  int32_t ReleaseAcquireFence() const;

  // Initialize OverlayLayer from layer.
  void InitializeFromHwcLayer(HwcLayer* layer,
                              NativeBufferHandler* buffer_handler,
                              uint32_t z_order, uint32_t layer_index,
                              const HwcRect<int>& display_frame);

  // Get z order of this layer.
  uint32_t GetZorder() const {
    return z_order_;
  }

  // Index of hwclayer which this layer
  // represents.
  uint32_t GetLayerIndex() const {
    return layer_index_;
  }

  uint32_t GetTransform() const {
    return transform_;
  }

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

  // Validates current state with previous frame state of
  // layer at same z order.
  void ValidatePreviousFrameState(const OverlayLayer& rhs, HwcLayer* layer);

  // Check if we want to use a separate overlay for this
  // layer.
  void ValidateForOverlayUsage();

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
    return cursor_layer_;
  }

  bool IsGpuRendered() const {
    return gpu_rendered_;
  }

  // Returns true if we should prefer
  // a separate plane for this layer
  // when validating layers in
  // DisplayPlaneManager.
  bool PreferSeparatePlane() const {
    return prefer_separate_plane_;
  }

  bool HasDimensionsChanged() const {
    return state_ & kDimensionsChanged;
  }

  void Dump();

 private:
  enum LayerState {
    kLayerAttributesChanged = 1 << 0,
    kLayerContentChanged = 1 << 1,
    kDimensionsChanged = 1 << 2
  };

  struct ImportedBuffer {
   public:
    ImportedBuffer(OverlayBuffer* buffer, int32_t acquire_fence);
    ~ImportedBuffer();

    std::unique_ptr<OverlayBuffer> buffer_;
    int32_t acquire_fence_ = -1;
  };

  uint32_t transform_ = 0;
  uint32_t rotation_ = 1 << DRM_ROTATE_0;
  uint32_t z_order_;
  uint32_t layer_index_;
  uint32_t source_crop_width_;
  uint32_t source_crop_height_;
  uint32_t display_frame_width_;
  uint32_t display_frame_height_;
  uint8_t alpha_ = 0xff;
  HwcRect<float> source_crop_;
  HwcRect<int> display_frame_;
  HwcRect<int> surface_damage_;
  HWCBlending blending_ = HWCBlending::kBlendingNone;
  uint32_t state_ =
      kLayerAttributesChanged | kLayerContentChanged | kDimensionsChanged;
  std::unique_ptr<ImportedBuffer> imported_buffer_;
  bool gpu_rendered_ = false;
  bool prefer_separate_plane_ = false;
  bool cursor_layer_ = false;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_OVERLAYLAYER_H_
