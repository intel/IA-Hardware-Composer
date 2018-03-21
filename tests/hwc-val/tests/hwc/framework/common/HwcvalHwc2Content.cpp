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

#include "HwcvalHwc2Content.h"
#include "DrmShimBuffer.h"
#include "HwcTestState.h"

#include <hardware/hwcomposer2.h>

Hwcval::CompositionType Hwcval::Hwc2CompositionTypeToHwcval(
    uint32_t compositionType) {
  switch (compositionType) {
    case HWC2_COMPOSITION_CLIENT:
      return Hwcval::CompositionType::SF;
    case HWC2_COMPOSITION_DEVICE:
      return Hwcval::CompositionType::HWC;
    default:
      return Hwcval::CompositionType::UNKNOWN;
  }
}

hwcomposer::HWCBlending Hwcval::Hwc2BlendingTypeToHwcval(
    uint32_t blendingType) {
  switch (blendingType) {
    case HWC_BLENDING_NONE:
      return hwcomposer::HWCBlending::kBlendingNone;
    case HWC_BLENDING_PREMULT:
      return hwcomposer::HWCBlending::kBlendingPremult;
    case HWC_BLENDING_COVERAGE:
      return hwcomposer::HWCBlending::kBlendingCoverage;
    default:
      return hwcomposer::HWCBlending::kBlendingNone;
  }
}

Hwcval::Hwc2Layer::Hwc2Layer(const hwcval_layer_t *sfLayer,
                             std::shared_ptr<DrmShimBuffer> &buf) {
  mCompositionType = Hwc2CompositionTypeToHwcval(sfLayer->compositionType);
  mHints = sfLayer->hints;
  mFlags = sfLayer->flags;
  mBuf = buf;

  // HWC1 transforms have the same values as internal Hwcval transforms
  mTransform = sfLayer->transform;

  mBlending = Hwc2BlendingTypeToHwcval(sfLayer->blending);
  mSourceCropf.left = sfLayer->sourceCropf.left;
  mSourceCropf.right = sfLayer->sourceCropf.right;
  mSourceCropf.top = sfLayer->sourceCropf.top;
  mSourceCropf.bottom = sfLayer->sourceCropf.bottom;

  mDisplayFrame.left = sfLayer->displayFrame.left;
  mDisplayFrame.right = sfLayer->displayFrame.right;
  mDisplayFrame.top = sfLayer->displayFrame.top;
  mDisplayFrame.bottom = sfLayer->displayFrame.bottom;

  mPlaneAlpha = float(sfLayer->planeAlpha) / HWCVAL_ALPHA_FLOAT_TO_INT;

  if (buf.get()) {
    // Copy the visible rects to separate area and provide link from layer
    mVisibleRegionScreen = ValRegion(sfLayer->visibleRegionScreen);
  }
}

Hwcval::Hwc2LayerList::Hwc2LayerList(
    const hwcval_display_contents_t *sfDisplay) {
  mRetireFenceFd = 0;  // Correct value won't be known until exit of OnSet

  if (sfDisplay) {
    mOutbuf =
	sfDisplay->outbuf;  // This will change when we do virtual displays
    // mOutbufAcquireFenceFd = sfDisplay->outbufAcquireFenceFd;
    // mFlags = sfDisplay->flags;
    mNumLayers = sfDisplay->numHwLayers;
  } else {
    mOutbufAcquireFenceFd = 0;
    mFlags = 0;
    mNumLayers = 0;
  }
}

uint32_t Hwcval::HwcvalBlendingTypeToHwc2(
    hwcomposer::HWCBlending blendingType) {
  switch (blendingType) {
    case hwcomposer::HWCBlending::kBlendingNone:
      return HWC_BLENDING_NONE;
    case hwcomposer::HWCBlending::kBlendingPremult:
      return HWC_BLENDING_PREMULT;
    case hwcomposer::HWCBlending::kBlendingCoverage:
      return HWC_BLENDING_COVERAGE;
    default:
      return HWC_BLENDING_NONE;
  }
}

// Convert internal layer format back to hwcval_layer_t.
void Hwcval::HwcvalLayerToHwc2(const char *str, uint32_t ix,
                               hwcval_layer_t &out, Hwcval::ValLayer &in,
                               hwc_rect_t *pRect, uint32_t &rectsRemaining) {
  hwc_frect_t sourceCropf;
  const hwcomposer::HwcRect<float> &crop = in.GetSourceCrop();
  sourceCropf.left = crop.left;
  sourceCropf.top = crop.top;
  sourceCropf.right = crop.right;
  sourceCropf.bottom = crop.bottom;

  hwc_rect_t displayFrame;
  const hwcomposer::HwcRect<int> &frame = in.GetDisplayFrame();
  displayFrame.left = frame.left;
  displayFrame.top = frame.top;
  displayFrame.right = frame.right;
  displayFrame.bottom = frame.bottom;

  HWCLOGV("%s %d handle %p src (%f,%f,%f,%f) dst (%d,%d,%d,%d) alpha %d", str,
          ix, in.GetHandle(), (double)sourceCropf.left, (double)sourceCropf.top,
          (double)sourceCropf.right, (double)sourceCropf.bottom,
          displayFrame.left, displayFrame.top, displayFrame.right,
          displayFrame.bottom, in.GetPlaneAlpha());
  out.gralloc_handle = in.GetHandle();
  out.sourceCropf = sourceCropf;
  out.displayFrame = displayFrame;
  out.transform = in.GetTransformId();
  out.blending = Hwcval::HwcvalBlendingTypeToHwc2(in.GetBlendingType());

  // Convert plane alpha from internal (floating point) form to the integer form
  // expected by the composer.
  //
  // I'm using 0.25 rather than the usual 0.5 because we know that the original
  // data was in integer form
  // and I don't want to end up incrementing the integer result.
  out.planeAlpha = (in.GetPlaneAlpha() * HWCVAL_ALPHA_FLOAT_TO_INT) + 0.25;

  in.GetVisibleRegion().GetHwcRects(out.visibleRegionScreen, pRect,
                                    rectsRemaining);

  // Composition type is left to the caller
}
