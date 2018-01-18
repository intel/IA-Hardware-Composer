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

#include <hwcutils.h>

#include "compositionregion.h"
#include "nativegpuresource.h"
#include "overlaybuffer.h"
#include "overlaylayer.h"

namespace hwcomposer {

void RenderState::ConstructState(std::vector<OverlayLayer> &layers,
                                 const CompositionRegion &region,
                                 const HwcRect<int> &damage, bool clear_surface,
                                 bool uses_display_up_scaling,
                                 uint32_t downscaling_factor) {
  float bounds[4];
  std::copy_n(region.frame.bounds, 4, bounds);
  x_ = bounds[0];
  y_ = bounds[1];
  width_ = (bounds[2] - bounds[0]);
  height_ = (bounds[3] - bounds[1]);

  ETRACE("render state scaler :%d, damage:%d %d %d %d, bounds:%f %f %f %f\n", downscaling_factor,
  damage.left, damage.top, damage.right, damage.bottom, bounds[0], bounds[1], bounds[2], bounds[3]);

  if (!clear_surface) {
    // If viewport and layer doesn't interact we can avoid re-rendering
    // this state.
    if (AnalyseOverlap(region.frame, damage) == kOutside) {
      return;
    }

    uint32_t top = damage.top;
    uint32_t left = damage.left;
    uint32_t scissor_right = std::min(damage.right, (int)bounds[2]);
    uint32_t scissor_bottom = std::min(damage.bottom, (int)bounds[3]);

    scissor_x_ = std::max(x_, left);
    scissor_y_ = std::max(y_, top);
    scissor_width_ = (scissor_right - scissor_x_);
    scissor_height_ = (scissor_bottom - scissor_y_);
  } else {
    scissor_x_ = x_;
    scissor_y_ = y_;
    scissor_width_ = width_;
    scissor_height_ = height_;
  }

  if(downscaling_factor > 1)
  {
    x_ /= downscaling_factor;
    y_ /= downscaling_factor;
    width_ /= downscaling_factor;
    height_ /= downscaling_factor;
    scissor_x_ /= downscaling_factor;
    scissor_y_ /= downscaling_factor;
    scissor_width_ /= downscaling_factor;
    scissor_height_ /= downscaling_factor;
  }

  const std::vector<size_t> &source = region.source_layers;
  for (size_t texture_index : source) {
    OverlayLayer &layer = layers.at(texture_index);
    if (!clear_surface) {
      // If viewport and layer doesn't interact we can avoid re-rendering
      // this state.
      const HwcRect<int> &layer_damage = layer.GetDisplayFrame();
      if (AnalyseOverlap(layer_damage, damage) == kOutside) {
	continue;
      }
    }

    layer_state_.emplace_back();
    RenderState::LayerState &src = layer_state_.back();
    src.layer_index_ = texture_index;
    bool swap_xy = false;
    bool flip_xy[2] = {false, false};
    switch (layer.GetTransform()) {
      case HWCTransform::kTransform180: {
        swap_xy = false;
        flip_xy[0] = true;
        flip_xy[1] = true;
        break;
      }
      case HWCTransform::kTransform270: {
        swap_xy = true;
        flip_xy[0] = true;
        flip_xy[1] = false;
        break;
      }
      case HWCTransform::kTransform90: {
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

    HwcRect<float> display_rect;
    float display_size[2];

    if (uses_display_up_scaling) {
      display_rect = layer.GetSourceCrop();
      display_size[0] = static_cast<float>(layer.GetSourceCropWidth());
      display_size[1] = static_cast<float>(layer.GetSourceCropHeight());
    } else {
      const HwcRect<int> &display_Rect = layer.GetDisplayFrame();
      display_rect.left = static_cast<float>(display_Rect.left);
      display_rect.right = static_cast<float>(display_Rect.right);
      display_rect.top = static_cast<float>(display_Rect.top);
      display_rect.bottom = static_cast<float>(display_Rect.bottom);
      if (0) {
        display_rect.right =
            display_rect.right -
            ((display_rect.right - display_rect.left) / downscaling_factor);

	display_rect.bottom =
	    display_rect.bottom - ((display_rect.bottom - display_rect.top) /
				 downscaling_factor);

        display_size[0] = display_rect.right - display_rect.left;
	display_size[1] = display_rect.bottom - display_rect.top;
      } else {
        display_size[0] = static_cast<float>(layer.GetDisplayFrameWidth());
	display_size[1] = static_cast<float>(layer.GetDisplayFrameHeight());
      }
    }

    float tex_width = static_cast<float>(layer.GetBuffer()->GetWidth());
    float tex_height = static_cast<float>(layer.GetBuffer()->GetHeight());
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
