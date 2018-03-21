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

#ifndef __DrmShimTransform_h__
#define __DrmShimTransform_h__

#include <stdint.h>

#include "Hwcval.h"
#include "HwcTestState.h"
#include "HwcvalEnums.h"

class DrmShimBuffer;
class HwcTestCrtc;

namespace Hwcval {
class ValLayer;
}

class DrmShimTransform {
 protected:
  // The source buffer to be transformed
  std::shared_ptr<DrmShimBuffer> mBuf;

  uint64_t mZOrder;
  uint32_t mZOrderLevels;  // Number of levels of mZOrder which have been used
                           // (from MSB)

  hwcomposer::HwcRect<float> mSourcecropf;

  double mXscale;
  double mYscale;
  double mXoffset;
  double mYoffset;

  // Rotation and flip
  int mTransform;

  // Layer index at which layer match is found
  uint32_t mLayerIndex;

  // Buffer will be decrypted
  bool mDecrypt;

  // Plane blending
  hwcomposer::HWCBlending mBlending;
  bool mHasPixelAlpha;
  float mPlaneAlpha;

  // Composition of this buffer descends from an iVP composition (directly or
  // indirectly).
  // Bit map, bit number given by enum values Hwcval::BufferSourceType::*
  // (Hwcval::BufferSourceType)
  uint32_t mSources;

  // Table to define effect of one transform after another
  static const int mTransformTable[hwcomposer::HWCTransform::kMaxTransform]
                                  [hwcomposer::HWCTransform::kMaxTransform];
  static const char* mTransformNames[];

 public:
  DrmShimTransform();
  DrmShimTransform(std::shared_ptr<DrmShimBuffer>& buf, double width = 0.0,
                   double height = 0.0);
  DrmShimTransform(double sw, double sh, double dw, double dh);
  DrmShimTransform(std::shared_ptr<DrmShimBuffer>& buf, uint32_t layerIx,
                   const hwcval_layer_t* layer);
  DrmShimTransform(std::shared_ptr<DrmShimBuffer>& buf, uint32_t layerIx,
                   const Hwcval::ValLayer& layer);
  ~DrmShimTransform();

  // Combine transforms one after another
  // Not commutative.
  DrmShimTransform(DrmShimTransform& a, DrmShimTransform& b,
                   HwcTestCheckType = eLogDrm, const char* str = "");

  // Invert an existing transform
  DrmShimTransform Inverse();

  // Accessors
  std::shared_ptr<DrmShimBuffer> GetBuf();
  const DrmShimBuffer* GetConstBuf() const;
  DrmShimTransform* SetBuf(std::shared_ptr<DrmShimBuffer>& buf);
  DrmShimTransform* ClearBuf();

  uint64_t GetZOrder() const;
  DrmShimTransform* SetPlaneOrder(uint32_t planeOrder);

  hwcomposer::HwcRect<float>& GetSourceCrop();
  void SetSourceCrop(double left, double top, double right, double bottom);
  void SetSourceCrop(const hwcomposer::HwcRect<float>& rect);

  void SetDisplayOffset(int32_t x, int32_t y);
  void SetDisplayFrameSize(int32_t w, int32_t h);
  double GetXScale();
  double GetYScale();
  double GetXOffset();
  double GetYOffset();
  void GetEffectiveDisplayFrame(hwcomposer::HwcRect<int>& rect);
  bool IsDfIntersecting(int32_t width, int32_t height);

  double DisplayRight();
  double DisplayBottom();

  uint32_t GetTransform();
  DrmShimTransform* SetRotation(uint32_t rotation);
  DrmShimTransform* SetTransform(uint32_t transform);

  void SetBlend(hwcomposer::HWCBlending blend, bool hasPixelAlpha,
                float planeAlpha);
  uint32_t GetBlend();
  bool GetPlaneAlpha(uint32_t& planeAlpha);

  const char* GetTransformName() const;
  static const char* GetTransformName(uint32_t transform);

  DrmShimTransform* SetLayerIndex(uint32_t layerIndex);
  uint32_t GetLayerIndex();

  DrmShimTransform* SetDecrypt(bool decrypt);
  bool IsDecrypted();

  // Set/get sources/composition types used to create this buffer (directly or
  // indirectly)
  void SetSources(uint32_t sources);
  bool IsFromSfComp();
  const char* SourcesStr(char* strbuf) const;
  static const char* SourcesStr(uint32_t sources, char* strbuf);

  // Compare layer transform with actual transform
  // Report any errors.
  bool Compare(DrmShimTransform& actual,  // The actual transform derived from
                                          // HWC composition and display
               DrmShimTransform& orig,    // The original, uncropped layer
                                          // transform for logging purposes only
               int display,  // Relevant display number, for logging
               HwcTestCrtc* crtc,
               uint32_t& cropErrorCount,   // Total number of crop errors
                                           // detected (cumulative)
               uint32_t& scaleErrorCount,  // Total number of scale errors
                                           // detected (cumulative)
               uint32_t hwcFrame);         // Frame number for logging

