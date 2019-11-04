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

#include "overlaylayer.h"

#include <cmath>

#include <drm_mode.h>
#include <hwctrace.h>
#include <map>
#include <vector>

#include "hwcutils.h"

#include "nativebufferhandler.h"
#include "resourcemanager.h"

namespace hwcomposer {

OverlayLayer::ImportedBuffer::~ImportedBuffer() {
  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }
}

OverlayLayer::ImportedBuffer::ImportedBuffer(
    std::shared_ptr<OverlayBuffer>& buffer, int32_t acquire_fence)
    : acquire_fence_(acquire_fence) {
  buffer_ = buffer;
}

void OverlayLayer::SetAcquireFence(int32_t acquire_fence) {
  // Release any existing fence.
  if (imported_buffer_.get()) {
    if (imported_buffer_->acquire_fence_ > 0) {
      close(imported_buffer_->acquire_fence_);
    }

    imported_buffer_->acquire_fence_ = acquire_fence;
  }
}

int32_t OverlayLayer::GetAcquireFence() const {
  if (imported_buffer_.get()) {
    return imported_buffer_->acquire_fence_;
  } else
    return -1;
}

int32_t OverlayLayer::ReleaseAcquireFence() const {
  if (imported_buffer_.get()) {
    int32_t fence = imported_buffer_->acquire_fence_;
    imported_buffer_->acquire_fence_ = -1;
    return fence;
  } else {
    return -1;
  }
}

OverlayBuffer* OverlayLayer::GetBuffer() const {
  if (imported_buffer_.get()) {
    if (imported_buffer_->buffer_.get() == NULL)
      ETRACE("hwc layer get NullBuffer");

    return imported_buffer_->buffer_.get();
  } else {
    return NULL;
  }
}

std::shared_ptr<OverlayBuffer>& OverlayLayer::GetSharedBuffer() const {
  return imported_buffer_->buffer_;
}

void OverlayLayer::SetBuffer(HWCNativeHandle handle, int32_t acquire_fence,
                             ResourceManager* resource_manager,
                             bool register_buffer) {
  std::shared_ptr<OverlayBuffer> buffer(NULL);

  uint32_t id;

  if (resource_manager && register_buffer) {
    uint32_t gpu_fd = resource_manager->GetNativeBufferHandler()->GetFd();
    id = GetNativeBuffer(gpu_fd, handle);
    buffer = resource_manager->FindCachedBuffer(id);
  }

  if (buffer == NULL) {
    buffer = OverlayBuffer::CreateOverlayBuffer();
    buffer->InitializeFromNativeHandle(handle, resource_manager);
    if (resource_manager && register_buffer) {
      resource_manager->RegisterBuffer(id, buffer);
    }
  } else {
    buffer->SetOriginalHandle(handle);
    // need to update interlace info since interlace info is update frame by
    // frame
    if (buffer->GetUsage() == kLayerVideo)
      buffer->SetInterlace(
          resource_manager->GetNativeBufferHandler()->GetInterlace(handle));
  }

  buffer->SetDataSpace(dataspace_);

  imported_buffer_.reset(new ImportedBuffer(buffer, acquire_fence));
  ValidateForOverlayUsage();
}

void OverlayLayer::SetBlending(HWCBlending blending) {
  blending_ = blending;
}

void OverlayLayer::SetSourceCrop(const HwcRect<float>& source_crop) {
  source_crop_width_ = static_cast<int>(ceilf(source_crop.right) -
                                        static_cast<int>(source_crop.left));
  source_crop_height_ = static_cast<int>(ceilf(source_crop.bottom) -
                                         static_cast<int>(source_crop.top));
  source_crop_ = source_crop;
}

void OverlayLayer::SetDisplayFrame(const HwcRect<int>& display_frame) {
  display_frame_width_ = display_frame.right - display_frame.left;
  display_frame_height_ = display_frame.bottom - display_frame.top;
  display_frame_ = display_frame;
}

void OverlayLayer::SetTransform(uint32_t transform) {
  transform_ = transform;
  merged_transform_ = transform;
}

