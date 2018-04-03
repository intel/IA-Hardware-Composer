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

#include "displayplanemanager.h"
#include "framebuffermanager.h"
#include "hwctrace.h"
#include "hwcutils.h"
#include "nativegpuresource.h"
#include "nativesurface.h"
#include "overlaylayer.h"
#include "renderer.h"
#include "resourcemanager.h"

#include <nativebufferhandler.h>

namespace hwcomposer {

CompositorThread::CompositorThread() : HWCThread(-8, "CompositorThread") {
  if (!cevent_.Initialize())
    return;

  fd_chandler_.AddFd(cevent_.get_fd());
}

CompositorThread::~CompositorThread() {
}

void CompositorThread::Initialize(ResourceManager *resource_manager,
                                  uint32_t gpu_fd,
                                  FrameBufferManager *frame_buffer_manager) {
  fb_manager_ = frame_buffer_manager;
  tasks_lock_.lock();
  if (!gpu_resource_handler_)
    gpu_resource_handler_.reset(CreateNativeGpuResourceHandler());

  resource_manager_ = resource_manager;
  gpu_fd_ = gpu_fd;
  tasks_lock_.unlock();
  if (!InitWorker()) {
    ETRACE("Failed to initalize CompositorThread. %s", PRINTERROR());
  }
}

void CompositorThread::SetExplicitSyncSupport(bool disable_explicit_sync) {
  disable_explicit_sync_ = disable_explicit_sync;
}

void CompositorThread::FreeResources() {
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

bool CompositorThread::Draw(std::vector<DrawState> &states,
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

  // We start of assuming that the draw calls
  // succeed.
  draw_succeeded_ = true;
  tasks_lock_.unlock();

  // Adding check to avoid waiting in this
  // thread in certain corner case.
  if (states_.empty() && media_states_.empty()) {
    return draw_succeeded_;
  }

  Resume();
  Wait();
  return draw_succeeded_;
}

void CompositorThread::ExitThread() {
  HWCThread::Exit();
  std::vector<DrawState>().swap(states_);
  std::vector<OverlayBuffer *>().swap(buffers_);
}

void CompositorThread::HandleExit() {
  HandleReleaseRequest();
  gl_renderer_.reset(nullptr);
  gpu_resource_handler_.reset(nullptr);
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

  std::vector<ResourceHandle> purged_gl_resources;
  std::vector<MediaResourceHandle> purged_media_resources;
  bool has_gpu_resource = false;
  resource_manager_->GetPurgedResources(
      purged_gl_resources, purged_media_resources, &has_gpu_resource);
  size_t purged_size = purged_gl_resources.size();

  if (purged_size != 0) {
    if (has_gpu_resource) {
      Ensure3DRenderer();
      gpu_resource_handler_->ReleaseGPUResources(purged_gl_resources);
    }

    const NativeBufferHandler *handler =
        resource_manager_->GetNativeBufferHandler();

    for (size_t i = 0; i < purged_size; i++) {
      const ResourceHandle &handle = purged_gl_resources.at(i);
      if (!handle.handle_) {
        continue;
      }

      fb_manager_->RemoveFB(handle.handle_->meta_data_.num_planes_,
                            handle.handle_->meta_data_.gem_handles_);

      handler->ReleaseBuffer(handle.handle_);
      handler->DestroyHandle(handle.handle_);
    }
  }

  purged_size = purged_media_resources.size();

  if (purged_size != 0) {
    EnsureMediaRenderer();
    media_renderer_->DestroyMediaResources(purged_media_resources);

    const NativeBufferHandler *handler =
        resource_manager_->GetNativeBufferHandler();

    for (size_t i = 0; i < purged_size; i++) {
      const MediaResourceHandle &handle = purged_media_resources.at(i);
      if (!handle.handle_) {
        continue;
      }

      fb_manager_->RemoveFB(handle.handle_->meta_data_.num_planes_,
                            handle.handle_->meta_data_.gem_handles_);
      handler->ReleaseBuffer(handle.handle_);
      handler->DestroyHandle(handle.handle_);
    }
  }
}

void CompositorThread::Handle3DDrawRequest() {
  tasks_lock_.lock();
  tasks_ &= ~kRender3D;
  tasks_lock_.unlock();

  Ensure3DRenderer();
  if (!gl_renderer_) {
    draw_succeeded_ = false;
    return;
  }

  gl_renderer_->SetExplicitSyncSupport(disable_explicit_sync_);

  if (!gpu_resource_handler_->PrepareResources(buffers_)) {
    ETRACE(
        "Failed to prepare GPU resources for compositing the frame, "
        "error: %s",
        PRINTERROR());
    draw_succeeded_ = false;
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

    if (!gl_renderer_->Draw(draw_state.states_, draw_state.surface_)) {
      ETRACE(
          "Failed to Draw: "
          "error: %s",
          PRINTERROR());
      draw_succeeded_ = false;
      break;
    }

    if (draw_state.destroy_surface_) {
      if (draw_succeeded_) {
        draw_state.retire_fence_ =
            draw_state.surface_->GetLayer()->ReleaseAcquireFence();
      }

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
    draw_succeeded_ = false;
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
      draw_succeeded_ = false;
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
    if (!media_renderer_->Init(gpu_fd_)) {
      ETRACE("Failed to initialize Media Renderer %s", PRINTERROR());
      media_renderer_.reset(nullptr);
    }
  }
}

}  // namespace hwcomposer
