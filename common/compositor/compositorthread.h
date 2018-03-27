/*
// Copyright (c) 2017 Intel Corporation
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

#ifndef COMMON_COMPOSITOR_COMPOSITORTHREAD_H_
#define COMMON_COMPOSITOR_COMPOSITORTHREAD_H_

#include <spinlock.h>
#include <platformdefines.h>

#include <memory>
#include <vector>

#include "renderstate.h"
#include "factory.h"
#include "hwcthread.h"

#include "fdhandler.h"
#include "hwcevent.h"

namespace hwcomposer {

class OverlayBuffer;
class DisplayPlaneManager;
class ResourceManager;
class NativeBufferHandler;

class CompositorThread : public HWCThread {
 public:
  CompositorThread();
  ~CompositorThread() override;

  void Initialize(ResourceManager* resource_manager, uint32_t gpu_fd);

  bool Draw(std::vector<DrawState>& states,
            std::vector<DrawState>& media_states,
            const std::vector<OverlayLayer>& layers);

  void SetExplicitSyncSupport(bool disable_explicit_sync);
  void UpdateLayerPixelData(std::vector<OverlayLayer>& layers);
  void EnsurePixelDataUpdated();
  void FreeResources();

  void HandleRoutine() override;
  void HandleExit() override;
  void ExitThread();

 private:
  enum Tasks {
    kNone = 0,           // No tasks
    kRender3D = 1 << 1,  // Render content.
    kRenderMedia = 1 << 2,
    kReleaseResources = 1 << 3,  // Release surfaces from plane manager.
    kRefreshRawPixelData = 1 << 4
  };

  void Handle3DDrawRequest();
  void HandleMediaDrawRequest();
  void HandleReleaseRequest();
  void HandleRawPixelUpdate();
  void Wait();
  void Ensure3DRenderer();
  void EnsureMediaRenderer();

  SpinLock tasks_lock_;
  SpinLock pixel_data_lock_;
  std::unique_ptr<Renderer> gl_renderer_;
  std::unique_ptr<Renderer> media_renderer_;
  std::unique_ptr<NativeGpuResource> gpu_resource_handler_;
  std::vector<OverlayBuffer*> buffers_;
  std::vector<OverlayBuffer*> pixel_data_;
  std::vector<DrawState> states_;
  std::vector<DrawState> media_states_;
  std::vector<ResourceHandle> purged_resources_;
  bool disable_explicit_sync_ = false;
  bool draw_succeeded_ = false;
  ResourceManager* resource_manager_ = NULL;
  uint32_t tasks_ = kNone;
  uint32_t gpu_fd_ = 0;
  FDHandler fd_chandler_;
  HWCEvent cevent_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_COMPOSITORTHREAD_H_
