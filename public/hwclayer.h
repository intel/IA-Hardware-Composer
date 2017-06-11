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
    return surface_damage_changed_;
  }

  /**
   * API for querying if content of layer has changed
   * for last Present call to NativeDisplay.
   */
  bool HasLayerContentChanged() const {
    return layer_content_changed_;
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

 private:
  uint32_t transform_ = 0;
  uint8_t alpha_ = 0xff;
  HwcRect<float> source_crop_;
  HwcRect<int> display_frame_;
  HwcRect<int> surface_damage_;
  HWCBlending blending_ = HWCBlending::kBlendingNone;
  HWCNativeHandle sf_handle_ = 0;
  int32_t release_fd_ = -1;
  int32_t acquire_fence_ = -1;
  bool surface_damage_changed_ = true;
  bool layer_content_changed_ = true;
};

}  // namespace hwcomposer
#endif  // PUBLIC_HWCLAYER_H_