  // Compare display frame  (only) of layer transform with actual transform
  bool CompareDf(DrmShimTransform& actual,  // The actual transform derived from
                                            // HWC composition and display
                 DrmShimTransform& orig,    // The original, uncropped layer
                                          // transform for logging purposes only
                 int display,  // Relevant display number, for logging
                 HwcTestCrtc* crtc,
                 uint32_t& scaleErrorCount);  // Total number of scale errors
                                              // detected (cumulative)

  static uint32_t Inverse(uint32_t transform);

  void Log(int priority, const char* str) const;
  const char* GetBlendingStr(char* strbuf) const;
  static std::string GetBlendingStr(hwcomposer::HWCBlending blending);
};

class DrmShimFixedAspectRatioTransform : public DrmShimTransform {
 public:
  DrmShimFixedAspectRatioTransform(uint32_t sw, uint32_t sh, uint32_t dw,
                                   uint32_t dh);
};

class DrmShimCroppedLayerTransform : public DrmShimTransform {
 public:
  DrmShimCroppedLayerTransform(std::shared_ptr<DrmShimBuffer>& buf,
                               uint32_t layerIx, const Hwcval::ValLayer& layer,
                               HwcTestCrtc* crtc);
};

inline DrmShimTransform* DrmShimTransform::SetBuf(
    std::shared_ptr<DrmShimBuffer>& buf) {
  mBuf = buf;
  return this;
}

inline DrmShimTransform* DrmShimTransform::ClearBuf() {
  mBuf = 0;
  return this;
}

inline std::shared_ptr<DrmShimBuffer> DrmShimTransform::GetBuf() {
  return mBuf;
}

inline const DrmShimBuffer* DrmShimTransform::GetConstBuf() const {
  return mBuf.get();
}

inline uint64_t DrmShimTransform::GetZOrder() const {
  return mZOrder;
}

inline hwcomposer::HwcRect<float>& DrmShimTransform::GetSourceCrop() {
  return mSourcecropf;
}

inline void DrmShimTransform::SetSourceCrop(double left, double top,
                                            double width, double height) {
  mSourcecropf.left = left;
  mSourcecropf.top = top;
  mSourcecropf.right = left + width;
  mSourcecropf.bottom = top + height;
}

inline void DrmShimTransform::SetSourceCrop(
    const hwcomposer::HwcRect<float>& rect) {
  mSourcecropf = rect;
}

inline double DrmShimTransform::GetXScale() {
  return mXscale;
}

inline double DrmShimTransform::GetYScale() {
  return mYscale;
}

inline double DrmShimTransform::GetXOffset() {
  return mXoffset;
}

inline double DrmShimTransform::GetYOffset() {
  return mYoffset;
}

inline double DrmShimTransform::DisplayRight() {
  if (mTransform & hwcomposer::HWCTransform::kTransform90) {
    return mXoffset + (mSourcecropf.bottom - mSourcecropf.top) * mXscale;
  } else {
    return mXoffset + (mSourcecropf.right - mSourcecropf.left) * mXscale;
  }
}

inline double DrmShimTransform::DisplayBottom() {
  if (mTransform & hwcomposer::HWCTransform::kTransform90) {
    return mYoffset + (mSourcecropf.right - mSourcecropf.left) * mYscale;
  } else {
    return mYoffset + (mSourcecropf.bottom - mSourcecropf.top) * mYscale;
  }
}

inline uint32_t DrmShimTransform::GetTransform() {
  return mTransform;
}

inline const char* DrmShimTransform::GetTransformName() const {
  return GetTransformName(mTransform);
}

inline DrmShimTransform* DrmShimTransform::SetLayerIndex(uint32_t layerIndex) {
  mLayerIndex = layerIndex;
  return this;
}

inline uint32_t DrmShimTransform::GetLayerIndex() {
  return mLayerIndex;
}

inline DrmShimTransform* DrmShimTransform::SetDecrypt(bool decrypt) {
  mDecrypt = decrypt;
  return this;
}

inline bool DrmShimTransform::IsDecrypted() {
  return mDecrypt;
}

inline void DrmShimTransform::SetBlend(hwcomposer::HWCBlending blend,
                                       bool hasPixelAlpha, float planeAlpha) {
  mBlending = blend;
  mHasPixelAlpha = hasPixelAlpha;
  mPlaneAlpha = planeAlpha;
}

inline void DrmShimTransform::SetSources(uint32_t sources) {
  mSources = sources;
}

// Sort should be into Z-order sequence
bool operator<(const DrmShimTransform& lhs, const DrmShimTransform& rhs);

hwcomposer::HwcRect<int> InverseTransformRect(hwcomposer::HwcRect<int>& rect,
                                              const Hwcval::ValLayer& layer);

#endif  // __DrmShimTransform_h__
