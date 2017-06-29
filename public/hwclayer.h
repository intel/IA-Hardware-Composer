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

#ifndef PUBLIC_HWCLAYER_H_
#define PUBLIC_HWCLAYER_H_

#include <hwcdefs.h>

#include <platformdefines.h>

namespace hwcomposer {

struct HwcLayer {
  ~HwcLayer();

  void SetNativeHandle(HWCNativeHandle handle);

  HWCNativeHandle GetNativeHandle() const {
    return sf_handle_;
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

  void SetSourceCrop(const HwcRect<float>& source_crop);
  const HwcRect<float>& GetSourceCrop() const {
    return source_crop_;
  }

  void SetDisplayFrame(const HwcRect<int>& display_frame);
  const HwcRect<int>& GetDisplayFrame() const {
    return display_frame_;
  }

  /**
   * API for setting surface damage for this layer.
   * @param surface_damage should contain exactly 1
   *        rect with all zeros if content of the
   *        layer has not changed from last Present call.
   *        If no of rects is zero than assumption is that
   *        the contents of layer has completely changed
   *        from last Present call.
   */
  void SetSurfaceDamage(const HwcRegion& surface_damage);

  /**
   * API for getting surface damage of this layer.
   */
  const HwcRect<int>& GetSurfaceDamage() const {
    return surface_damage_;
  }

  /**
   * API for querying if surface damage region has
   * changed from last Present call to NativeDisplay.
   */
  bool HasSurfaceDamageRegionChanged() const {
    return state_ & kSurfaceDamaged;
  }

  /**
   * API for querying if content of layer has changed
   * for last Present call to NativeDisplay.
   */
  bool HasLayerContentChanged() const {
    return state_ & kLayerContentChanged;
  }

  /**
   * API for setting visible region for this layer. The
   * new visible region will take into effect in next Present
   * call, by default this would be same as default frame.
   * @param visible_region should contain regions of
   *        layer which are visible.
   */
  void SetVisibleRegion(const HwcRegion& visible_region);

  /**
   * API for getting visible rect of this layer.
   */
  const HwcRect<int>& GetVisibleRect() const {
    return visible_rect_;
  }

  /**
   * API for querying if visible region has
   * changed from last Present call to NativeDisplay.
   */
  bool HasVisibleRegionChanged() const {
    return state_ & kVisibleRegionChanged;
  }

  /**
   * API for querying if layer is visible.
   */
  bool IsVisible() const {
    return state_ & kVisible;
  }

  /**
   * API for querying if layer attributes has
   * changed from last Present call to NativeDisplay.
   * This takes into consideration any changes to
   * transform, display frame, source rect and
   * blending.
   */
  bool HasLayerAttributesChanged() const {
    return layer_cache_ & kLayerAttributesChanged;
  }

  /**
   * API for querying if layer position has
   * changed from last Present call to NativeDisplay.
   */
  bool HasLayerPositionChanged() const {
    return layer_cache_ & kLayerPositionChanged;
  }

  /**
   * API for setting release fence for this layer.
   * @param fd will be populated with Native Fence object.
   *        When fd is signalled, any previous frame
   *        composition results can be invalidated.
   */
  void SetReleaseFence(int32_t fd);

  /**
   * API for getting release fence of this layer.
   * @return "-1" if no valid release fence present
   *         for this layer. Ownership of fd is passed
   *         to caller and caller is responsible for
   *	     closing the fd.
   */
  int32_t GetReleaseFence();

  /**
   * API for setting acquire fence for this layer.
   * @param fd will be populated with Native Fence object.
   *        When fd is signalled, the buffer associated
   *        with the layer is ready to be read from.
   */
  void SetAcquireFence(int32_t fd);

  /**
   * API for getting acquire fence of this layer.
   * @return "-1" if no valid acquire fence present
   *         for this layer. Ownership of acquire
   *         fence is passed to caller and caller is
   *	     responsible for closing the fd.
   */
  int32_t GetAcquireFence();

  /**
   * API for querying if this layer has been presented
   * atleast once during Present call to NativeDisplay.
   */
  bool IsValidated() const {
    return state_ & kLayerValidated;
  }

  /**
   * API for setting ZOrder for this layer.
   */
  void SetLayerZOrder(uint32_t z_order);

 private:
  void Validate();
  friend class DisplayQueue;

  enum LayerState {
    kSurfaceDamaged = 1 << 0,
    kLayerContentChanged = 1 << 1,
    kVisibleRegionChanged = 1 << 2,
    kVisible = 1 << 3,
    kLayerValidated = 1 << 4,
    kVisibleRegionSet = 1 << 5
  };

  enum LayerCache {
    kLayerAttributesChanged = 1 << 0,
    kLayerPositionChanged = 1 << 1
  };

  int32_t transform_ = 0;
  uint8_t alpha_ = 0xff;
  HwcRect<float> source_crop_;
  HwcRect<int> display_frame_;
  HwcRect<int> surface_damage_;
  HwcRect<int> visible_rect_;
  HWCBlending blending_ = HWCBlending::kBlendingNone;
  HWCNativeHandle sf_handle_ = 0;
  int32_t release_fd_ = -1;
  int32_t acquire_fence_ = -1;
  uint32_t z_order_ = 0;
  int state_ = kVisible | kSurfaceDamaged | kVisibleRegionChanged;
  int layer_cache_ = kLayerAttributesChanged;
};

}  // namespace hwcomposer
#endif  // PUBLIC_HWCLAYER_H_