void OverlayLayer::ValidateTransform(uint32_t transform,
                                     uint32_t display_transform) {
  std::map<int, int> tmap = {{kIdentity, 0},
                             {kTransform90, 1},
                             {kTransform180, 2},
                             {kTransform270, 3}};
  std::vector<int> inv_tmap = {kIdentity, kTransform90, kTransform180,
                               kTransform270};

  int mdisplay_transform = display_transform;
  int mtransform =
      transform & (kIdentity | kTransform90 | kTransform180 | kTransform270);

  if (tmap.find(mtransform) != tmap.end()) {
    mtransform = tmap[mtransform];
  } else {
    // reaching here indicates that transform is
    // is an OR of multiple values
    // Assign Identity in this case
    mtransform = kIdentity;
  }

  if (tmap.find(mdisplay_transform) != tmap.end()) {
    mdisplay_transform = tmap[mdisplay_transform];
  } else {
    mdisplay_transform = kIdentity;
  }

  // The elements {0, 1, 2, 3} form a circulant matrix under mod 4 arithmetic
  mtransform = (mtransform + mdisplay_transform) % 4;
  mtransform = inv_tmap[mtransform];
  merged_transform_ = mtransform;

  if (merged_transform_ & kTransform90) {
    if (transform & kReflectX)
      merged_transform_ |= kReflectX;

    if (transform & kReflectY)
      merged_transform_ |= kReflectY;
  }
}

void OverlayLayer::TransformDamage(HwcLayer* layer, uint32_t max_height,
                                   uint32_t max_width) {
  const HwcRect<int>& surface_damage = layer->GetLayerDamage();
  if (surface_damage.empty()) {
    surface_damage_ = surface_damage;
    return;
  }
  HwcRect<int> translated_damage = TranslateRect(surface_damage, 0, 0);
#ifdef RECT_DAMAGE_TRACING
  IRECTDAMAGETRACE("Calculating Overlaylayer Damage for layer[%d]", z_order_);
  IRECTDAMAGETRACE("max_width: %d, max_height:%d", max_width, max_height);
  IRECTDAMAGETRACE("Original Surface_damage (LTWH): %d, %d, %d, %d",
                   surface_damage.left, surface_damage.top,
                   (surface_damage.right - surface_damage.left),
                   (surface_damage.bottom - surface_damage.top));
  IRECTDAMAGETRACE("translated_damage (LTWH): %d, %d, %d, %d",
                   translated_damage.left, translated_damage.top,
                   (translated_damage.right - translated_damage.left),
                   (translated_damage.bottom - translated_damage.top));
  IRECTDAMAGETRACE("source_crop_ (LTWH): %f, %f, %f, %f", source_crop_.left,
                   source_crop_.top, (source_crop_.right - source_crop_.left),
                   (source_crop_.bottom - source_crop_.top));
  IRECTDAMAGETRACE("display_frame_ (LTWH): %d, %d, %d, %d", display_frame_.left,
                   display_frame_.top,
                   (display_frame_.right - display_frame_.left),
                   (display_frame_.bottom - display_frame_.top));
#endif
  float ratio_w_h = max_width * 1.0 / max_height;
  float ratio_h_w = max_height * 1.0 / max_width;

  int ox = 0, oy = 0;

  if (merged_transform_ == kTransform270) {
    oy = max_height;
    surface_damage_.left = translated_damage.top * ratio_w_h + 0.5;
    surface_damage_.top = oy - translated_damage.right * ratio_h_w + 0.5;
    surface_damage_.right = translated_damage.bottom * ratio_w_h + 0.5;
    surface_damage_.bottom = oy - translated_damage.left * ratio_h_w + 0.5;
  } else if (merged_transform_ == kTransform180) {
    ox = max_width;
    oy = max_height;
    surface_damage_.left = ox - translated_damage.right;
    surface_damage_.top = oy - translated_damage.bottom;
    surface_damage_.right = ox - translated_damage.left;
    surface_damage_.bottom = oy - translated_damage.top;
  } else if (merged_transform_ & hwcomposer::HWCTransform::kTransform90) {
    if (merged_transform_ & kReflectX) {
      surface_damage_.left = translated_damage.top * ratio_w_h + 0.5;
      surface_damage_.top = translated_damage.left * ratio_h_w + 0.5;
      surface_damage_.right = translated_damage.bottom * ratio_w_h + 0.5;
      surface_damage_.bottom = translated_damage.right * ratio_h_w + 0.5;
    } else if (merged_transform_ & kReflectY) {
      ox = max_width;
      oy = max_height;
      surface_damage_.left = ox - (translated_damage.bottom * ratio_w_h + 0.5);
      surface_damage_.top = oy - (translated_damage.right * ratio_h_w + 0.5);
      surface_damage_.right = ox - (translated_damage.top * ratio_w_h + 0.5);
      surface_damage_.bottom = oy - (translated_damage.left * ratio_h_w + 0.5);
    } else {
      ox = max_width;
      surface_damage_.left = ox - translated_damage.bottom * ratio_w_h + 0.5;
      surface_damage_.top = translated_damage.left * ratio_h_w + 0.5;
      surface_damage_.right = ox - translated_damage.top * ratio_w_h + 0.5;
      surface_damage_.bottom = translated_damage.right * ratio_h_w + 0.5;
    }
  } else if (merged_transform_ == 0) {
    surface_damage_.left = translated_damage.left;
    surface_damage_.top = translated_damage.top;
    surface_damage_.right = translated_damage.right;
    surface_damage_.bottom = translated_damage.bottom;
  }
#ifdef RECT_DAMAGE_TRACING
  IRECTDAMAGETRACE("Surface_damage (LTWH): %d, %d, %d, %d",
                   surface_damage_.left, surface_damage_.top,
                   (surface_damage_.right - surface_damage_.left),
                   (surface_damage_.bottom - surface_damage_.top));
#endif
}

