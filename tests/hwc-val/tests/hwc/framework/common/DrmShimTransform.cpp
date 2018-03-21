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

#include "DrmShimTransform.h"
#include "hardware/hwcomposer2.h"
#include "HwcTestDefs.h"
#include "HwcTestUtil.h"
#include "HwcTestState.h"
#include "DrmShimBuffer.h"
#include "HwcTestCrtc.h"
#include "HwcvalHwc2Content.h"

#include <math.h>

using namespace Hwcval;

#define Z_ORDER_LEVEL_BITS 8
#define MOST_SIGNIFICANT_Z_ORDER_BITS (Z_ORDER_LEVEL_BITS * 7)
// This means we can have at most 4 levels of composition, and no more than 256
// layers in any one composition,
// because we use one byte per level and the Z-order value is stored in a
// uint32_t.
// Should be enough for anybody, but if we ever need more, we can always use
// uint64_t.

// This table gives the result of applying two transforms one after another
// where the primary index is the first transform and the secondary index is
// the second transform.
// The actual transform values (from the enum) are all in the range 0-7
// so this constitutes a closed set.
const int DrmShimTransform::mTransformTable
    [hwcomposer::HWCTransform::kMaxTransform]
    [hwcomposer::HWCTransform::kMaxTransform] = {
        {hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform270},
        {hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform135},
        {hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform45},
        {hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform90},
        {hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kIdentity},
        {hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kReflectY},
        {hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kReflectX},
        {hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform180}};

const char* DrmShimTransform::mTransformNames[] = {
    "None", "FlipH", "FlipV", "Rot180", "Rot90", "Flip135", "Flip45", "Rot270"};

// Only exists for debug logging
DrmShimTransform::~DrmShimTransform() {
  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::~DrmShimTransform() deleted transform@%p",
               this);
}

// Null transform, to support container classes
DrmShimTransform::DrmShimTransform()
    : mZOrder(0),
      mZOrderLevels(1),
      mXscale(1.0),
      mYscale(1.0),
      mXoffset(0.0),
      mYoffset(0.0),
      mTransform(0),
      mLayerIndex(eNoLayer),
      mDecrypt(false),
      mBlending(hwcomposer::HWCBlending::kBlendingNone),
      mSources(0) {
  // TODO: pass in image size as parameters so we have correct right & bottom
  // crop
  mSourcecropf.left = mSourcecropf.top = 0.0;
  mSourcecropf.right = 0;
  mSourcecropf.bottom = 0;

  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::DrmShimTransform() Created transform@%p",
               this);
}

// Identity transform, for OVERLAYs
DrmShimTransform::DrmShimTransform(std::shared_ptr<DrmShimBuffer>& buf,
                                   double width, double height)
    : mBuf(buf),
      mZOrder(0),
      mZOrderLevels(1),
      mXscale(1.0),
      mYscale(1.0),
      mXoffset(0.0),
      mYoffset(0.0),
      mTransform(0),
      mLayerIndex(eNoLayer),
      mDecrypt(false),
      mBlending(hwcomposer::HWCBlending::kBlendingNone),
      mHasPixelAlpha(true),
      mPlaneAlpha(1.0),
      mSources(0) {
  // TODO: pass in image size as parameters so we have correct right & bottom
  // crop
  mSourcecropf.left = mSourcecropf.top = 0.0;
  mSourcecropf.right = width;
  mSourcecropf.bottom = height;
  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::DrmShimTransform(&buf, double, double) "
               "Created transform@%p",
               this);
}

// Scaling transform, for panel fitter
DrmShimTransform::DrmShimTransform(double sw, double sh, double dw, double dh)
    : mBuf(0),
      mZOrder(0),
      mZOrderLevels(1),
      mXscale(dw / sw),
      mYscale(dh / sh),
      mXoffset(0.0),
      mYoffset(0.0),
      mTransform(0),
      mLayerIndex(eNoLayer),
      mDecrypt(false),
      mBlending(hwcomposer::HWCBlending::kBlendingNone),
      mHasPixelAlpha(true),
      mPlaneAlpha(1.0),
      mSources(0) {
  // TODO: pass in image size as parameters so we have correct right & bottom
  // crop
  mSourcecropf.left = mSourcecropf.top = 0.0;
  mSourcecropf.right = sw;
  mSourcecropf.bottom = sh;
  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::DrmShimTransform(double, double, double, "
               "double) Created transform@%p",
               this);
}

