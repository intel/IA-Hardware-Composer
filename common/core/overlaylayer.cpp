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
  if (imported_buffer_->acquire_fence_ > 0) {
    close(imported_buffer_->acquire_fence_);
  }

  imported_buffer_->acquire_fence_ = acquire_fence;
}

int32_t OverlayLayer::GetAcquireFence() const {
  return imported_buffer_->acquire_fence_;
}

int32_t OverlayLayer::ReleaseAcquireFence() const {
  int32_t fence = imported_buffer_->acquire_fence_;
  imported_buffer_->acquire_fence_ = -1;
  return fence;
}

OverlayBuffer* OverlayLayer::GetBuffer() const {
  if (imported_buffer_->buffer_.get() == NULL)
    ETRACE("hwc layer get NullBuffer");

  return imported_buffer_->buffer_.get();
}

std::shared_ptr<OverlayBuffer>& OverlayLayer::GetSharedBuffer() const {
  return imported_buffer_->buffer_;
}

void OverlayLayer::SetBuffer(HWCNativeHandle handle, int32_t acquire_fence,
                             ResourceManager* resource_manager,
                             bool register_buffer,
                             FrameBufferManager* frame_buffer_manager) {
  std::shared_ptr<OverlayBuffer> buffer(NULL);

  uint32_t id;

  if (resource_manager && register_buffer) {
    uint32_t gpu_fd = resource_manager->GetNativeBufferHandler()->GetFd();
    id = GetNativeBuffer(gpu_fd, handle);
    buffer = resource_manager->FindCachedBuffer(id);
  }

  if (buffer == NULL) {
    buffer = OverlayBuffer::CreateOverlayBuffer();
    buffer->InitializeFromNativeHandle(handle, resource_manager,
                                       frame_buffer_manager);
    if (resource_manager && register_buffer) {
      resource_manager->RegisterBuffer(id, buffer);
    }
  } else {
    buffer->SetOriginalHandle(handle);
  }

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
  plane_transform_ = transform;
  transform_ = transform;
}

void OverlayLayer::ValidateTransform(uint32_t transform,
                                     uint32_t display_transform) {
  std::map<int, int> tmap = {{kIdentity, 0},
                             {kTransform90, 1},
                             {kTransform180, 2},
                             {kTransform270, 3}};
  std::vector<int> inv_tmap = {kIdentity, kTransform90, kTransform180,
                               kTransform270};

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

  // The elements {0, 1, 2, 3} form a circulant matrix under mod 4 arithmetic
  mtransform = (mtransform + display_transform) % 4;
  mtransform = inv_tmap[mtransform];
  plane_transform_ = mtransform;

  if (plane_transform_ & kTransform90) {
    if (transform & kReflectX)
      plane_transform_ |= kReflectX;

    if (transform & kReflectY)
      plane_transform_ |= kReflectY;
  }
}

void OverlayLayer::InitializeState(HwcLayer* layer,
                                   ResourceManager* resource_manager,
                                   OverlayLayer* previous_layer,
                                   uint32_t z_order, uint32_t layer_index,
                                   uint32_t max_height, uint32_t rotation,
                                   bool handle_constraints,
                                   FrameBufferManager* frame_buffer_manager) {
  transform_ = layer->GetTransform();
  if (rotation != kRotateNone) {
    ValidateTransform(layer->GetTransform(), rotation);
  } else {
    plane_transform_ = transform_;
  }

  alpha_ = layer->GetAlpha();
  layer_index_ = layer_index;
  z_order_ = z_order;
  source_crop_width_ = layer->GetSourceCropWidth();
  source_crop_height_ = layer->GetSourceCropHeight();
  source_crop_ = layer->GetSourceCrop();
  blending_ = layer->GetBlending();
  surface_damage_ = layer->GetLayerDamage();
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
    } else if (!layer->IsCursorLayer()) {
      state_ |= kNeedsReValidation;
    }
  }

  SetBuffer(layer->GetNativeHandle(), layer->GetAcquireFence(),
            resource_manager, true, frame_buffer_manager);

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
    display_frame_.left =
        (display_frame_.left - left_source_constraint) + left_constraint;
    display_frame_.right =
        (display_frame_.right - left_source_constraint) + left_constraint;
    IMOSAICDISPLAYTRACE(
        "display_frame_ %d %d %d %d  left_source_constraint: %d "
        "left_constraint: %d \n",
        display_frame_.left, display_frame_.right, display_frame_.top,
        display_frame_.bottom, left_source_constraint, left_constraint);

    display_frame_.bottom =
        std::min(max_height, static_cast<uint32_t>(display_frame_.bottom));
    display_frame_width_ = display_frame_.right - display_frame_.left;
    display_frame_height_ = display_frame_.bottom - display_frame_.top;

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
    uint32_t max_height, uint32_t rotation, bool handle_constraints,
    FrameBufferManager* frame_buffer_manager) {
  display_frame_width_ = layer->GetDisplayFrameWidth();
  display_frame_height_ = layer->GetDisplayFrameHeight();
  display_frame_ = layer->GetDisplayFrame();
  InitializeState(layer, resource_manager, previous_layer, z_order, layer_index,
                  max_height, rotation, handle_constraints,
                  frame_buffer_manager);
}

