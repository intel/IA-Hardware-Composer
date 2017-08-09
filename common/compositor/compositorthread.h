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

class CompositorThread : public HWCThread {
 public:
  CompositorThread();
  ~CompositorThread() override;

  void Initialize(DisplayPlaneManager* plane_manager);

  void Draw(std::vector<DrawState>& states,
            const std::vector<OverlayLayer>& layers);

  void SetExplicitSyncSupport(bool disable_explicit_sync);
  void FreeResources(bool all_resources);
  void EnsureTasksAreDone();

  void HandleRoutine() override;
  void ExitThread();

 private:
  enum Tasks {
    kNone = 0,                   // No tasks
    kRender = 1 << 1,            // Render content.
    kReleaseResources = 1 << 2,  // Release surfaces from plane manager.
  };

  void ReleaseGpuResources();
  void HandleDrawRequest();
  void HandleReleaseRequest();
  void Wait();

  SpinLock tasks_lock_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<NativeGpuResource> gpu_resource_handler_;
  std::vector<OverlayBuffer*> buffers_;
  std::vector<DrawState> states_;
  bool disable_explicit_sync_;
  bool release_all_resources_;
  DisplayPlaneManager* plane_manager_ = NULL;
  uint32_t tasks_ = kNone;
  FDHandler fd_chandler_;
  HWCEvent cevent_;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_COMPOSITORTHREAD_H_