// Transform creation for SF composition
DrmShimTransform::DrmShimTransform(std::shared_ptr<DrmShimBuffer>& buf,
                                   uint32_t layerIx,
                                   const hwcval_layer_t* layer)
    : mBuf(buf),
      mZOrder(uint64_t(layerIx) << MOST_SIGNIFICANT_Z_ORDER_BITS),
      mZOrderLevels(1),
      mXoffset(layer->displayFrame.left),
      mYoffset(layer->displayFrame.top),
      mTransform(layer->transform),
      mLayerIndex(eNoLayer),
      mDecrypt(false),
      mBlending(Hwc2BlendingTypeToHwcval(layer->blending)),
      mHasPixelAlpha(buf.get() ? buf->FormatHasPixelAlpha() : false),
      mPlaneAlpha(float(layer->planeAlpha) / 255.0),
      mSources(0) {
  mSourcecropf.left = layer->sourceCropf.left;
  mSourcecropf.right = layer->sourceCropf.right;
  mSourcecropf.top = layer->sourceCropf.top;
  mSourcecropf.bottom = layer->sourceCropf.bottom;

  hwcomposer::HwcRect<int> displayframe;
  displayframe.left = layer->displayFrame.left;
  displayframe.right = layer->displayFrame.right;
  displayframe.top = layer->displayFrame.top;
  displayframe.bottom = layer->displayFrame.bottom;

  uint32_t transform = layer->transform;

  if (transform & hwcomposer::HWCTransform::kTransform90) {
    mXscale = (displayframe.right - displayframe.left) /
	      (mSourcecropf.bottom - mSourcecropf.top);
    mYscale = (displayframe.bottom - displayframe.top) /
	      (mSourcecropf.right - mSourcecropf.left);
  } else {
    mXscale = (displayframe.right - displayframe.left) /
	      (mSourcecropf.right - mSourcecropf.left);
    mYscale = (displayframe.bottom - displayframe.top) /
	      (mSourcecropf.bottom - mSourcecropf.top);
  }

  if (HWCCOND(eLogCombinedTransform)) {
    Log(ANDROID_LOG_VERBOSE, "SF Transform:");
  }
  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::DrmShimTransform(&buf, int, layer*) Created "
               "transform@%p",
               this);
}

DrmShimTransform::DrmShimTransform(std::shared_ptr<DrmShimBuffer>& buf,
                                   uint32_t layerIx,
                                   const Hwcval::ValLayer& layer)
    : mBuf(buf),
      mZOrder(uint64_t(layerIx) << MOST_SIGNIFICANT_Z_ORDER_BITS),
      mZOrderLevels(1),
      mSourcecropf(layer.GetSourceCrop()),
      mXoffset(layer.GetDisplayFrame().left),
      mYoffset(layer.GetDisplayFrame().top),
      mTransform(layer.GetTransformId()),
      mLayerIndex(eNoLayer),
      mDecrypt(false),
      mBlending(layer.GetBlendingType()),
      mHasPixelAlpha(buf.get() ? buf->FormatHasPixelAlpha() : false),
      mPlaneAlpha(layer.GetPlaneAlpha()),
      mSources(0) {
  hwcomposer::HwcRect<int> displayframe = layer.GetDisplayFrame();

  uint32_t transform = layer.GetTransformId();

  if (transform & hwcomposer::HWCTransform::kTransform90) {
    mXscale = (displayframe.right - displayframe.left) /
	      (mSourcecropf.bottom - mSourcecropf.top);
    mYscale = (displayframe.bottom - displayframe.top) /
	      (mSourcecropf.right - mSourcecropf.left);
  } else {
    mXscale = (displayframe.right - displayframe.left) /
	      (mSourcecropf.right - mSourcecropf.left);
    mYscale = (displayframe.bottom - displayframe.top) /
	      (mSourcecropf.bottom - mSourcecropf.top);
  }

  if (HWCCOND(eLogCombinedTransform)) {
    Log(ANDROID_LOG_VERBOSE, "SF Transform from LLQ:");
  }
  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::DrmShimTransform(&buf, int, &layer) Created "
               "transform@%p",
               this);
}

DrmShimTransform DrmShimTransform::Inverse() {
  DrmShimTransform result;
  hwcomposer::HwcRect<int> df;
  GetEffectiveDisplayFrame(df);
  result.mSourcecropf = hwcomposer::HwcRect<float>(df);

  if (mTransform & hwcomposer::HWCTransform::kTransform90) {
    result.mXscale = 1 / mYscale;
    result.mYscale = 1 / mXscale;
  } else {
    result.mXscale = 1 / mXscale;
    result.mYscale = 1 / mYscale;
  }

  result.mXoffset = mSourcecropf.left;
  result.mYoffset = mSourcecropf.top;

  result.mTransform = Inverse(mTransform);

  return result;
}