void OverlayLayer::InitializeFromScaledHwcLayer(
    HwcLayer* layer, ResourceManager* resource_manager,
    OverlayLayer* previous_layer, uint32_t z_order, uint32_t layer_index,
    const HwcRect<int>& display_frame, uint32_t max_height, uint32_t rotation,
    bool handle_constraints, FrameBufferManager* frame_buffer_manager) {
  SetDisplayFrame(display_frame);
  InitializeState(layer, resource_manager, previous_layer, z_order, layer_index,
                  max_height, rotation, handle_constraints,
                  frame_buffer_manager);
}

void OverlayLayer::ValidatePreviousFrameState(OverlayLayer* rhs,
                                              HwcLayer* layer) {
  OverlayBuffer* buffer = imported_buffer_->buffer_.get();
  supported_composition_ = rhs->supported_composition_;
  actual_composition_ = rhs->actual_composition_;

  bool content_changed = false;
  bool rect_changed = layer->HasDisplayRectChanged();
  bool source_rect_changed = layer->HasSourceRectChanged();
  if (source_rect_changed)
    state_ |= kSourceRectChanged;

  // We expect cursor plane to support alpha always.
  if ((actual_composition_ & kGpu) || (type_ == kLayerCursor)) {
    if (actual_composition_ & kGpu) {
      content_changed = rect_changed || source_rect_changed;
      // This layer has replaced an existing layer, let's make sure
      // we re-draw this and previous layer regions.
      if (!layer->IsValidated()) {
        content_changed = true;
        CalculateRect(rhs->display_frame_, surface_damage_);
      } else if (!content_changed) {
        if ((buffer->GetFormat() !=
             rhs->imported_buffer_->buffer_->GetFormat()) ||
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
    if (buffer->GetFormat() != rhs->imported_buffer_->buffer_->GetFormat()) {
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
      !(state_ & kNeedsReValidation)) {
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
                              uint32_t z_order,
                              FrameBufferManager* frame_buffer_manager) {
  int32_t fence = layer->GetAcquireFence();
  int32_t aquire_fence = 0;
  if (fence > 0) {
    aquire_fence = dup(fence);
  }
  SetDisplayFrame(display_frame);
  SetSourceCrop(layer->GetSourceCrop());
  SetBuffer(layer->GetBuffer()->GetOriginalHandle(), aquire_fence,
            resource_manager, true, frame_buffer_manager);
  ValidateForOverlayUsage();
  surface_damage_ = layer->GetSurfaceDamage();
  transform_ = layer->transform_;
  plane_transform_ = layer->plane_transform_;
  alpha_ = layer->alpha_;
  layer_index_ = z_order;
  z_order_ = z_order;
  blending_ = layer->blending_;
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
  DUMPTRACE("AquireFence: %d", imported_buffer_->acquire_fence_);

  imported_buffer_->buffer_->Dump();
}

}  // namespace hwcomposer