void OverlayLayer::InitializeState(HwcLayer* layer,
                                   ResourceManager* resource_manager,
                                   OverlayLayer* previous_layer,
                                   uint32_t z_order, uint32_t layer_index,
                                   uint32_t max_height, uint32_t max_width,
                                   uint32_t rotation, bool handle_constraints) {
  transform_ = layer->GetTransform();
  plane_transform_ = rotation;
  if (rotation != kRotateNone) {
    ValidateTransform(layer->GetTransform(), rotation);
  } else {
    merged_transform_ = transform_;
  }
#ifdef RECT_DAMAGE_TRACING
  IRECTDAMAGETRACE("validated plane_transform_: %d", plane_transform_);
#endif

  alpha_ = layer->GetAlpha();
  layer_index_ = layer_index;
  z_order_ = z_order;
  source_crop_width_ = layer->GetSourceCropWidth();
  source_crop_height_ = layer->GetSourceCropHeight();
  source_crop_ = layer->GetSourceCrop();
  dataspace_ = layer->GetDataSpace();
  blending_ = layer->GetBlending();
  solid_color_ = layer->GetSolidColor();
  TransformDamage(layer, max_height, max_width);

  if (previous_layer && layer->HasZorderChanged()) {
    if (previous_layer->actual_composition_ == kGpu) {
      CalculateRect(previous_layer->display_frame_, surface_damage_);
      bool force_partial_clear = true;
      // We can skip Clear in case display frame, transforms are same.
      if (previous_layer->display_frame_ == display_frame_ &&
          transform_ == previous_layer->transform_ &&
          plane_transform_ == previous_layer->plane_transform_) {
        force_partial_clear = false;
      }

      if (force_partial_clear) {
        state_ |= kForcePartialClear;
      }
    } else {
      state_ |= kNeedsReValidation;
    }
  }

  if (layer->GetNativeHandle()) {
    SetBuffer(layer->GetNativeHandle(), layer->GetAcquireFence(),
              resource_manager, true);
  } else if (Composition_SolidColor == layer->GetLayerCompositionType()) {
    type_ = kLayerSolidColor;
    source_crop_width_ = layer->GetDisplayFrameWidth();
    source_crop_height_ = layer->GetDisplayFrameHeight();
    source_crop_.left = source_crop_.top = 0;
    source_crop_.right = source_crop_width_;
    source_crop_.top = source_crop_height_;
    imported_buffer_.reset(NULL);
  } else {
    ETRACE(
        "HWC don't support a layer with no buffer handle except in SolidColor "
        "type");
  }

  if (!surface_damage_.empty()) {
    if (type_ == kLayerCursor) {
      const std::shared_ptr<OverlayBuffer>& buffer = imported_buffer_->buffer_;
      surface_damage_.right = surface_damage_.left + buffer->GetWidth();
      surface_damage_.bottom = surface_damage_.top + buffer->GetHeight();
    }
  }

  if (!handle_constraints) {
    if (previous_layer) {
      ValidatePreviousFrameState(previous_layer, layer);
    }
#ifdef RECT_DAMAGE_TRACING
    IRECTDAMAGETRACE("Surface_damage after init (LTWH): %d, %d, %d, %d",
                     surface_damage_.left, surface_damage_.top,
                     (surface_damage_.right - surface_damage_.left),
                     (surface_damage_.bottom - surface_damage_.top));
#endif
    return;
  }

  int32_t left_constraint = layer->GetLeftConstraint();
  int32_t right_constraint = layer->GetRightConstraint();
  int32_t left_source_constraint = layer->GetLeftSourceConstraint();
  int32_t right_source_constraint = layer->GetRightSourceConstraint();
  int32_t display_frame_left = display_frame_.left;
  uint32_t frame_width = display_frame_.right - display_frame_.left;
  uint32_t source_width =
      static_cast<int>(source_crop_.right - source_crop_.left);
  uint32_t frame_offset_left = 0;
  uint32_t frame_offset_right = frame_width;
  if (left_constraint >= 0 && right_constraint >= 0) {
    if (display_frame_.left > right_source_constraint) {
      state_ |= kInvisible;
      return;
    }

    if (display_frame_.right < left_source_constraint) {
      state_ |= kInvisible;
      return;
    }

    if (display_frame_.left < left_source_constraint) {
      frame_offset_left = left_source_constraint - display_frame_left;
      display_frame_.left = left_source_constraint;
    }

    if (display_frame_.right > right_source_constraint) {
      frame_offset_right = right_source_constraint - display_frame_left;
      display_frame_.right = right_source_constraint;
    }

    // Handle case where we might be using logical and Mosaic together.
    IMOSAICDISPLAYTRACE("-------Layer[%d]", z_order_);
    IMOSAICDISPLAYTRACE(
        "Original display_frame_ %d %d %d %d  left_source_constraint: %d "
        "left_constraint: %d \n",
        display_frame_.left, display_frame_.right, display_frame_.top,
        display_frame_.bottom, left_source_constraint, left_constraint);

    display_frame_.left =
        (display_frame_.left - left_source_constraint) + left_constraint;
    display_frame_.right =
        (display_frame_.right - left_source_constraint) + left_constraint;
    IMOSAICDISPLAYTRACE(
        "display_frame_ %d %d %d %d  left_source_constraint: %d "
        "left_constraint: %d \n",
        display_frame_.left, display_frame_.right, display_frame_.top,
        display_frame_.bottom, left_source_constraint, left_constraint);

    IMOSAICDISPLAYTRACE(
        "Original surface_damage_ %d %d %d %d  left_source_constraint: %d "
        "left_constraint: %d \n",
        surface_damage_.left, surface_damage_.right, surface_damage_.top,
        surface_damage_.bottom, left_source_constraint, left_constraint);

    display_frame_.bottom =
        std::min(max_height, static_cast<uint32_t>(display_frame_.bottom));
    display_frame_width_ = display_frame_.right - display_frame_.left;
    display_frame_height_ = display_frame_.bottom - display_frame_.top;

    surface_damage_.left =
        (surface_damage_.left - left_source_constraint) + left_constraint;
    surface_damage_.right =
        (surface_damage_.right - left_source_constraint) + left_constraint;

    if ((surface_damage_.left < display_frame_.left) &&
        (surface_damage_.right > display_frame_.left)) {
      surface_damage_.left = display_frame_.left;
    }

    if (surface_damage_.right > display_frame_.right) {
      surface_damage_.right = display_frame_.right;
    }

    if (AnalyseOverlap(surface_damage_, display_frame_) != kOutside) {
      surface_damage_.bottom =
          std::min(surface_damage_.bottom, display_frame_.bottom);
      surface_damage_.right =
          std::min(surface_damage_.right, display_frame_.right);
      surface_damage_.left =
          std::max(surface_damage_.left, display_frame_.left);
    } else {
      surface_damage_.reset();
    }
    IMOSAICDISPLAYTRACE(
        "surface_damage_ %d %d %d %d  left_source_constraint: %d "
        "left_constraint: %d \n",
        surface_damage_.left, surface_damage_.right, surface_damage_.top,
        surface_damage_.bottom, left_source_constraint, left_constraint);

    // split the source in proportion of frame rect offset for sub displays as:
    // 1. the original source size may be different with the original frame
    // rect,
    //    we need get proportional content of source.
    // 2. the UI content may cross the sub displays of Mosaic or Logical mode

    source_crop_.left = static_cast<float>(source_width) *
                        (static_cast<float>(frame_offset_left) /
                         static_cast<float>(frame_width));
    source_crop_.right = static_cast<float>(source_width) *
                         (static_cast<float>(frame_offset_right) /
                          static_cast<float>(frame_width));
    source_crop_width_ = static_cast<int>(ceilf(source_crop_.right) -
                                          static_cast<int>(source_crop_.left));
    source_crop_height_ = static_cast<int>(ceilf(source_crop_.bottom) -
                                           static_cast<int>(source_crop_.top));
  }

  if (previous_layer) {
    ValidatePreviousFrameState(previous_layer, layer);
  }
}

