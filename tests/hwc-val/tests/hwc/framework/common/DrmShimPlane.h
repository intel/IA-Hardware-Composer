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

#ifndef __DrmShimPlane_h__
#define __DrmShimPlane_h__

#include <stdint.h>
#include "DrmShimBuffer.h"

// Forward reference
class HwcTestCrtc;

class DrmShimPlane {
 public:
  // Tiling enumeration to store tiling type decoded from modifier
  enum PlaneTiling {
    ePlaneXTiled = 0,
    ePlaneYTiled,
    ePlaneYfTiled,
    ePlaneLinear
  };

 protected:
  // Plane Id
  uint32_t mPlaneId;

  // Plane index within CRTC. 0 for main plane.
  uint32_t mPlaneIx;

  // Current Device-Specific buffer Id (FB ID if DRM)
  int64_t mDsId;

  /// CRTC for this plane
  HwcTestCrtc* mCrtc;

  /// Framebuffer and hardware transformation that will be set at page flip
  /// event
  DrmShimTransform mTransform;

  /// Framebuffer that was set at last page flip event
  std::shared_ptr<DrmShimBuffer> mFlippedBuffer;

  /// Redraw is expected this frame
  bool mRedrawExpected;

  /// Did the last attempt to set the buffer to be displayed fail?
  bool mSetDisplayFailed;

  // Buffer bits per pixel
  uint32_t mBpp;

  // Pixel format
  uint32_t mPixelFormat;

  // Aux buffer data members
  bool mHasAuxBuffer;
  uint32_t mAuxPitch;
  uint32_t mAuxOffset;

  // Data member to hold tiling information
  PlaneTiling mTiling;

  int64_t mDrmCallStartTime;

  // ADF additions
  bool mBufferUpdated;

 public:
  DrmShimPlane(uint32_t planeId);

  DrmShimPlane(uint32_t planeId, HwcTestCrtc* crtc);

  virtual ~DrmShimPlane();

  HwcTestCrtc* GetCrtc();
  void SetCrtc(HwcTestCrtc* crtc);

  uint32_t GetPlaneId();
  uint32_t GetDrmPlaneId();
  bool IsMainPlane();

  void SetPlaneIndex(uint32_t ix);
  uint32_t GetPlaneIndex();

  void SetHwTransform(uint32_t hwTransform);

  void SetDecrypt(bool decrypt);
  bool IsDecrypted();

  void SetRedrawExpected(bool redrawExpected);
  bool IsRedrawExpected();

  // Is the stated buffer in use by this plane?
  bool IsUsing(std::shared_ptr<DrmShimBuffer> buf);

  // Drm Only: access Framebuffer Id
  void SetCurrentDsId(int64_t dsId);
  int64_t GetCurrentDsId();

  DrmShimTransform& GetTransform();
  void SetBuf(std::shared_ptr<DrmShimBuffer>& buf);
  void ClearBuf();
  std::shared_ptr<DrmShimBuffer> GetCurrentBuf();

  uint32_t GetZOrder();

  void SetDisplayFrame(int32_t x, int32_t y, uint32_t w, uint32_t h);
  void SetSourceCrop(float left, float top, float width, float height);

  // Bits-per-pixel for current buffer
  void SetBpp(uint32_t bpp);
  uint32_t GetBpp();

  // Pixel format
  void SetPixelFormat(uint32_t pixelFormat);
  uint32_t GetPixelFormat();

  // Aux buffer accessors
  void SetHasAuxBuffer(bool hasAux);
  bool GetHasAuxBuffer();
  void SetAuxPitch(uint32_t auxPitch);
  uint32_t GetAuxPitch();
  void SetAuxOffset(uint32_t auxOffset);
  uint32_t GetAuxOffset();

  // Tiling related accessors
  void SetTilingFromModifier(__u64 modifier);
  PlaneTiling GetTiling();

  // Failure of last attempt to set the buffer on the plane
  void SetDisplayFailed(bool failed);
  bool DidSetDisplayFail();

  // Drm call duration evaluation
  void DrmCallStart();
  int64_t GetDrmCallDuration();

  // Page Flip event processing
  void Flip();

  // Checks
  void Expand(DrmShimSortedTransformVector& transforms);
  bool FormatHasPixelAlpha();
  void ValidateFormat();