// Combine transforms one after another
// a then b, Not commutative.
DrmShimTransform::DrmShimTransform(DrmShimTransform& a, DrmShimTransform& b,
                                   HwcTestCheckType cond, const char* str)
    : mBuf(a.mBuf),
      mZOrder(b.mZOrder |
              (a.mZOrder >> (b.mZOrderLevels * Z_ORDER_LEVEL_BITS))),
      mZOrderLevels(a.mZOrderLevels + b.mZOrderLevels),
      mLayerIndex(a.mLayerIndex),
      mSources(a.mSources) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  if (HwcTestState::getInstance()->IsCheckEnabled(cond)) {
    HWCLOGV("Transform product %s", str);
    a.Log(ANDROID_LOG_VERBOSE, "  a:");
    b.Log(ANDROID_LOG_VERBOSE, "  b:");
  }

  if ((mZOrderLevels * Z_ORDER_LEVEL_BITS) > (8 * sizeof(mZOrder))) {
    HWCERROR(eCheckInternalZOrder,
             "Maximum Z-order nesting capability exceeded (%d+%d=%d)",
             a.mZOrderLevels, b.mZOrderLevels, mZOrderLevels);
  }

  if ((a.mTransform < hwcomposer::HWCTransform::kMaxTransform) &&
      (b.mTransform < hwcomposer::HWCTransform::kMaxTransform)) {
    mTransform = mTransformTable[a.mTransform][b.mTransform];
  } else {
    HWCERROR(eCheckInternalError, "Invalid transform (%d or %d)", a.mTransform,
             b.mTransform);
  }

  double xOrigin;
  double yOrigin;

  // In any transform with an element of 90 degree rotation in it
  //  - i.e. Rot90, Rot270, Flip45 & Flip135
  if (a.mTransform & hwcomposer::HWCTransform::kTransform90) {
    if (a.mTransform & hwcomposer::HWCTransform::kReflectY) {
      // Translate 2nd source crop into space of 1st translation and use
      // whichever coords are more restrictive
      double xcrop = b.mSourcecropf.top - a.mYoffset;
      if (xcrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.left = a.mSourcecropf.left;
        yOrigin = -xcrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.left = xcrop / a.mYscale + a.mSourcecropf.left;
        yOrigin = 0;
      }

      mSourcecropf.right =
          min<double>((b.mSourcecropf.bottom - a.mYoffset) / a.mYscale +
                          a.mSourcecropf.left,
                      a.mSourcecropf.right);
    } else {
      // Translate 2nd source crop into space of 1st translation and use
      // whichever coords are more restrictive
      double xcrop = b.mSourcecropf.top - a.mYoffset;
      if (xcrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.right = a.mSourcecropf.right;
        yOrigin = -xcrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.right = a.mSourcecropf.right - xcrop / a.mYscale;
        yOrigin = 0;
      }

      mSourcecropf.left =
          a.mSourcecropf.left -
          min<double>((b.mSourcecropf.bottom - a.mYoffset) / a.mYscale, 0);
    }

    if (a.mTransform & hwcomposer::HWCTransform::kReflectX) {
      double ycrop = b.mSourcecropf.left - a.mXoffset;
      if (ycrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.top = a.mSourcecropf.top;
        xOrigin = -ycrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.top = ycrop / a.mXscale + a.mSourcecropf.top;
        xOrigin = 0;
      }

      mSourcecropf.bottom = min<double>(
          (b.mSourcecropf.right - a.mXoffset) / a.mXscale + a.mSourcecropf.top,
          a.mSourcecropf.bottom);
    } else {
      double ycrop = b.mSourcecropf.left - a.mXoffset;
      if (ycrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.bottom = a.mSourcecropf.bottom;
        xOrigin = -ycrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.bottom = a.mSourcecropf.bottom - ycrop / a.mXscale;
        xOrigin = 0;
      }

      mSourcecropf.top =
          max<double>(a.mSourcecropf.bottom -
                          (b.mSourcecropf.right - a.mXoffset) / a.mXscale,
                      a.mSourcecropf.top);
    }

  } else {
    // No Rot90 in a. Could still be FlipH/FLipV.

    if (a.mTransform & hwcomposer::HWCTransform::kReflectX) {
      // Translate 2nd source crop into space of 1st translation and use
      // whichever coords are more restrictive
      double xcrop = b.mSourcecropf.left - a.mXoffset;
      if (xcrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.right = a.mSourcecropf.right;
        xOrigin = -xcrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.right = a.mSourcecropf.right - xcrop / a.mXscale;
        xOrigin = 0;
      }

      mSourcecropf.left =
          max<double>(a.mSourcecropf.right +
                          ((a.mXoffset - b.mSourcecropf.right) / a.mXscale),
                      a.mSourcecropf.left);
    } else {
      // Translate 2nd source crop into space of 1st translation and use
      // whichever coords are more restrictive
      double xcrop = b.mSourcecropf.left - a.mXoffset;
      if (xcrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.left = a.mSourcecropf.left;
        xOrigin = -xcrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.left = xcrop / a.mXscale + a.mSourcecropf.left;
        xOrigin = 0;
      }

      mSourcecropf.right = min<double>(
          (b.mSourcecropf.right - a.mXoffset) / a.mXscale + a.mSourcecropf.left,
          a.mSourcecropf.right);
    }

    if (a.mTransform & hwcomposer::HWCTransform::kReflectY) {
      double ycrop = b.mSourcecropf.top - a.mYoffset;
      if (ycrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.bottom = a.mSourcecropf.bottom;
        yOrigin = -ycrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.bottom = a.mSourcecropf.bottom - ycrop / a.mYscale;
        yOrigin = 0;
      }

      // mSourcecropf.top = a.mSourcecropf.top - min<double>(
      // (b.mSourcecropf.bottom - a.mYoffset) / a.mYscale, 0);
      mSourcecropf.top =
          max<double>(a.mSourcecropf.bottom +
                          ((a.mYoffset - b.mSourcecropf.bottom) / a.mYscale),
                      a.mSourcecropf.top);
    } else {
      double ycrop = b.mSourcecropf.top - a.mYoffset;
      if (ycrop < 0) {
        // 1st crop is more restrictive
        mSourcecropf.top = a.mSourcecropf.top;
        yOrigin = -ycrop;
      } else {
        // 2nd crop is more restrictive
        mSourcecropf.top = ycrop / a.mYscale + a.mSourcecropf.top;
        yOrigin = 0;
      }

      mSourcecropf.bottom = min<double>(
          (b.mSourcecropf.bottom - a.mYoffset) / a.mYscale + a.mSourcecropf.top,
          a.mSourcecropf.bottom);
    }
  }

  if (b.mTransform & hwcomposer::HWCTransform::kTransform90) {
    // Combine scaling factors appropriately to rotation
    mXscale = a.mYscale * b.mXscale;
    mYscale = a.mXscale * b.mYscale;

    // and deduce display offsets
    mXoffset = b.mXoffset + yOrigin * b.mXscale;
    // ... this mXoffset calculation actually gives offset from RHS. Corrected
    // in the flip
    // ... see comment later.
    mYoffset = b.mYoffset + xOrigin * b.mYscale;
  } else {
    // Combine scaling factors
    mXscale = a.mXscale * b.mXscale;
    mYscale = a.mYscale * b.mYscale;

    // and deduce display offsets
    mXoffset = b.mXoffset + xOrigin * b.mXscale;
    mYoffset = b.mYoffset + yOrigin * b.mYscale;
  }

  // Execute flips
  bool flipH = b.mTransform & hwcomposer::HWCTransform::kReflectX;
  bool flipV = b.mTransform & hwcomposer::HWCTransform::kReflectY;

  // Should have done the flip before the rotation
  if (b.mTransform & hwcomposer::HWCTransform::kTransform90) {
    swap(flipH, flipV);

    // This is necessary because the above calculation for mXoffset actually
    // gives the offset from the RHS.
    flipH = !flipH;
  }

  if (flipH) {
    mXoffset = b.mXoffset + b.DisplayRight() - DisplayRight();
  }

  if (flipV) {
    mYoffset = b.mYoffset + b.DisplayBottom() - DisplayBottom();
  }

  // Overall decrypt state is that of the hardware
  mDecrypt = a.mDecrypt || b.mDecrypt;

  // Take blend state from the input
  mBlending = a.mBlending;
  mHasPixelAlpha = a.mHasPixelAlpha;

  switch (b.mBlending) {
    case hwcomposer::HWCBlending::kBlendingNone:
      mPlaneAlpha = a.mPlaneAlpha;
      break;

    case hwcomposer::HWCBlending::kBlendingCoverage:
      // Only makes sense if a is a HWC buffer
      if (b.GetBuf()->IsCompositionTarget()) {
        HWCERROR(eCheckCompositionBlend,
                 "Invalid blend %s on composition target handle %p",
                 b.GetBlendingStr(strbuf), b.GetBuf()->GetHandle());
        b.Log(ANDROID_LOG_ERROR, "Invalid blend");
      }
    // fall through

    default:
      // PREMULT / NONE

      mPlaneAlpha = a.mPlaneAlpha * b.mPlaneAlpha;
  }

  if (HWCCOND(cond)) {
    Log(ANDROID_LOG_VERBOSE, "  =>");
  }
  HWCLOGD_COND(eLogBuffer,
               "DrmShimTransform::DrmShimTransform(&transform, &transform, "
               "check, str) Created transform@%p",
               this);
}