void OverlayLayer::InitializeFromHwcLayer(
    HwcLayer* layer, ResourceManager* resource_manager,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    uint32_t max_height, uint32_t max_width, uint32_t rotation,
    bool handle_constraints) {
  display_frame_width_ = layer->GetDisplayFrameWidth();
  display_frame_height_ = layer->GetDisplayFrameHeight();
  display_frame_ = layer->GetDisplayFrame();
  InitializeState(layer, resource_manager, previous_layer, z_order, layer_index,
                  max_height, max_width, rotation, handle_constraints);
}

void OverlayLayer::InitializeFromScaledHwcLayer(
    HwcLayer* layer, ResourceManager* resource_manager,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    const HwcRect<int>& display_frame, uint32_t max_height, uint32_t max_width,
    uint32_t rotation, bool handle_constraints) {
  SetDisplayFrame(display_frame);
  InitializeState(layer, resource_manager, previous_layer, z_order, layer_index,
                  max_height, max_width, rotation, handle_constraints);
}

void OverlayLayer::ValidatePreviousFrameState(OverlayLayer* rhs,
                                              HwcLayer* layer) {
  OverlayBuffer* buffer = NULL;
  if (imported_buffer_.get())
    buffer = imported_buffer_->buffer_.get();

  supported_composition_ = rhs->supported_composition_;
  actual_composition_ = rhs->actual_composition_;

  bool content_changed = false;
  bool rect_changed = layer->HasDisplayRectChanged();
  bool source_rect_changed = layer->HasSourceRectChanged();
  if (source_rect_changed)
    state_ |= kSourceRectChanged;

  // We expect cursor plane to support alpha always.
  if ((actual_composition_ & kGpu) || (type_ == kLayerCursor) ||
      (type_ == kLayerSolidColor)) {
    if (actual_composition_ & kGpu) {
      content_changed = rect_changed || source_rect_changed;
      // This layer has replaced an existing layer, let's make sure
      // we re-draw this and previous layer regions.
      if (!layer->IsValidated()) {
        content_changed = true;
        CalculateRect(rhs->display_frame_, surface_damage_);
      } else if (!content_changed) {
        if ((buffer && rhs->imported_buffer_.get() &&
             (buffer->GetFormat() !=
              rhs->imported_buffer_->buffer_->GetFormat())) ||
            (alpha_ != rhs->alpha_) || (blending_ != rhs->blending_) ||
            (transform_ != rhs->transform_)) {
          content_changed = true;
        }
      }
    } else if (type_ == kLayerCursor) {
      if (layer->HasLayerAttributesChanged()) {
        state_ |= kNeedsReValidation;
      }
    }
  } else {
    // Ensure the buffer can be supported by display for direct
    // scanout.
    if (!rhs->imported_buffer_.get()) {
      state_ |= kNeedsReValidation;
      return;
    } else if (buffer && (buffer->GetFormat() !=
                          rhs->imported_buffer_->buffer_->GetFormat())) {
      state_ |= kNeedsReValidation;
      return;
    }

    // If previous layer was opaque and we have alpha now,
    // let's mark this layer for re-validation. Plane
    // supporting XRGB format might not necessarily support
    // transparent planes. We assume plane supporting
    // ARGB will support XRGB.
    if ((rhs->alpha_ == 0xff) && (alpha_ != rhs->alpha_)) {
      state_ |= kNeedsReValidation;
      return;
    }

    if (blending_ != rhs->blending_) {
      state_ |= kNeedsReValidation;
      return;
    }

    if (rect_changed || layer->HasLayerAttributesChanged()) {
      state_ |= kNeedsReValidation;
      return;
    }

    if (source_rect_changed) {
      // If the overall width and height hasn't changed, it
      // shouldn't impact the plane composition results.
      if ((source_crop_width_ != rhs->source_crop_width_) ||
          (source_crop_height_ != rhs->source_crop_height_)) {
        state_ |= kNeedsReValidation;
        return;
      }
    }
  }

  if (!rect_changed) {
    state_ &= ~kDimensionsChanged;
  }

  if (!layer->HasVisibleRegionChanged() && !content_changed &&
      surface_damage_.empty() && !layer->HasLayerContentChanged() &&
      !(state_ & kNeedsReValidation) && !layer->GetUseForMosaic()) {
    state_ &= ~kLayerContentChanged;
  }
}