  // ADF additions
  void SetBufferUpdated(bool updated);
  bool IsBufferUpdated();

  // Logging
  void Log(int priority);
};

inline HwcTestCrtc* DrmShimPlane::GetCrtc() {
  return mCrtc;
}

inline void DrmShimPlane::SetCrtc(HwcTestCrtc* crtc) {
  mCrtc = crtc;
}

inline uint32_t DrmShimPlane::GetPlaneId() {
  return mPlaneId;
}

inline void DrmShimPlane::SetPlaneIndex(uint32_t ix) {
  mPlaneIx = ix;
  HWCLOGD("Plane %d index set to %d", mPlaneId, mPlaneIx);
}

inline uint32_t DrmShimPlane::GetPlaneIndex() {
  return mPlaneIx;
}

inline bool DrmShimPlane::IsMainPlane() {
  // HWCLOGI("IsMainPlane: Plane %d Crtc %p Id %d",GetPlaneId(), mCrtc,
  // mCrtc?(mCrtc->GetCrtcId()):0);
  return (mPlaneIx == 0);
}

inline void DrmShimPlane::SetHwTransform(uint32_t hwTransform) {
  mTransform.SetTransform(hwTransform);
}

inline void DrmShimPlane::SetDecrypt(bool decrypt) {
  mTransform.SetDecrypt(decrypt);
}

inline bool DrmShimPlane::IsDecrypted() {
  return mTransform.IsDecrypted();
}

inline void DrmShimPlane::SetRedrawExpected(bool redrawExpected) {
  mRedrawExpected = redrawExpected;
}

inline bool DrmShimPlane::IsRedrawExpected() {
  bool ret = mRedrawExpected;
  mRedrawExpected = false;
  return ret;
}

inline void DrmShimPlane::SetCurrentDsId(int64_t dsId) {
  mDsId = dsId;
}

inline int64_t DrmShimPlane::GetCurrentDsId() {
  return mDsId;
}

inline DrmShimTransform& DrmShimPlane::GetTransform() {
  return mTransform;
}

inline void DrmShimPlane::SetBuf(std::shared_ptr<DrmShimBuffer>& buf) {
  mTransform.SetBuf(buf);
  mBufferUpdated = true;
}

inline void DrmShimPlane::ClearBuf() {
  mTransform.ClearBuf();
  mDsId = 0;
}

inline std::shared_ptr<DrmShimBuffer> DrmShimPlane::GetCurrentBuf() {
  return mTransform.GetBuf();
}

inline void DrmShimPlane::SetBpp(uint32_t bpp) {
  mBpp = bpp;
}

inline uint32_t DrmShimPlane::GetBpp() {
  return mBpp;
}

inline void DrmShimPlane::SetPixelFormat(uint32_t pixelFormat) {
  mPixelFormat = pixelFormat;
}

inline uint32_t DrmShimPlane::GetPixelFormat() {
  return mPixelFormat;
}

inline void DrmShimPlane::SetHasAuxBuffer(bool hasAux) {
  mHasAuxBuffer = hasAux;
}

inline bool DrmShimPlane::GetHasAuxBuffer() {
  return mHasAuxBuffer;
}

inline void DrmShimPlane::SetAuxPitch(uint32_t auxPitch) {
  mAuxPitch = auxPitch;
}

inline uint32_t DrmShimPlane::GetAuxPitch() {
  return mAuxPitch;
}

inline void DrmShimPlane::SetAuxOffset(uint32_t auxOffset) {
  mAuxOffset = auxOffset;
}

inline uint32_t DrmShimPlane::GetAuxOffset() {
  return mAuxOffset;
}

inline DrmShimPlane::PlaneTiling DrmShimPlane::GetTiling() {
  return mTiling;
}

inline void DrmShimPlane::SetBufferUpdated(bool updated) {
  mBufferUpdated = updated;
}

inline bool DrmShimPlane::IsBufferUpdated() {
  return mBufferUpdated;
}

inline void DrmShimPlane::SetDisplayFailed(bool failed) {
  mSetDisplayFailed = failed;
}

inline bool DrmShimPlane::DidSetDisplayFail() {
  return mSetDisplayFailed;
}

#endif  // __DrmShimPlane_h__
