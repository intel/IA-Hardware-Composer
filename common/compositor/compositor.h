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

#ifndef COMMON_COMPOSITOR_COMPOSITOR_H_
#define COMMON_COMPOSITOR_COMPOSITOR_H_

#include <platformdefines.h>

#include <memory>
#include <vector>

#include "compositionregion.h"
#include "displayplanestate.h"
#include "renderstate.h"
#include "compositorthread.h"
#include "factory.h"

namespace hwcomposer {

class DisplayPlaneManager;
class ResourceManager;
struct OverlayLayer;

class Compositor {
 public:
  Compositor();
  ~Compositor();

  void Init(ResourceManager *buffer_manager, uint32_t gpu_fd);
  void Reset();

  Compositor(const Compositor &) = delete;

  bool BeginFrame(bool disable_explicit_sync);
  bool Draw(DisplayPlaneStateList &planes, std::vector<OverlayLayer> &layers,
            const std::vector<HwcRect<int>> &display_frame);
  bool DrawOffscreen(std::vector<OverlayLayer> &layers,
                     const std::vector<HwcRect<int>> &display_frame,
                     const std::vector<size_t> &source_layers,
                     ResourceManager *resource_manager, uint32_t width,
                     uint32_t height, HWCNativeHandle output_handle,
                     int32_t acquire_fence, int32_t *retire_fence);
  void FreeResources();

  void SetVideoColor(HWCColorControl color, float value);
  void GetVideoColor(HWCColorControl color, float *value, float *start,
                     float *end);
  void RestoreVideoDefaultColor(HWCColorControl color);
  void SetVideoSharp(float value);
  void GetVideoSharp(float *value, float *start, float *end);
  void RestoreVideoDefaultSharp();

 private:
  bool CalculateRenderState(std::vector<OverlayLayer> &layers,
                            const std::vector<CompositionRegion> &comp_regions,
                            DrawState &state);
  void SeparateLayers(const std::vector<size_t> &dedicated_layers,
                      const std::vector<size_t> &source_layers,
                      const std::vector<HwcRect<int>> &display_frame,
                      std::vector<CompositionRegion> &comp_regions);

  std::unique_ptr<CompositorThread> thread_;
  SpinLock lock_;
  HWCColorMap colors_;
  float sharp_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_COMPOSITOR_H_
