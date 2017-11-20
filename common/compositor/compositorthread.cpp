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

#include "compositorthread.h"

#include "hwcutils.h"
#include "hwctrace.h"
#include "nativegpuresource.h"
#include "overlaylayer.h"
#include "renderer.h"
#include "displayplanemanager.h"
#include "nativesurface.h"

namespace hwcomposer {

CompositorThread::CompositorThread() : HWCThread(-8, "CompositorThread") {
  if (!cevent_.Initialize())
    return;

  fd_chandler_.AddFd(cevent_.get_fd());
}

CompositorThread::~CompositorThread() {
}

void CompositorThread::Initialize(DisplayPlaneManager *plane_manager) {
  tasks_lock_.lock();
  if (!gpu_resource_handler_)
    gpu_resource_handler_.reset(CreateNativeGpuResourceHandler());

  plane_manager_ = plane_manager;
  tasks_lock_.unlock();
  if (!InitWorker()) {
    ETRACE("Failed to initalize CompositorThread. %s", PRINTERROR());
  }
}

void CompositorThread::SetExplicitSyncSupport(bool disable_explicit_sync) {
  disable_explicit_sync_ = disable_explicit_sync;
}

void CompositorThread::EnsureTasksAreDone() {
  tasks_lock_.lock();
  tasks_lock_.unlock();
}

void CompositorThread::FreeResources(bool all_resources) {
  release_all_resources_ = all_resources;
  tasks_lock_.lock();
  tasks_ |= kReleaseResources;
  tasks_lock_.unlock();
  Resume();
}

void CompositorThread::Wait() {
  if (fd_chandler_.Poll(-1) <= 0) {
    ETRACE("Poll Failed in DisplayManager %s", PRINTERROR());
    return;
  }

  if (fd_chandler_.IsReady(cevent_.get_fd())) {
    // If eventfd_ is ready, we need to wait on it (using read()) to clean
    // the flag that says it is ready.
    cevent_.Wait();
  }
}

void CompositorThread::Draw(std::vector<DrawState> &states,
                            std::vector<DrawState> &media_states,
                            const std::vector<OverlayLayer> &layers) {
  states_.swap(states);
  tasks_lock_.lock();

  if (!states_.empty()) {
    std::vector<OverlayBuffer *>().swap(buffers_);
    buffers_.reserve(layers.size());
    for (auto &layer : layers) {
      buffers_.emplace_back(layer.GetBuffer());
    }

    tasks_ |= kRender3D;
  }

  if (!media_states.empty()) {
    media_states_.swap(media_states);
    tasks_ |= kRenderMedia;
  }

  tasks_lock_.unlock();
  Resume();
  Wait();
}

void CompositorThread::ExitThread() {
  FreeResources(true);
  HWCThread::Exit();
  std::vector<DrawState>().swap(states_);
  std::vector<OverlayBuffer *>().swap(buffers_);
  release_all_resources_ = false;
}

void CompositorThread::ReleaseGpuResources() {
  gpu_resource_handler_->ReleaseGPUResources();
}

void CompositorThread::HandleExit() {
  gl_renderer_.reset(nullptr);
}

void CompositorThread::HandleRoutine() {
  bool signal = false;
  if (tasks_ & kRender3D) {
    Handle3DDrawRequest();
    signal = true;
  }

  if (tasks_ & kRenderMedia) {
    HandleMediaDrawRequest();
    signal = true;
  }

  if (tasks_ & kReleaseResources) {
    HandleReleaseRequest();
  }

  if (signal) {
    cevent_.Signal();
  }
}

void CompositorThread::HandleReleaseRequest() {
  ScopedSpinLock lock(tasks_lock_);
  tasks_ &= ~kReleaseResources;

  if (!plane_manager_)
    return;

  if (!plane_manager_->HasSurfaces())
    return;

  Ensure3DRenderer();

  if (release_all_resources_) {
    gpu_resource_handler_->ReleaseGPUResources();
    plane_manager_->ReleaseAllOffScreenTargets();
    gpu_resource_handler_.reset(nullptr);
  } else {
    plane_manager_->ReleaseFreeOffScreenTargets();
  }
}

void CompositorThread::Handle3DDrawRequest() {
  tasks_lock_.lock();
  tasks_ &= ~kRender3D;
  tasks_lock_.unlock();

  Ensure3DRenderer();
  if (!gl_renderer_) {
    return;
  }

  gl_renderer_->SetExplicitSyncSupport(disable_explicit_sync_);

  if (!gpu_resource_handler_->PrepareResources(buffers_)) {
    ETRACE(
        "Failed to prepare GPU resources for compositing the frame, "
        "error: %s",
        PRINTERROR());
    ReleaseGpuResources();
    return;
  }

  size_t size = states_.size();
  for (size_t i = 0; i < size; i++) {
    DrawState &draw_state = states_.at(i);
    for (RenderState &render_state : draw_state.states_) {
      std::vector<RenderState::LayerState> &layer_state =
          render_state.layer_state_;

      for (RenderState::LayerState &temp : layer_state) {
        temp.handle_ =
            gpu_resource_handler_->GetResourceHandle(temp.layer_index_);
      }
    }

    const std::vector<int32_t> &fences = draw_state.acquire_fences_;
    for (int32_t fence : fences) {
      gl_renderer_->InsertFence(fence);
    }

    std::vector<int32_t>().swap(draw_state.acquire_fences_);

    if (!gl_renderer_->Draw(draw_state.states_, draw_state.surface_,
                            draw_state.surface_->ClearSurface())) {
      ETRACE(
          "Failed to prepare GPU resources for compositing the frame, "
          "error: %s",
          PRINTERROR());
      break;
    }

    if (draw_state.destroy_surface_) {
      draw_state.retire_fence_ =
          draw_state.surface_->GetLayer()->ReleaseAcquireFence();
      delete draw_state.surface_;
    }
  }

  if (disable_explicit_sync_)
    gl_renderer_->InsertFence(-1);
}

void CompositorThread::HandleMediaDrawRequest() {
  tasks_lock_.lock();
  tasks_ &= ~kRenderMedia;
  tasks_lock_.unlock();

  EnsureMediaRenderer();
  if (!media_renderer_) {
    return;
  }

  size_t size = media_states_.size();
  for (size_t i = 0; i < size; i++) {
    DrawState &draw_state = media_states_[i];
    if (!media_renderer_->Draw(draw_state.media_state_, draw_state.surface_)) {
      ETRACE(
          "Failed to render the frame by VA, "
          "error: %s\n",
          PRINTERROR());
      break;
    }
  }
}

void CompositorThread::Ensure3DRenderer() {
  if (!gl_renderer_) {
    gl_renderer_.reset(Create3DRenderer());
    if (!gl_renderer_->Init()) {
      ETRACE("Failed to initialize OpenGL compositor %s", PRINTERROR());
      gl_renderer_.reset(nullptr);
    }
  }
}

void CompositorThread::EnsureMediaRenderer() {
  if (!media_renderer_) {
    media_renderer_.reset(CreateMediaRenderer());
    if (!media_renderer_->Init(plane_manager_->GetGpuFd())) {
      ETRACE("Failed to initialize Media Renderer %s", PRINTERROR());
      media_renderer_.reset(nullptr);
    }
  }
}

}  // namespace hwcomposer
