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

#include "compositor.h"

#include <xf86drmMode.h>

#include <algorithm>

#include "disjoint_layers.h"
#include "displayplanestate.h"
#include "hwcdefs.h"
#include "hwctrace.h"
#include "hwcutils.h"
#include "nativegpuresource.h"
#include "nativesurface.h"
#include "overlaylayer.h"
#include "renderer.h"

namespace hwcomposer {

Compositor::Compositor() {
  deinterlace_.flag_ = HWCDeinterlaceFlag::kDeinterlaceFlagNone;
  deinterlace_.mode_ = HWCDeinterlaceControl::kDeinterlaceNone;
}

Compositor::~Compositor() {
}

void Compositor::Init(ResourceManager *resource_manager, uint32_t gpu_fd,
                      FrameBufferManager *frame_buffer_manager) {
  if (!thread_)
    thread_.reset(new CompositorThread());

  thread_->Initialize(resource_manager, gpu_fd, frame_buffer_manager);
}

void Compositor::BeginFrame(bool disable_explicit_sync) {
  thread_->SetExplicitSyncSupport(disable_explicit_sync);
}

void Compositor::Reset() {
  if (thread_)
    thread_->ExitThread();
}

bool Compositor::Draw(DisplayPlaneStateList &comp_planes,
                      std::vector<OverlayLayer> &layers,
                      const std::vector<HwcRect<int>> &display_frame) {
  CTRACE();
  const DisplayPlaneState *comp = NULL;
  std::vector<size_t> dedicated_layers;
  std::vector<DrawState> draw_state;
  std::vector<DrawState> media_state;

  for (DisplayPlaneState &plane : comp_planes) {
    if (plane.Scanout()) {
      if (!plane.IsSurfaceRecycled()) {
        dedicated_layers.insert(dedicated_layers.end(),
                                plane.GetSourceLayers().begin(),
                                plane.GetSourceLayers().end());
      }
    } else if (plane.IsVideoPlane()) {
      dedicated_layers.insert(dedicated_layers.end(),
                              plane.GetSourceLayers().begin(),
                              plane.GetSourceLayers().end());
      media_state.emplace_back();
      plane.SwapSurfaceIfNeeded();
      DrawState &state = media_state.back();
      state.surface_ = plane.GetOffScreenTarget();
      MediaState &media_state = state.media_state_;
      lock_.lock();
      media_state.colors_ = colors_;
      media_state.scaling_mode_ = scaling_mode_;
      media_state.deinterlace_ = deinterlace_;
      lock_.unlock();
      const OverlayLayer &layer = layers[plane.GetSourceLayers().at(0)];
      media_state.layer_ = &layer;
    } else if (plane.NeedsOffScreenComposition()) {
      comp = &plane;
      plane.SwapSurfaceIfNeeded();
      std::vector<CompositionRegion> &comp_regions =
          plane.GetCompositionRegion();
      bool regions_empty = comp_regions.empty();
      NativeSurface *surface = plane.GetOffScreenTarget();
      if (surface == NULL) {
        ETRACE("GetOffScreenTarget() returned NULL pointer 'surface'.");
        return false;
      }
      if (!regions_empty &&
          (surface->ClearSurface() || surface->IsPartialClear() ||
           surface->IsSurfaceDamageChanged())) {
        plane.ResetCompositionRegion();
        regions_empty = true;
      }

      if (surface->ClearSurface()) {
        plane.UpdateDamage(plane.GetDisplayFrame());
      }

      if (regions_empty) {
        SeparateLayers(dedicated_layers, comp->GetSourceLayers(), display_frame,
                       surface->GetSurfaceDamage(), comp_regions);
      }

      std::vector<size_t>().swap(dedicated_layers);
      if (comp_regions.empty())
        continue;

      draw_state.emplace_back();
      DrawState &state = draw_state.back();
      state.surface_ = surface;
      size_t num_regions = comp_regions.size();
      state.states_.reserve(num_regions);
      bool use_plane_transform = false;
      if (plane.GetRotationType() ==
          DisplayPlaneState::RotationType::kGPURotation) {
        use_plane_transform = true;
      }

      CalculateRenderState(layers, comp_regions, state,
                           plane.GetDownScalingFactor(),
                           plane.IsUsingPlaneScalar(), use_plane_transform);

      if (state.states_.empty()) {
        draw_state.pop_back();
      }
    }
  }

  bool status = true;
  if (!draw_state.empty() || !media_state.empty())
    status = thread_->Draw(draw_state, media_state, layers);

  return status;
}

bool Compositor::DrawOffscreen(std::vector<OverlayLayer> &layers,
                               const std::vector<HwcRect<int>> &display_frame,
                               const std::vector<size_t> &source_layers,
                               ResourceManager *resource_manager,
                               uint32_t width, uint32_t height,
                               HWCNativeHandle output_handle,
                               int32_t acquire_fence, int32_t *retire_fence) {
  std::vector<CompositionRegion> comp_regions;
  SeparateLayers(std::vector<size_t>(), source_layers, display_frame,
                 HwcRect<int>(0, 0, width, height), comp_regions);
  if (comp_regions.empty()) {
    ETRACE(
        "Failed to prepare offscreen buffer. "
        "error: %s",
        PRINTERROR());
    return false;
  }

  NativeSurface *surface = Create3DBuffer(width, height);
  surface->InitializeForOffScreenRendering(output_handle, resource_manager);
  std::vector<DrawState> draw;
  std::vector<DrawState> media;
  draw.emplace_back();
  DrawState &draw_state = draw.back();
  draw_state.destroy_surface_ = true;
  draw_state.surface_ = surface;
  size_t num_regions = comp_regions.size();
  draw_state.states_.reserve(num_regions);
  CalculateRenderState(layers, comp_regions, draw_state, 1, false);

  if (draw_state.states_.empty()) {
    return true;
  }

  if (acquire_fence > 0) {
    draw_state.acquire_fences_.emplace_back(acquire_fence);
  }

  bool status = thread_->Draw(draw, media, layers);
  if (status) {
    *retire_fence = draw_state.retire_fence_;
  } else {
    *retire_fence = -1;
  }

  return status;
}

void Compositor::FreeResources() {
  thread_->FreeResources();
}

void Compositor::CalculateRenderState(
    std::vector<OverlayLayer> &layers,
    const std::vector<CompositionRegion> &comp_regions, DrawState &draw_state,
    uint32_t downscaling_factor, bool uses_display_up_scaling,
    bool use_plane_transform) {
  CTRACE();
  size_t num_regions = comp_regions.size();
  for (size_t region_index = 0; region_index < num_regions; region_index++) {
    const CompositionRegion &region = comp_regions.at(region_index);
    RenderState state;
    state.ConstructState(layers, region, downscaling_factor,
                         uses_display_up_scaling, use_plane_transform);
    if (state.layer_state_.empty()) {
      continue;
    }

    draw_state.states_.emplace(draw_state.states_.begin(), state);
    const std::vector<size_t> &source = region.source_layers;
    for (size_t texture_index : source) {
      OverlayLayer &layer = layers.at(texture_index);
      int32_t fence = layer.ReleaseAcquireFence();
      if (fence > 0) {
        draw_state.acquire_fences_.emplace_back(fence);
      }
    }
  }
}

void Compositor::SetVideoScalingMode(uint32_t mode) {
  lock_.lock();
  scaling_mode_ = mode;
  lock_.unlock();
}

void Compositor::SetVideoColor(HWCColorControl color, float value) {
  lock_.lock();
  colors_[color].value_ = value;
  colors_[color].use_default_ = false;
  lock_.unlock();
}

void Compositor::GetVideoColor(HWCColorControl /*color*/, float * /*value*/,
                               float * /*start*/, float * /*end*/) {
  // TODO
}

void Compositor::RestoreVideoDefaultColor(HWCColorControl color) {
  lock_.lock();
  colors_[color].use_default_ = true;
  lock_.unlock();
}

void Compositor::SetVideoDeinterlace(HWCDeinterlaceFlag flag,
                                     HWCDeinterlaceControl mode) {
  lock_.lock();
  deinterlace_.flag_ = flag;
  deinterlace_.mode_ = mode;
  lock_.unlock();
}

void Compositor::RestoreVideoDefaultDeinterlace() {
  lock_.lock();
  deinterlace_.flag_ = HWCDeinterlaceFlag::kDeinterlaceFlagNone;
  deinterlace_.mode_ = HWCDeinterlaceControl::kDeinterlaceNone;
  lock_.unlock();
}

// Below code is taken from drm_hwcomposer adopted to our needs.
static std::vector<size_t> SetBitsToVector(
    uint64_t in, const std::vector<size_t> &index_map) {
  std::vector<size_t> out;
  size_t msb = sizeof(in) * 8 - 1;
  uint64_t mask = (uint64_t)1 << msb;
  for (size_t i = msb; mask != (uint64_t)0; i--, mask >>= 1)
    if (in & mask)
      out.emplace_back(index_map[i]);
  return out;
}

void Compositor::SeparateLayers(const std::vector<size_t> &dedicated_layers,
                                const std::vector<size_t> &source_layers,
                                const std::vector<HwcRect<int>> &display_frame,
                                const HwcRect<int> &damage_region,
                                std::vector<CompositionRegion> &comp_regions) {
  CTRACE();
  if (source_layers.size() > 64) {
    ETRACE("Failed to separate layers because there are more than 64");
    return;
  }

  size_t num_exclude_rects = 0;
  // Index at which the actual layers begin
  size_t layer_offset = dedicated_layers.size();
  if (source_layers.size() + layer_offset > 64) {
    WTRACE(
        "Exclusion rectangles are being truncated to make the rectangle count "
        "fit into 64");
    num_exclude_rects = 64 - source_layers.size() - dedicated_layers.size();
  }

  // We inject all the exclude rects into the rects list. Any resulting rect
  // that includes ANY of the first num_exclude_rects is rejected. After the
  // exclude rects, we add the lower layers. The rects that intersect with
  // these layers will be inspected and only those which are to be composited
  // above the layer will be included in the composition regions.
  std::vector<HwcRect<int>> layer_rects(source_layers.size() + layer_offset);
  std::transform(
      dedicated_layers.begin(), dedicated_layers.end(),
      layer_rects.begin() + num_exclude_rects,
      [=](size_t layer_index) { return display_frame[layer_index]; });
  std::transform(source_layers.begin(), source_layers.end(),
                 layer_rects.begin() + layer_offset, [=](size_t layer_index) {
                   return display_frame[layer_index];
                 });

  std::vector<RectSet<int>> separate_regions;
  get_draw_regions(layer_rects, damage_region, &separate_regions);
  uint64_t exclude_mask = ((uint64_t)1 << num_exclude_rects) - 1;
  uint64_t dedicated_mask = (((uint64_t)1 << dedicated_layers.size()) - 1)
                            << num_exclude_rects;

  for (RectSet<int> &region : separate_regions) {
    if (region.id_set.getBits() & exclude_mask)
      continue;

    // If a rect intersects one of the dedicated layers, we need to remove the
    // layers from the composition region which appear *below* the dedicated
    // layer. This effectively punches a hole through the composition layer such
    // that the dedicated layer can be placed below the composition and not
    // be occluded.
    uint64_t dedicated_intersect = region.id_set.getBits() & dedicated_mask;
    for (size_t i = 0; dedicated_intersect && i < dedicated_layers.size();
         ++i) {
      // Only exclude layers if they intersect this particular dedicated layer
      if (!(dedicated_intersect & (1 << (i + num_exclude_rects))))
        continue;

      for (size_t j = 0; j < source_layers.size(); ++j) {
        if (source_layers[j] < dedicated_layers[i])
          region.id_set.subtract(j + layer_offset);
      }
    }

    if (!(region.id_set.getBits() >> layer_offset))
      continue;

    comp_regions.emplace_back(CompositionRegion{
        region.rect, SetBitsToVector(region.id_set.getBits() >> layer_offset,
                                     source_layers)});
  }
}

}  // namespace hwcomposer