void OverlayLayer::ValidateForOverlayUsage() {
  const std::shared_ptr<OverlayBuffer>& buffer = imported_buffer_->buffer_;
  type_ = buffer->GetUsage();
}

void OverlayLayer::CloneLayer(const OverlayLayer* layer,
                              const HwcRect<int>& display_frame,
                              ResourceManager* resource_manager,
                              uint32_t z_order) {
  int32_t fence = layer->GetAcquireFence();
  int32_t aquire_fence = 0;
  if (fence > 0) {
    aquire_fence = dup(fence);
  }
  SetDisplayFrame(display_frame);
  SetSourceCrop(layer->GetSourceCrop());
  OverlayBuffer* layer_buffer = layer->GetBuffer();
  if (layer_buffer) {
    SetBuffer(layer_buffer->GetOriginalHandle(), aquire_fence, resource_manager,
              true);
  }
  ValidateForOverlayUsage();
  surface_damage_ = layer->GetSurfaceDamage();
  transform_ = layer->transform_;
  plane_transform_ = layer->plane_transform_;
  merged_transform_ = layer->merged_transform_;
  alpha_ = layer->alpha_;
  layer_index_ = z_order;
  z_order_ = z_order;
  blending_ = layer->blending_;
  solid_color_ = layer->solid_color_;
}

