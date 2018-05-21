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
#include "compositorthread.h"
#include "displayplanestate.h"
#include "factory.h"
#include "renderstate.h"

namespace hwcomposer {

class DisplayPlaneManager;
class ResourceManager;
struct OverlayLayer;

class Compositor {
 public:
  Compositor();
  Compositor(const Compositor &) = delete;
  Compositor &operator=(const Compositor &) = delete;
  ~Compositor();

  void Init(ResourceManager *buffer_manager, uint32_t gpu_fd,
            FrameBufferManager *frame_buffer_manager);
  void Reset();
  void BeginFrame(bool disable_explicit_sync);
  bool Draw(DisplayPlaneStateList &planes, std::vector<OverlayLayer> &layers,
            const std::vector<HwcRect<int>> &display_frame);
  bool DrawOffscreen(std::vector<OverlayLayer> &layers,
                     const std::vector<HwcRect<int>> &display_frame,
                     const std::vector<size_t> &source_layers,
                     ResourceManager *resource_manager, uint32_t width,
                     uint32_t height, HWCNativeHandle output_handle,
                     int32_t acquire_fence, int32_t *retire_fence);
  void FreeResources();

  void SetVideoScalingMode(uint32_t);
  void SetVideoColor(HWCColorControl color, float value);
  void GetVideoColor(HWCColorControl color, float *value, float *start,
                     float *end);
  void RestoreVideoDefaultColor(HWCColorControl color);

  void SetVideoDeinterlace(HWCDeinterlaceFlag flag, HWCDeinterlaceControl mode);
  void RestoreVideoDefaultDeinterlace();

 private:
  void CalculateRenderState(std::vector<OverlayLayer> &layers,
                            const std::vector<CompositionRegion> &comp_regions,
                            DrawState &state, uint32_t downscaling_factor,
                            bool uses_display_up_scaling,
                            bool use_plane_transform = false);
  void SeparateLayers(const std::vector<size_t> &dedicated_layers,
                      const std::vector<size_t> &source_layers,
                      const std::vector<HwcRect<int>> &display_frame,
                      const HwcRect<int> &damage_region,
                      std::vector<CompositionRegion> &comp_regions);

  std::unique_ptr<CompositorThread> thread_;
  SpinLock lock_;
  HWCColorMap colors_;
  uint32_t scaling_mode_ = 0;
  HWCDeinterlaceProp deinterlace_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_COMPOSITOR_H_