DrmShimTransform* DrmShimTransform::SetTransform(uint32_t transform) {
  mTransform = transform;
  return this;
}

const char* DrmShimTransform::GetTransformName(uint32_t transform) {
  if (transform < hwcomposer::HWCTransform::kMaxTransform) {
    return mTransformNames[transform];
  } else {
    return "";
  }
}

DrmShimTransform* DrmShimTransform::SetPlaneOrder(uint32_t planeOrder) {
  mZOrder = uint64_t(planeOrder) << MOST_SIGNIFICANT_Z_ORDER_BITS;
  return this;
}

void DrmShimTransform::SetDisplayOffset(int32_t x, int32_t y) {
  mXoffset = double(x);
  mYoffset = double(y);
}

void DrmShimTransform::SetDisplayFrameSize(int32_t w, int32_t h) {
  // Must be called after source crop and transform have been set.
  if (mTransform & hwcomposer::HWCTransform::kTransform90) {
    mXscale = double(w) / (mSourcecropf.bottom - mSourcecropf.top);
    mYscale = double(h) / (mSourcecropf.right - mSourcecropf.left);
  } else {
    mXscale = double(w) / (mSourcecropf.right - mSourcecropf.left);
    mYscale = double(h) / (mSourcecropf.bottom - mSourcecropf.top);
  }
}

