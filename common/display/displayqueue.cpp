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

#include "displayqueue.h"

#include <hwcdefs.h>
#include <hwclayer.h>
#include <nativebufferhandler.h>

#include <vector>

#include "displayplanemanager.h"
#include "hwctrace.h"
#include "overlaylayer.h"
#include "pageflipeventhandler.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, uint32_t crtc_id)
    : HWCThread(-8, "DisplayQueue"),
      frame_(0),
      dpms_prop_(0),
      out_fence_ptr_prop_(0),
      active_prop_(0),
      mode_id_prop_(0),
      crtc_id_(crtc_id),
      connector_(0),
      crtc_prop_(0),
      blob_id_(0),
      old_blob_id_(0),
      gpu_fd_(gpu_fd) {
  compositor_.Init();
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC));
  GetDrmObjectProperty("ACTIVE", crtc_props, &active_prop_);
  GetDrmObjectProperty("MODE_ID", crtc_props, &mode_id_prop_);
#ifndef DISABLE_EXPLICIT_SYNC
  GetDrmObjectProperty("OUT_FENCE_PTR", crtc_props, &out_fence_ptr_prop_);
#endif
  memset(&mode_, 0, sizeof(mode_));
}

DisplayQueue::~DisplayQueue() {
  if (blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, blob_id_);

  if (old_blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);
}

bool DisplayQueue::Initialize(uint32_t width, uint32_t height, uint32_t pipe,
                              uint32_t connector,
                              const drmModeModeInfo& mode_info,
                              NativeBufferHandler* buffer_handler) {
  ScopedSpinLock lock(spin_lock_);
  frame_ = 0;
  previous_layers_.clear();
  previous_plane_state_.clear();
  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, pipe, crtc_id_));

  if (!display_plane_manager_->Initialize(buffer_handler, width, height)) {
    ETRACE("Failed to initialize DisplayQueue Manager.");
    return false;
  }

  connector_ = connector;
  mode_ = mode_info;

  ScopedDrmObjectPropertyPtr connector_props(drmModeObjectGetProperties(
      gpu_fd_, connector_, DRM_MODE_OBJECT_CONNECTOR));
  if (!connector_props) {
    ETRACE("Unable to get connector properties.");
    return false;
  }

  GetDrmObjectProperty("DPMS", connector_props, &dpms_prop_);
  GetDrmObjectProperty("CRTC_ID", connector_props, &crtc_prop_);

  return true;
}

bool DisplayQueue::GetFence(drmModeAtomicReqPtr property_set,
                            uint64_t* out_fence) {
#ifndef DISABLE_EXPLICIT_SYNC
  if (out_fence_ptr_prop_ != 0) {
    int ret = drmModeAtomicAddProperty(
        property_set, crtc_id_, out_fence_ptr_prop_, (uintptr_t)out_fence);
    if (ret < 0) {
      ETRACE("Failed to add OUT_FENCE_PTR property to pset: %d", ret);
      return false;
    }
  }
#else
  *out_fence = 0;
#endif

  return true;
}

bool DisplayQueue::ApplyPendingModeset(drmModeAtomicReqPtr property_set) {
  if (old_blob_id_) {
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);
    old_blob_id_ = 0;
  }

  needs_modeset_ = false;

  drmModeCreatePropertyBlob(gpu_fd_, &mode_, sizeof(drmModeModeInfo),
                            &blob_id_);
  if (blob_id_ == 0)
    return false;

  bool active = true;

  int ret = drmModeAtomicAddProperty(property_set, crtc_id_, mode_id_prop_,
                                     blob_id_) < 0 ||
            drmModeAtomicAddProperty(property_set, connector_, crtc_prop_,
                                     crtc_id_) < 0 ||
            drmModeAtomicAddProperty(property_set, crtc_id_, active_prop_,
                                     active) < 0;
  if (ret) {
    ETRACE("Failed to add blob %d to pset", blob_id_);
    return false;
  }

  old_blob_id_ = blob_id_;
  blob_id_ = 0;

  return true;
}

bool DisplayQueue::SetPowerMode(uint32_t power_mode) {
  switch (power_mode) {
    case kOff:
      Exit();
      break;
    case kDoze:
      Flush();
      Exit();
      break;
    case kDozeSuspend:
      Flush();
      break;
    case kOn:
      needs_modeset_ = true;
      drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                                  DRM_MODE_DPMS_ON);
      if (!InitWorker()) {
        ETRACE("Failed to initalize thread for DisplayQueue. %s", PRINTERROR());
        return false;
      }
      break;
    default:
      break;
  }

  return true;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers) {
  CTRACE();
  ScopedSpinLock lock(display_queue_);
  queue_.emplace();
  DisplayQueueItem& queue_item = queue_.back();
  size_t size = source_layers.size();

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    queue_item.layers_.emplace_back();
    OverlayLayer& overlay_layer = queue_item.layers_.back();
    overlay_layer.SetNativeHandle(layer->GetNativeHandle());
    overlay_layer.SetTransform(layer->GetTransform());
    overlay_layer.SetAlpha(layer->GetAlpha());
    overlay_layer.SetBlending(layer->GetBlending());
    overlay_layer.SetSourceCrop(layer->GetSourceCrop());
    overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    overlay_layer.SetIndex(layer_index);
    overlay_layer.SetAcquireFence(layer->acquire_fence.Release());
    queue_item.layers_rects_.emplace_back(layer->GetDisplayFrame());
    int ret = layer->release_fence.Reset(overlay_layer.GetReleaseFence());
    if (ret < 0)
      ETRACE("Failed to create fence for layer, error: %s", PRINTERROR());
  }

  Resume();
  return true;
}

