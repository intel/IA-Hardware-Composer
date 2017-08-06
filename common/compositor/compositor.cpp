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
#include "hwctrace.h"
#include "nativegpuresource.h"
#include "nativesurface.h"
#include "overlaylayer.h"
#include "renderer.h"
#include "renderstate.h"
#include "scopedrendererstate.h"
#include "hwcutils.h"

namespace hwcomposer {

Compositor::Compositor() {
}

Compositor::~Compositor() {
}

void Compositor::Init() {
  gpu_resource_handler_.reset(CreateNativeGpuResourceHandler());
}

bool Compositor::BeginFrame(bool disable_explicit_sync) {
  if (!renderer_) {
    renderer_.reset(CreateRenderer());
    if (!renderer_->Init()) {
      ETRACE("Failed to initialize OpenGL compositor %s", PRINTERROR());
      renderer_.reset(nullptr);
      return false;
    }
  }

  renderer_->SetExplicitSyncSupport(disable_explicit_sync);

  return true;
}

void Compositor::Reset() {
  renderer_.reset(nullptr);
}

bool Compositor::Draw(DisplayPlaneStateList &comp_planes,
                      std::vector<OverlayLayer> &layers,
                      const std::vector<HwcRect<int>> &display_frame) {
  CTRACE();
  const DisplayPlaneState *comp = NULL;
  std::vector<size_t> dedicated_layers;
  ScopedRendererState state(renderer_.get());
  if (!state.IsValid()) {
    ETRACE("Failed to draw as Renderer doesnt have a valid context.");
    return false;
  }

  if (!gpu_resource_handler_->PrepareResources(layers)) {
    ETRACE(
        "Failed to prepare GPU resources for compositing the frame, "
        "error: %s",
        PRINTERROR());
    ReleaseGpuResources();
    return false;
  }

  for (DisplayPlaneState &plane : comp_planes) {
    if (plane.GetCompositionState() == DisplayPlaneState::State::kScanout) {
      dedicated_layers.insert(dedicated_layers.end(),
                              plane.source_layers().begin(),
                              plane.source_layers().end());
    } else if (plane.GetCompositionState() ==
               DisplayPlaneState::State::kRender) {
      comp = &plane;
      std::vector<CompositionRegion> &comp_regions =
          plane.GetCompositionRegion();
      if (comp_regions.empty()) {
        SeparateLayers(dedicated_layers, comp->source_layers(), display_frame,
                       comp_regions);
      }

      std::vector<size_t>().swap(dedicated_layers);
      if (comp_regions.empty())
        continue;
      for (size_t texture_index : comp->source_layers()) {
        OverlayLayer &layer = layers.at(texture_index);
        int32_t fence = layer.ReleaseAcquireFence();
        if (fence > 0) {
          InsertFence(fence);
        }
      }

      if (!Render(layers, plane.GetOffScreenTarget(), comp_regions,
                  plane.ClearSurface())) {
        ETRACE("Failed to Render layer.");
        ReleaseGpuResources();
        return false;
      }
    }
  }

  ReleaseGpuResources();
  return true;
}

bool Compositor::DrawOffscreen(std::vector<OverlayLayer> &layers,
                               const std::vector<HwcRect<int>> &display_frame,
                               const std::vector<size_t> &source_layers,
                               NativeBufferHandler *buffer_handler,
                               uint32_t width, uint32_t height,
                               HWCNativeHandle output_handle,
                               int32_t *retire_fence) {
  ScopedRendererState state(renderer_.get());
  if (!state.IsValid()) {
    ETRACE("Failed to draw as Renderer doesnt have a valid context.");
    return false;
  }

  if (!gpu_resource_handler_->PrepareResources(layers)) {
    ETRACE(
        "Failed to prepare GPU resources for compositing the frame, "
        "error: %s",
        PRINTERROR());
    ReleaseGpuResources();
    return false;
  }

  std::vector<CompositionRegion> comp_regions;
  SeparateLayers(std::vector<size_t>(), source_layers, display_frame,
                 comp_regions);
  if (comp_regions.empty()) {
    ETRACE(
        "Failed to prepare offscreen buffer. "
        "error: %s",
        PRINTERROR());
    ReleaseGpuResources();
    return false;
  }

  for (size_t texture_index : source_layers) {
    OverlayLayer &layer = layers.at(texture_index);
    int32_t fence = layer.ReleaseAcquireFence();
    if (fence > 0) {
      InsertFence(fence);
    }
  }

  NativeSurface *surface = CreateBackBuffer(width, height);
  surface->InitializeForOffScreenRendering(buffer_handler, output_handle);

  if (!Render(layers, surface, comp_regions, true)) {
    ReleaseGpuResources();
    return false;
  }

  *retire_fence = surface->GetLayer()->ReleaseAcquireFence();

  ReleaseGpuResources();
  delete surface;

  return true;
}

void Compositor::InsertFence(int32_t fence) {
  renderer_->InsertFence(fence);
}

void Compositor::ReleaseGpuResources() {
  gpu_resource_handler_->ReleaseGPUResources();
}

bool Compositor::Render(std::vector<OverlayLayer> &layers,
                        NativeSurface *surface,
                        const std::vector<CompositionRegion> &comp_regions,
                        bool clear_surface) {
  CTRACE();
  std::vector<RenderState> states;
  size_t num_regions = comp_regions.size();
  states.reserve(num_regions);

  for (size_t region_index = 0; region_index < num_regions; region_index++) {
    const CompositionRegion &region = comp_regions.at(region_index);
    RenderState state;
    state.ConstructState(layers, region, gpu_resource_handler_.get(),
                         surface->GetSurfaceDamage(), clear_surface);
    if (state.layer_state_.empty()) {
      continue;
    }

    states.emplace(states.begin(), state);
  }

  if (states.empty()) {
    return true;
  }

  if (!renderer_->Draw(states, surface, clear_surface))
    return false;

  return true;
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
  get_draw_regions(layer_rects, &separate_regions);
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