void DrmShimTransform::GetEffectiveDisplayFrame(
    hwcomposer::HwcRect<int>& rect) {
  rect.left = int(GetXOffset());
  rect.top = int(GetYOffset());

  if (mTransform & hwcomposer::HWCTransform::kTransform90) {
    rect.right = int(
        rect.left + ((mSourcecropf.bottom - mSourcecropf.top) * mXscale + 0.5));
    rect.bottom = int(
        rect.top + ((mSourcecropf.right - mSourcecropf.left) * mYscale + 0.5));
  } else {
    rect.right = int(
        rect.left + ((mSourcecropf.right - mSourcecropf.left) * mXscale + 0.5));
    rect.bottom = int(
        rect.top + ((mSourcecropf.bottom - mSourcecropf.top) * mYscale + 0.5));
  }
}

// Does the display frame intersect a box (0, 0, width, height)?
bool DrmShimTransform::IsDfIntersecting(int32_t width, int32_t height) {
  hwcomposer::HwcRect<int> rect;
  GetEffectiveDisplayFrame(rect);

  if ((rect.left == rect.right) || (rect.top == rect.bottom)) {
    return false;
  }

  if ((rect.right <= 0) || (rect.bottom <= 0)) {
    return false;
  }

  if ((rect.left > width) || (rect.top > height)) {
    return false;
  }

  return true;
}

void DrmShimTransform::Log(int priority, const char* str) const {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  char strbuf2[HWCVAL_DEFAULT_STRLEN];
  char strbuf3[HWCVAL_DEFAULT_STRLEN];
  HWCLOG(priority,
         "%s@%p %s z=%08x Sourcecropf(l,t,r,b)=(%4.1f,%4.1f,%4.1f,%4.1f) "
         "Offset=(%4.1f,%4.1f) Scale=(%1.3f,%1.3f)%s Tf=%s %s srcs %x",
         str, this, mBuf.get() ? mBuf->IdStr(strbuf) : "buf@0", GetZOrder(),
         mSourcecropf.left, mSourcecropf.top, mSourcecropf.right,
         mSourcecropf.bottom, mXoffset, mYoffset, mXscale, mYscale,
         mDecrypt ? " DECRYPT" : "", GetTransformName(),
         GetBlendingStr(strbuf2), SourcesStr(strbuf3));
}

std::string DrmShimTransform::GetBlendingStr(
    hwcomposer::HWCBlending blending) {
  switch (blending) {
    case hwcomposer::HWCBlending::kBlendingNone:
      return std::string("NONE");

    case hwcomposer::HWCBlending::kBlendingCoverage:
      return std::string("COVERAGE");

    case hwcomposer::HWCBlending::kBlendingPremult:
      return std::string("PREMULT");

    default:
      return std::string("UNKNOWN HWCBlending");
  }
}

const char* DrmShimTransform::GetBlendingStr(char* strbuf) const {
  std::string str(GetBlendingStr(mBlending));

  sprintf(strbuf, "%s %sPXA %f", str.c_str(), mHasPixelAlpha ? "+" : "-",
          double(mPlaneAlpha));

  return strbuf;
}