void DisplayQueue::GetNextQueueItem(DisplayQueueItem& item) {
  DisplayQueueItem& queue_item = queue_.front();
  item.layers_.swap(queue_item.layers_);
  item.layers_rects_.swap(queue_item.layers_rects_);
  queue_.pop();
}

void DisplayQueue::HandleUpdateRequest(DisplayQueueItem& queue_item) {
  CTRACE();

  ScopedSpinLock lock(spin_lock_);
  // Reset any DisplayQueue Manager and Compositor state.
  if (!display_plane_manager_->BeginFrameUpdate(&queue_item.layers_)) {
    ETRACE("Failed to import needed buffers in DisplayQueueManager.");
    return;
  }

  uint32_t flags = 0;
  if (needs_modeset_) {
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  } else {
#ifdef DISABLE_OVERLAY_USAGE
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
#else
    flags |= DRM_MODE_ATOMIC_NONBLOCK;
#endif
  }

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  // Validate Overlays and Layers usage.
  std::tie(render_layers, current_composition_planes) =
      display_plane_manager_->ValidateLayers(
          &queue_item.layers_, previous_layers_, previous_plane_state_,
          needs_modeset_);

  DUMP_CURRENT_COMPOSITION_PLANES();

  if (!compositor_.BeginFrame()) {
    ETRACE("Failed to initialize compositor.");
    return;
  }

  if (render_layers) {
    // Prepare for final composition.
    if (!compositor_.Draw(current_composition_planes, queue_item.layers_,
                          queue_item.layers_rects_)) {
      ETRACE("Failed to prepare for the frame composition. ");
      return;
    }
  }

  // Do the actual commit.
  bool succesful_commit = true;
  uint64_t fence = 0;
  // Do the actual commit.
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());

  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return;
  }

  if (needs_modeset_) {
    if (!ApplyPendingModeset(pset.get())) {
      ETRACE("Failed to Modeset.");
      return;
    }
  } else {
    GetFence(pset.get(), &fence);
  }

  if (!display_plane_manager_->CommitFrame(current_composition_planes,
                                           pset.get(), flags)) {
    succesful_commit = false;
  } else {
    display_plane_manager_->EndFrameUpdate();
    previous_layers_.swap(queue_item.layers_);
    previous_plane_state_.swap(current_composition_planes);
  }

  if (!succesful_commit || (flags & DRM_MODE_ATOMIC_ALLOW_MODESET))
    return;

#ifdef DISABLE_EXPLICIT_SYNC
  compositor_.InsertFence(fence);
#else
  if (fence > 0) {
    compositor_.InsertFence(dup(fence));
    fd_handler_.AddFd(fence);
    out_fence_.Reset(fence);
  }
#endif
}

void DisplayQueue::CommitFinished() {
  fd_handler_.RemoveFd(out_fence_.get());
  out_fence_.Reset(-1);
}

void DisplayQueue::ProcessRequests() {
  display_queue_.lock();
  size_t size = queue_.size();

  if (size <= 0) {
    display_queue_.unlock();
    return;
  }

  DisplayQueueItem item;

  GetNextQueueItem(item);
  display_queue_.unlock();

  HandleUpdateRequest(item);
}

void DisplayQueue::HandleRoutine() {
  // If we have a commit pending and the out_fence_ is ready, we can process
  // the end of the last commit.
  int fd = out_fence_.get();
  if (fd > 0 && fd_handler_.IsReady(fd))
    CommitFinished();

  // Do not submit another commit while there is one still pending.
  if (out_fence_.get() > 0)
    return;

  // Check whether there are more requests to process, and commit the first
  // one.
  ProcessRequests();
}

void DisplayQueue::Flush() {
  ScopedSpinLock lock(display_queue_);

  while (queue_.size()) {
    DisplayQueueItem item;

    GetNextQueueItem(item);
    HandleUpdateRequest(item);
  }
}

void DisplayQueue::HandleExit() {
  ScopedSpinLocks lock(spin_lock_, display_queue_);
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());
  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return;
  }

  bool active = false;
  int ret =
      drmModeAtomicAddProperty(pset.get(), crtc_id_, active_prop_, active) < 0;
  if (ret) {
    ETRACE("Failed to set display to inactive");
    return;
  }
  display_plane_manager_->DisablePipe(pset.get());
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_OFF);
  previous_layers_.clear();
  previous_plane_state_.clear();
  std::queue<DisplayQueueItem>().swap(queue_);
  current_sync_.reset(nullptr);
  compositor_.Reset();
}

void DisplayQueue::GetDrmObjectProperty(const char* name,
                                        const ScopedDrmObjectPropertyPtr& props,
                                        uint32_t* id) const {
  uint32_t count_props = props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(gpu_fd_, props->props[i]));
    if (property && !strcmp(property->name, name)) {
      *id = property->prop_id;
      break;
    }
  }
  if (!(*id))
    ETRACE("Could not find property %s", name);
}

}  // namespace hwcomposer
