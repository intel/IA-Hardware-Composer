/*
// Copyright (c) 2018 Intel Corporation
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

#ifndef __HwcvalContent_h__
#define __HwcvalContent_h__

#include "hardware/hwcomposer2.h"
#include <vector>
#include "HwcvalEnums.h"
#include "HwcTestUtil.h"
#include "public/nativebufferhandler.h"

class DrmShimBuffer;

namespace Hwcval {
class ValRegion {
 private:
  std::vector<hwcomposer::HwcRect<int>> mRects;

 public:
  ValRegion(const hwc_region_t& region);
  ValRegion();

  void GetHwcRects(hwc_region_t& region, hwc_rect_t *& pRect,
                   uint32_t& maxRects) const;
  hwcomposer::HwcRect<int> GetBounds(
      const hwcomposer::HwcRect<int>& displayFrame) const;
  uint32_t NumRects() const;
};

class ValLayer {
 protected:
  //
  // compositionType is used to specify this layer's type and is set by either
  // the hardware composer implementation, or by the caller (see below).
  //
  CompositionType mCompositionType;

  //
  // hints is bit mask set by the HWC implementation during (*prepare)().
  // It is preserved between (*prepare)() calls, unless the
  // HWC_GEOMETRY_CHANGED flag is set, in which case it is reset to 0.
  //
  // see hwc_layer_t::hints
  //
  // Not currently used in validation so not attempting to map to HWC 2 at
  // present.
  //
  uint32_t mHints;

  // see hwc_layer_t::flags
  //
  // Not currently used in validation so not attempting to map to HWC 2 at
  // present.
  //
  uint32_t mFlags;

  // Pointer to internal record of the buffer
  std::shared_ptr<DrmShimBuffer> mBuf;

  // transformation to apply to the buffer during composition
  uint32_t mTransform;

  // blending to apply during composition
  hwcomposer::HWCBlending mBlending;

  /* area of the source to consider, the origin is the top-left corner of
   * the buffer. As of HWC_DEVICE_API_VERSION_1_3, sourceRect uses floats.
   * If the h/w can't support a non-integer source crop rectangle, it should
   * punt to OpenGL ES composition.
   */

  hwcomposer::HwcRect<float> mSourceCropf;

  /* where to composite the sourceCrop onto the display. The sourceCrop
   * is scaled using linear filtering to the displayFrame. The origin is the
   * top-left corner of the screen.
   */
  hwcomposer::HwcRect<int> mDisplayFrame;

  /* visible region in screen space. The origin is the
   * top-left corner of the screen.
   * The visible region INCLUDES areas overlapped by a translucent layer.
   */
  ValRegion mVisibleRegionScreen;

  /* Sync fence object that will be signaled when the buffer's
   * contents are available. May be -1 if the contents are already
   * available. This field is only valid during set(), and should be
   * ignored during prepare(). The set() call must not wait for the
   * fence to be signaled before returning, but the HWC must wait for
   * all buffers to be signaled before reading from them.
   *
   * HWC2_COMPOSITION_CLIENT layers will never have an acquire fence, since
   * reads from them are complete before the framebuffer is ready for
   * display.
   *
   * The HWC takes ownership of the acquireFenceFd and is responsible
   * for closing it when no longer needed.
   */
  int mAcquireFenceFd;

  /* During set() the HWC must set this field to a file descriptor for
   * a sync fence object that will signal after the HWC has finished
   * reading from the buffer. The field is ignored by prepare(). Each
   * layer should have a unique file descriptor, even if more than one
   * refer to the same underlying fence object; this allows each to be
   * closed independently.
   *
   * If buffer reads can complete at significantly different times,
   * then using independent fences is preferred. For example, if the
   * HWC handles some layers with a blit engine and others with
   * overlays, then the blit layers can be reused immediately after
   * the blit completes, but the overlay layers can't be reused until
   * a subsequent frame has been displayed.
   *
   * Since HWC doesn't read from HWC2_COMPOSITION_CLIENT layers, it shouldn't
   * produce a release fence for them. The releaseFenceFd will be -1
   * for these layers when set() is called.
   *
   * The HWC client taks ownership of the releaseFenceFd and is
   * responsible for closing it when no longer needed.
   */
  int mReleaseFenceFd;

  //
  // Alpha value applied to the whole layer.
  //
  // We use 0.0=invisible 1.0=opaque
  // This is HWC2 style/
  // HWC1 uses 255=opaque so conversion is required.
  float mPlaneAlpha;

  /*
   * Internal variables for validation
   *
   * Content validity at time of onSet
   */
  Hwcval::ValidityType mValidity;

 public:
  std::shared_ptr<DrmShimBuffer> GetBuf() const;
  void SetBuf(std::shared_ptr<DrmShimBuffer> buf);
  HWCNativeHandle GetHandle() const;
  CompositionType GetCompositionType() const;
  uint32_t GetTransformId() const;
  void SetTransformId(uint32_t transformId);
  hwcomposer::HWCBlending GetBlendingType() const;
  void SetBlendingType(hwcomposer::HWCBlending blending);
  const hwcomposer::HwcRect<float>& GetSourceCrop() const;
  void SetSourceCrop(const hwcomposer::HwcRect<float>& sourceCrop);
  const hwcomposer::HwcRect<int>& GetDisplayFrame() const;
  void SetDisplayFrame(const hwcomposer::HwcRect<int>& displayFrame);
  void SetAcquireFenceFd(int fence);
  int GetReleaseFenceFd();
  float GetPlaneAlpha() const;
  void SetFlags(uint32_t flags);
  uint32_t GetFlags() const;

  void SetValidity(Hwcval::ValidityType validity);
  Hwcval::ValidityType GetValidity();

  const ValRegion& GetVisibleRegion() const;
  hwcomposer::HwcRect<int> GetVisibleRegionBounds() const;
};

