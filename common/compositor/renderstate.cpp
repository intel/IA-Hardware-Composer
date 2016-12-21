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

#include "renderstate.h"

#include "compositionregion.h"
#include "nativegpuresource.h"
#include "overlaybuffer.h"
#include "overlaylayer.h"

namespace hwcomposer {

void RenderState::ConstructState(const std::vector<OverlayLayer> &layers,
                                 const CompositionRegion &region,
                                 const NativeGpuResource *resources) {
  float bounds[4];
  std::copy_n(region.frame.bounds, 4, bounds);
  x_ = bounds[0];
  y_ = bounds[1];
  width_ = bounds[2] - bounds[0];
  height_ = bounds[3] - bounds[1];

  for (size_t texture_index : region.source_layers) {
    const OverlayLayer &layer = layers.at(texture_index);
    layer_state_.emplace_back();
    RenderState::LayerState &src = layer_state_.back();
    src.handle_ = resources->GetResourceHandle(texture_index);

    bool swap_xy = false;
    bool flip_xy[2] = {false, false};
    switch (layer.GetTransform()) {
      case HWCTransform::kRotate180: {
        swap_xy = false;
        flip_xy[0] = true;
        flip_xy[1] = true;
        break;
      }
      case HWCTransform::kRotate270: {
        swap_xy = true;
        flip_xy[0] = true;
        flip_xy[1] = false;
        break;
      }
      case HWCTransform::kRotate90: {
        swap_xy = true;
        if (layer.GetTransform() & HWCTransform::kReflectX) {
          flip_xy[0] = true;
          flip_xy[1] = true;
        } else if (layer.GetTransform() & HWCTransform::kReflectY) {
          flip_xy[0] = false;
          flip_xy[1] = false;
        } else {
          flip_xy[0] = false;
          flip_xy[1] = true;
        }
	break;
      }
      default: {
        if (layer.GetTransform() & HWCTransform::kReflectX)
          flip_xy[0] = true;
        if (layer.GetTransform() & HWCTransform::kReflectY)
          flip_xy[1] = true;
      }
    }

    if (swap_xy)
      std::copy_n(&TransformMatrices[4], 4, src.texture_matrix_);
    else
      std::copy_n(&TransformMatrices[0], 4, src.texture_matrix_);

    HwcRect<float> display_rect(layer.GetDisplayFrame());
    float display_size[2] = {(float)layer.GetDisplayFrameWidth(),
			     (float)layer.GetDisplayFrameHeight()};
    float tex_width = layer.GetBuffer()->GetWidth();
    float tex_height = layer.GetBuffer()->GetHeight();
    const HwcRect<float> &source_crop = layer.GetSourceCrop();

    HwcRect<float> crop_rect(
	source_crop.left / tex_width, source_crop.top / tex_height,
	source_crop.right / tex_width, source_crop.bottom / tex_height);

    float crop_size[2] = {crop_rect.bounds[2] - crop_rect.bounds[0],
			  crop_rect.bounds[3] - crop_rect.bounds[1]};

    for (int j = 0; j < 4; j++) {
      int b = j ^ (swap_xy ? 1 : 0);
      float bound_percent =
          (bounds[b] - display_rect.bounds[b % 2]) / display_size[b % 2];
      if (flip_xy[j % 2]) {
        src.crop_bounds_[j] =
            crop_rect.bounds[j % 2 + 2] - bound_percent * crop_size[j % 2];
      } else {
        src.crop_bounds_[j] =
            crop_rect.bounds[j % 2] + bound_percent * crop_size[j % 2];
      }
    }

    if (layer.GetBlending() == HWCBlending::kBlendingNone) {
      src.alpha_ = src.premult_ = 1.0f;
      break;
    }

    src.alpha_ = layer.GetAlpha() / 255.0f;
    src.premult_ =
        (layer.GetBlending() == HWCBlending::kBlendingPremult) ? 1.0f : 0.0f;
  }
}

}  // namespace hwcomposer