void OverlayLayer::Dump() {
  DUMPTRACE("OverlayLayer Information Starts. -------------");
  switch (blending_) {
    case HWCBlending::kBlendingNone:
      DUMPTRACE("Blending: kBlendingNone.");
      break;
    case HWCBlending::kBlendingPremult:
      DUMPTRACE("Blending: kBlendingPremult.");
      break;
    case HWCBlending::kBlendingCoverage:
      DUMPTRACE("Blending: kBlendingCoverage.");
      break;
    default:
      break;
  }

  if (transform_ & kReflectX)
    DUMPTRACE("Transform: kReflectX.");
  if (transform_ & kReflectY)
    DUMPTRACE("Transform: kReflectY.");
  if (transform_ & kTransform90)
    DUMPTRACE("Transform: kTransform90.");
  else if (transform_ & kTransform180)
    DUMPTRACE("Transform: kTransform180.");
  else if (transform_ & kTransform270)
    DUMPTRACE("Transform: kTransform270.");
  else
    DUMPTRACE("Transform: kTransform0.");

  DUMPTRACE("Alpha: %u", alpha_);

  DUMPTRACE("SourceWidth: %d", source_crop_width_);
  DUMPTRACE("SourceHeight: %d", source_crop_height_);
  DUMPTRACE("DstWidth: %d", display_frame_width_);
  DUMPTRACE("DstHeight: %d", display_frame_height_);
  DUMPTRACE("Source crop %s", StringifyRect(source_crop_).c_str());
  DUMPTRACE("Display frame %s", StringifyRect(display_frame_).c_str());
  DUMPTRACE("Surface Damage %s", StringifyRect(surface_damage_).c_str());
  if (imported_buffer_) {
    DUMPTRACE("AquireFence: %d", imported_buffer_->acquire_fence_);
    imported_buffer_->buffer_->Dump();
  }
}

}  // namespace hwcomposer