inline Hwcval::CompositionType ValLayer::GetCompositionType() const {
  return mCompositionType;
}

inline std::shared_ptr<DrmShimBuffer> ValLayer::GetBuf() const {
  return mBuf;
}

inline void ValLayer::SetBuf(std::shared_ptr<DrmShimBuffer> buf) {
  mBuf = buf;
}

inline uint32_t ValLayer::GetTransformId() const {
  return mTransform;
}

inline void ValLayer::SetTransformId(uint32_t transformId) {
  mTransform = transformId;
}

inline hwcomposer::HWCBlending ValLayer::GetBlendingType() const {
  return mBlending;
}

inline void ValLayer::SetBlendingType(hwcomposer::HWCBlending blending) {
  mBlending = blending;
}

inline const hwcomposer::HwcRect<float>& ValLayer::GetSourceCrop() const {
  return mSourceCropf;
}

inline void ValLayer::SetSourceCrop(
    const hwcomposer::HwcRect<float>& sourceCrop) {
  mSourceCropf = sourceCrop;
}

inline const hwcomposer::HwcRect<int>& ValLayer::GetDisplayFrame() const {
  return mDisplayFrame;
}

inline void ValLayer::SetDisplayFrame(
    const hwcomposer::HwcRect<int>& displayFrame) {
  mDisplayFrame = displayFrame;
}

inline void ValLayer::SetAcquireFenceFd(int fence) {
  mAcquireFenceFd = fence;
}

inline int ValLayer::GetReleaseFenceFd() {
  return mReleaseFenceFd;
}

inline float ValLayer::GetPlaneAlpha() const {
  return mPlaneAlpha;
}

inline void ValLayer::SetFlags(uint32_t flags) {
  mFlags = flags;
}

inline uint32_t ValLayer::GetFlags() const {
  return mFlags;
}

inline Hwcval::ValidityType ValLayer::GetValidity() {
  return mValidity;
}

inline void ValLayer::SetValidity(Hwcval::ValidityType validity) {
  mValidity = validity;
}

inline const ValRegion& ValLayer::GetVisibleRegion() const {
  return mVisibleRegionScreen;
}

inline hwcomposer::HwcRect<int> ValLayer::GetVisibleRegionBounds() const {
  return mVisibleRegionScreen.GetBounds(mDisplayFrame);
}

/*
 * Description of the contents to output on a display.
 *
 * This is the top-level structure passed to the prepare and set calls to
 * negotiate and commit the composition of a display image.
 */
class LayerList {
 public:
  struct VideoFlags {
    bool mSingleFullScreenVideo : 1;
    TriState mFullScreenVideo : 2;
    bool mPartScreenVideo : 1;
    void Log(const char* str, uint32_t d, uint32_t hwcFrame);
  };

 protected:
  /* File descriptor referring to a Sync HAL fence object which will signal
   * when this composition is retired. For a physical display, a composition
   * is retired when it has been replaced on-screen by a subsequent set. For
   * a virtual display, the composition is retired when the writes to
   * outputBuffer are complete and can be read. The fence object is created
   * and returned by the set call; this field will be -1 on entry to prepare
   * and set. SurfaceFlinger will close the returned file descriptor.
   */
  int mRetireFenceFd;

  /* These fields are used for virtual displays when the h/w composer
   * version is at least HWC_DEVICE_VERSION_1_3. */

  /* outbuf is the buffer that receives the composed image for
  * virtual displays.
  */
  HWCNativeHandle mOutbuf;

  /* File descriptor for a fence that will signal when outbuf is
   * ready to be written.
   */
  int mOutbufAcquireFenceFd;

  uint32_t mFlags;

  VideoFlags mVideoFlags;

  /* List of layers that will be composed on the display.
   */
  std::vector<ValLayer> mLayers;

  // This is provided so we can cross-check that the number of layers is as we
  // expect.
  uint32_t mNumLayers;

 public:
  // Option to pre-allocate vector for performance
  LayerList(uint32_t numLayers = 0);

  // Add a layer
  void Add(const ValLayer& layer);

  // Returns true if layer stack contains at least one video layer
  bool IsVideo();

  uint32_t GetNumLayers();
  ValLayer& GetLayer(uint32_t ix);

  int GetRetireFence();
  void SetRetireFence(int fence);
  HWCNativeHandle GetOutbuf();

  void SetVideoFlags(VideoFlags videoFlags);
  VideoFlags GetVideoFlags();
};

inline uint32_t LayerList::GetNumLayers() {
  return mLayers.size();
}

inline ValLayer& LayerList::GetLayer(uint32_t ix) {
  return mLayers[ix];
}

inline int LayerList::GetRetireFence() {
  return mRetireFenceFd;
}

inline void LayerList::SetRetireFence(int fence) {
  mRetireFenceFd = fence;
}

inline HWCNativeHandle LayerList::GetOutbuf() {
  return mOutbuf;
}

inline void LayerList::SetVideoFlags(LayerList::VideoFlags videoFlags) {
  mVideoFlags = videoFlags;
}

inline LayerList::VideoFlags LayerList::GetVideoFlags() {
  return mVideoFlags;
}

}

#endif  // __HwcvalContent_h__
