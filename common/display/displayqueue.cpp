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
#include <hwctrace.h>

#include <nativebufferhandler.h>

#include "displayplanemanager.h"
#include "overlaylayer.h"
#include "pageflipeventhandler.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, uint32_t crtc_id)
    : HWCThread(-8, "DisplayQueue"),
      crtc_id_(crtc_id),
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

  dpms_mode_ = DRM_MODE_DPMS_ON;
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_ON);

  needs_modeset_ = true;
  lock.Reset();
  if (!InitWorker()) {
    ETRACE("Failed to initalize thread for DisplayQueue. %s", PRINTERROR());
    return false;
  }

  return true;
}

void DisplayQueue::Exit() {
  IHOTPLUGEVENTTRACE("DisplayQueue::Exit recieved.");
  HWCThread::Exit();
}

bool DisplayQueue::GetFence(ScopedDrmAtomicReqPtr& property_set,
                            uint64_t* out_fence) {
#ifndef DISABLE_EXPLICIT_SYNC
  if (out_fence_ptr_prop_ != 0) {
    int ret =
        drmModeAtomicAddProperty(property_set.get(), crtc_id_,
                                 out_fence_ptr_prop_, (uintptr_t)out_fence);
    if (ret < 0) {
      ETRACE("Failed to add OUT_FENCE_PTR property to pset: %d", ret);
      return false;
    }
  }
#else
  *out_fence = -1;
#endif

  return true;
}

bool DisplayQueue::ApplyPendingModeset(drmModeAtomicReqPtr property_set) {
  if (old_blob_id_) {
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);
    old_blob_id_ = 0;
  }

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

bool DisplayQueue::SetDpmsMode(uint32_t dpms_mode) {
  ScopedSpinLock lock(spin_lock_);
  if (dpms_mode_ == dpms_mode)
    return true;

  dpms_mode_ = dpms_mode;
  if (dpms_mode_ == DRM_MODE_DPMS_OFF) {
    Exit();
    return true;
  }

  if (dpms_mode_ == DRM_MODE_DPMS_ON) {
    needs_modeset_ = true;
    if (!InitWorker()) {
      ETRACE("Failed to initalize thread for DisplayQueue. %s", PRINTERROR());
      return false;
    }
  }

  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_, dpms_mode);

  return true;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers) {
  CTRACE();
  ScopedSpinLock lock(display_queue_);
  if (!display_plane_manager_) {
    IHOTPLUGEVENTTRACE("Trying to update an Disconnected Display.");
    return false;
  }

  queue_.emplace();
  DisplayQueueItem& queue_item = queue_.back();
  // Create a Sync object for this Composition.
  queue_item.sync_object_.reset(new NativeSync());
  if (!queue_item.sync_object_->Init()) {
    ETRACE("Failed to create sync object.");
    return false;
  }

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
    int ret = layer->release_fence.Reset(
        queue_item.sync_object_->CreateNextTimelineFence());
    if (ret < 0)
      ETRACE("Failed to create fence for layer, error: %s", PRINTERROR());
  }

  Resume();
  return true;
}

void DisplayQueue::HandleUpdateRequest(DisplayQueueItem& queue_item) {
  CTRACE();
  ScopedSpinLock lock(spin_lock_);
  // Reset any DisplayQueue Manager and Compositor state.
  if (!display_plane_manager_->BeginFrameUpdate(queue_item.layers_)) {
    ETRACE("Failed to import needed buffers in DisplayQueueManager.");
    return;
  }

  bool needs_modeset = needs_modeset_;
  needs_modeset_ = false;

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  // Validate Overlays and Layers usage.
  std::tie(render_layers, current_composition_planes) =
      display_plane_manager_->ValidateLayers(
          queue_item.layers_, previous_layers_, previous_plane_state_,
          needs_modeset);

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

  if (needs_modeset && !ApplyPendingModeset(pset.get())) {
    ETRACE("Failed to Modeset.");
    return;
  }

  GetFence(pset, &fence);
  if (!display_plane_manager_->CommitFrame(
          current_composition_planes, pset.get(), needs_modeset,
          queue_item.sync_object_, out_fence_)) {
    succesful_commit = false;
  } else {
    display_plane_manager_->EndFrameUpdate();
    previous_layers_.swap(queue_item.layers_);
    previous_plane_state_.swap(current_composition_planes);
  }

  if (!succesful_commit || needs_modeset)
    return;
#ifndef DISABLE_EXPLICIT_SYNC
  compositor_.InsertFence(dup(fence));
#else
  compositor_.InsertFence(fence);
#endif

  if (fence > 0)
    out_fence_.Reset(fence);
}

void DisplayQueue::HandleRoutine() {
  display_queue_.lock();
  size_t size = queue_.size();

  if (size <= 0) {
    display_queue_.unlock();
    ConditionalSuspend();
    return;
  }

  DisplayQueueItem& queue_item = queue_.front();
  DisplayQueueItem item;
  item.layers_.swap(queue_item.layers_);
  item.layers_rects_.swap(queue_item.layers_rects_);
  item.sync_object_.reset(queue_item.sync_object_.release());
  queue_.pop();
  display_queue_.unlock();

  HandleUpdateRequest(item);
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
  dpms_mode_ = DRM_MODE_DPMS_OFF;
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_OFF);
  previous_layers_.clear();
  previous_plane_state_.clear();
  display_plane_manager_.reset(nullptr);
  std::queue<DisplayQueueItem>().swap(queue_);
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