// Check for differences in the requested and actual transforms
// return false if no more checks are to be carried out on this display
bool DrmShimTransform::Compare(DrmShimTransform& actual, DrmShimTransform& orig,
                               int display, HwcTestCrtc* crtc,
                               uint32_t& cropErrorCount,
                               uint32_t& scaleErrorCount, uint32_t hwcFrame) {
  HwcTestCheckType errorCode;
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  char strbuf2[HWCVAL_DEFAULT_STRLEN];
  char strbuf3[HWCVAL_DEFAULT_STRLEN];

  if ((fabs(mSourcecropf.right - mSourcecropf.left) < HWCVAL_CROP_MARGIN) ||
      (fabs(mSourcecropf.bottom - mSourcecropf.top) < HWCVAL_CROP_MARGIN)) {
    // Should be nothing on screen
    if ((fabs(actual.mSourcecropf.right - actual.mSourcecropf.left) <
         HWCVAL_CROP_MARGIN) ||
        (fabs(actual.mSourcecropf.bottom - actual.mSourcecropf.top) <
         HWCVAL_CROP_MARGIN)) {
      // ... and there isn't
      return true;
    }
    // TODO in what cases would the "else" case be an error?
  }

  if ((fabs(mSourcecropf.left - actual.mSourcecropf.left) >
       HWCVAL_CROP_MARGIN) ||
      (fabs(mSourcecropf.top - actual.mSourcecropf.top) > HWCVAL_CROP_MARGIN) ||
      (fabs(mSourcecropf.right - actual.mSourcecropf.right) >
       HWCVAL_CROP_MARGIN) ||
      (fabs(mSourcecropf.bottom - actual.mSourcecropf.bottom) >
       HWCVAL_CROP_MARGIN)) {
    if (!crtc->ClassifyError(errorCode, eCheckPlaneCrop)) {
      // Don't do any more checks on this display
      return false;
    } else {
      ++cropErrorCount;

      HWCLOGE(
          "  D%d SC: Layer%2d (%6.1f,%6.1f,%6.1f,%6.1f) Scaled "
          "(%6.1f,%6.1f,%6.1f,%6.1f) actual (%6.1f,%6.1f,%6.1f,%6.1f) %s",
          display, GetLayerIndex(), (double)orig.mSourcecropf.left,
          (double)orig.mSourcecropf.top, (double)orig.mSourcecropf.right,
          (double)orig.mSourcecropf.bottom, (double)mSourcecropf.left,
          (double)mSourcecropf.top, (double)mSourcecropf.right,
          (double)mSourcecropf.bottom, (double)actual.mSourcecropf.left,
          (double)actual.mSourcecropf.top, (double)actual.mSourcecropf.right,
          (double)actual.mSourcecropf.bottom, GetBuf()->IdStr(strbuf));
    }
  }

  if (!CompareDf(actual, orig, display, crtc, scaleErrorCount)) {
    // Don't do any more checks on this display
    return false;
  }

  HWCCHECK(eCheckPlaneTransform);
  if (GetTransform() != actual.GetTransform()) {
    HWCERROR(
        eCheckPlaneTransform,
        "Layer %d %s transform expected=%s actual=%s to display %d frame:%d",
        GetLayerIndex(), GetBuf()->IdStr(strbuf), GetTransformName(),
        actual.GetTransformName(), display, hwcFrame);
  }
  HWCCHECK(eCheckPlaneBlending);

  // Special for back layer
  if (mBlending == hwcomposer::HWCBlending::kBlendingNone &&
      (actual.mBlending == hwcomposer::HWCBlending::kBlendingNone ||
       actual.mBlending == hwcomposer::HWCBlending::kBlendingPremult)) {
    return true;
  }

  if ((mLayerIndex == 0 &&
       (mBlending == hwcomposer::HWCBlending::kBlendingNone ||
        mBlending == hwcomposer::HWCBlending::kBlendingPremult) &&
       (actual.mBlending == hwcomposer::HWCBlending::kBlendingNone ||
        actual.mBlending == hwcomposer::HWCBlending::kBlendingPremult)) ||
      (mBlending == actual.mBlending)) {
    // Blending is compatible
  } else {
    // Blending is irrelevant if format does not have pixel alpha
    if (actual.mHasPixelAlpha) {
      HWCERROR(
          eCheckPlaneBlending,
          "Layer %d %s incompatible blending: expected %s actual %s (frame:%d)",
          GetLayerIndex(), GetBuf()->IdStr(strbuf), GetBlendingStr(strbuf2),
          actual.GetBlendingStr(strbuf3), hwcFrame);
      return true;
    }
  }

  HWCCHECK(eCheckPixelAlpha);
  if ((mHasPixelAlpha && !actual.mHasPixelAlpha) && (GetLayerIndex() > 0)) {
    if ((mBlending == hwcomposer::HWCBlending::kBlendingNone) &&
        !actual.mHasPixelAlpha) {
      // HWC can represent no blending by remapping an RGBA buffer as RGBX
    } else {
      HWCERROR(eCheckPixelAlpha,
               "Layer %d %s per-pixel alpha is not being rendered: expected %s "
               "actual %s (frame: %d)",
               GetLayerIndex(), GetBuf()->IdStr(strbuf),
               GetBlendingStr(strbuf2), actual.GetBlendingStr(strbuf3),
               hwcFrame);
    }
  }

  HWCCHECK(eCheckPlaneAlpha);
  if (mPlaneAlpha != actual.mPlaneAlpha) {
    HWCERROR(eCheckPlaneAlpha,
             "Layer %d %s plane alpha rendered incorrectly: expected %f actual "
             "%f (frame:%d)",
             GetLayerIndex(), GetBuf()->IdStr(strbuf), mPlaneAlpha,
             actual.mPlaneAlpha, hwcFrame);
  }

  return true;
}

// Check for differences in the requested and actual display frames
// return false if no more checks are to be carried out on this display
bool DrmShimTransform::CompareDf(DrmShimTransform& actual,
                                 DrmShimTransform& orig, int display,
                                 HwcTestCrtc* crtc, uint32_t& scaleErrorCount) {
  HwcTestCheckType errorCode;
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  hwcomposer::HwcRect<int> requestedDisplayFrame;
  GetEffectiveDisplayFrame(requestedDisplayFrame);

  hwcomposer::HwcRect<int> effectiveDisplayFrame;
  actual.GetEffectiveDisplayFrame(effectiveDisplayFrame);

  if ((abs(requestedDisplayFrame.left - effectiveDisplayFrame.left) >
       HWCVAL_DISPLAYFRAME_SIZE_MARGIN) ||
      (abs(requestedDisplayFrame.top - effectiveDisplayFrame.top) >
       HWCVAL_DISPLAYFRAME_SIZE_MARGIN) ||
      (abs(requestedDisplayFrame.right - effectiveDisplayFrame.right) >
       HWCVAL_DISPLAYFRAME_SIZE_MARGIN) ||
      (abs(requestedDisplayFrame.bottom - effectiveDisplayFrame.bottom) >
       HWCVAL_DISPLAYFRAME_SIZE_MARGIN)) {
    if (!crtc->ClassifyError(errorCode, eCheckPlaneScale)) {
      // Don't do any more checks on this display
      return false;
    } else {
      hwcomposer::HwcRect<int> origDisplayFrame;
      orig.GetEffectiveDisplayFrame(origDisplayFrame);

      ++scaleErrorCount;
      HWCLOGE(
          "  D%d DF: Layer%2d (%6d,%6d,%6d,%6d) Scaled (%6d,%6d,%6d,%6d) "
          "actual (%6d,%6d,%6d,%6d) %s",
          display, GetLayerIndex(), origDisplayFrame.left, origDisplayFrame.top,
          origDisplayFrame.right, origDisplayFrame.bottom,
          requestedDisplayFrame.left, requestedDisplayFrame.top,
          requestedDisplayFrame.right, requestedDisplayFrame.bottom,
          effectiveDisplayFrame.left, effectiveDisplayFrame.top,
          effectiveDisplayFrame.right, effectiveDisplayFrame.bottom,
          GetBuf()->IdStr(strbuf));
    }
  }

  return true;
}

// Maintains constant aspect ratio and avoids cropping, with the source
// centred within the destination area
DrmShimFixedAspectRatioTransform::DrmShimFixedAspectRatioTransform(
    uint32_t sw, uint32_t sh, uint32_t dw, uint32_t dh) {
  SetSourceCrop(0.0, 0.0, double(sw), double(sh));

  double xscale = double(dw) / double(sw);
  double yscale = double(dh) / double(sh);
  double scale;

  if (xscale > yscale) {
    scale = yscale;
    mXoffset = (dw - (scale * sw)) / 2;
    mYoffset = 0;
  } else {
    scale = xscale;
    mXoffset = 0;
    mYoffset = (dh - (scale * sh)) / 2;
  }

  mXscale = mYscale = scale;
}

// Sort should be into Z-order sequence
bool operator<(const DrmShimTransform& lhs, const DrmShimTransform& rhs) {
  if (lhs.GetZOrder() == rhs.GetZOrder()) {
    HWCERROR(eCheckInternalZOrder,
             "Warning: identical Z-orders in transform comparison (%p %p), you "
             "may get items overwritten in sorted vector",
             &lhs, &rhs);
    lhs.Log(ANDROID_LOG_ERROR, "lhs");
    rhs.Log(ANDROID_LOG_ERROR, "rhs");
  }
  return (lhs.GetZOrder() < rhs.GetZOrder());
}

uint32_t DrmShimTransform::Inverse(uint32_t transform) {
  switch (transform) {
    case hwcomposer::HWCTransform::kTransform90:
      return hwcomposer::HWCTransform::kTransform270;

    case hwcomposer::HWCTransform::kTransform270:
      return hwcomposer::HWCTransform::kTransform90;

    default:
      return transform;
  }
}

// Translate a rect from frame of destination rect back to source
hwcomposer::HwcRect<int> InverseTransformRect(hwcomposer::HwcRect<int>& rect,
                                              const Hwcval::ValLayer& layer) {
  // Dummy buffer pointer
  std::shared_ptr<DrmShimBuffer> buf;
  const hwcomposer::HwcRect<int>& dst = layer.GetDisplayFrame();

  DrmShimTransform layerTransform(buf, 0, layer);
  DrmShimTransform inverseLayerTransform = layerTransform.Inverse();

  // dest rect, translated into its own frame of reference
  // so starting from (0,0).
  hwcomposer::HwcRect<float> tdfZeroBased;
  tdfZeroBased.left = 0;
  tdfZeroBased.top = 0;
  tdfZeroBased.right = dst.right - dst.left;
  tdfZeroBased.bottom = dst.bottom - dst.top;

  // subject rect, translated into the dest frame of reference.
  hwcomposer::HwcRect<float> vdfTdfBased;
  vdfTdfBased.left = rect.left - dst.left;
  vdfTdfBased.top = rect.top - dst.top;
  vdfTdfBased.right = rect.right - dst.left;
  vdfTdfBased.bottom = rect.bottom - dst.top;

  // The subject rect in the dest frame of reference,
  // expressed as a transform so we can combine it.
  hwcval_layer_t videoDfLayer;
  videoDfLayer.sourceCropf.left = (float)(rect.left);
  videoDfLayer.sourceCropf.right = (float)(rect.right);
  videoDfLayer.sourceCropf.top = (float)(rect.top);
  videoDfLayer.sourceCropf.bottom = (float)(rect.bottom);

  videoDfLayer.displayFrame.left = rect.left;
  videoDfLayer.displayFrame.right = rect.right;
  videoDfLayer.displayFrame.top = rect.top;
  videoDfLayer.displayFrame.bottom = rect.bottom;

  videoDfLayer.transform = 0;
  DrmShimTransform videoDfTransform(buf, 0, &videoDfLayer);

  // The subject rect transformed into the source frame of reference
  // (as a transform...)
  DrmShimTransform videoDfInLayerSourceFrame(
      videoDfTransform, inverseLayerTransform, eLogVideo,
      "Video displayframe transformed into frame of reference of source layer");

  // ... and as a rect.
  hwcomposer::HwcRect<int> result;
  result.left = videoDfInLayerSourceFrame.GetXOffset();
  result.right = videoDfInLayerSourceFrame.DisplayRight();
  result.top = videoDfInLayerSourceFrame.GetYOffset();
  result.bottom = videoDfInLayerSourceFrame.DisplayBottom();

  return result;
}

bool DrmShimTransform::IsFromSfComp() {
  return ((mSources &
           (1 << static_cast<int>(Hwcval::BufferSourceType::SfComp))) != 0);
}

const char* DrmShimTransform::SourcesStr(char* strbuf) const {
  return SourcesStr(mSources, strbuf);
}

const char* DrmShimTransform::SourcesStr(uint32_t sources, char* strbuf) {
  char* p = strbuf;
  *strbuf = '\0';

  if (sources & (1 << static_cast<int>(Hwcval::BufferSourceType::SfComp))) {
    p += sprintf(p, " Sf");
  }
  if (sources &
      (1 << static_cast<int>(Hwcval::BufferSourceType::PartitionedComposer))) {
    p += sprintf(p, " PC");
  }

  if (*strbuf) {
    return strbuf + 1;
  } else {
    return strbuf;
  }
}

DrmShimCroppedLayerTransform::DrmShimCroppedLayerTransform(
    std::shared_ptr<DrmShimBuffer>& buf, uint32_t layerIx,
    const Hwcval::ValLayer& layer, HwcTestCrtc* crtc) {
  // Get the requested transform
  DrmShimTransform layerTransform(buf, layerIx, layer);

  // Find bounding box of visible regions
  if (layer.GetVisibleRegion().NumRects() > 0) {
    hwcomposer::HwcRect<int> bounds = layer.GetVisibleRegionBounds();
    const hwcomposer::HwcRect<int>& df = layer.GetDisplayFrame();

    if ((bounds.left > df.left) || (bounds.right < df.right) ||
        (bounds.top > df.top) || (bounds.bottom < df.bottom)) {
      // We have now concluded that the bounding box of the visible regions
      // is a more limiting constraint than the source crop.
      //
      // So, work out the effective source crop by converting the display frame
      // using an inverse transform.
      //
      // TODO: Consider if this needs to be made more efficient - we are doing
      // quite a lot of unnecessary work here.
      ValLayer inverseLayer;
      inverseLayer.SetSourceCrop(hwcomposer::HwcRect<float>(df));
      inverseLayer.SetDisplayFrame(layer.GetSourceCrop());
      inverseLayer.SetTransformId(
          DrmShimTransform::Inverse(layer.GetTransformId()));
      inverseLayer.SetBlendingType(hwcomposer::HWCBlending::kBlendingNone);
      DrmShimTransform inverseLayerTransform(buf, 0, inverseLayer);

      ValLayer boundsLayer;
      boundsLayer.SetSourceCrop(hwcomposer::HwcRect<float>(
          {0, 0, float(crtc->GetWidth()), float(crtc->GetHeight())}));
      boundsLayer.SetDisplayFrame(bounds);
      boundsLayer.SetTransformId(0);
      boundsLayer.SetBlendingType(hwcomposer::HWCBlending::kBlendingNone);
      DrmShimTransform boundingTransform(buf, 0, boundsLayer);

      DrmShimTransform derived(boundingTransform, inverseLayerTransform,
                               eLogCombinedTransform,
                               "Visible regions: bounding box reverse "
                               "transformed into source frame of reference");
      hwcomposer::HwcRect<int> boundsInSource;
      derived.GetEffectiveDisplayFrame(boundsInSource);
      layerTransform.SetSourceCrop(hwcomposer::HwcRect<float>(boundsInSource));
    }
  }

  // Apply the portal of the physical screen
  DrmShimTransform croppedLayerTransform(
      layerTransform, crtc->GetScaleTransform(), eLogCroppedTransform,
      "Trim [and scale if appropriate] input layer to physical screen "
      "co-ordinates");
  croppedLayerTransform.SetLayerIndex(layerIx);
  *(static_cast<DrmShimTransform*>(this)) = croppedLayerTransform;
}
