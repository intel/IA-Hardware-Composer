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

#include <vector>

#include "displayplanemanager.h"
#include "hwctrace.h"
#include "overlaylayer.h"
#include "vblankeventhandler.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, uint32_t crtc_id,
                           OverlayBufferManager* buffer_manager)
    : frame_(0),
      dpms_prop_(0),
      out_fence_ptr_prop_(0),
      active_prop_(0),
      mode_id_prop_(0),
      crtc_id_(crtc_id),
      connector_(0),
      crtc_prop_(0),
      blob_id_(0),
      old_blob_id_(0),
      gpu_fd_(gpu_fd),
      buffer_manager_(buffer_manager) {
  compositor_.Init();
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC));
  GetDrmObjectProperty("ACTIVE", crtc_props, &active_prop_);
  GetDrmObjectProperty("MODE_ID", crtc_props, &mode_id_prop_);
#ifndef DISABLE_EXPLICIT_SYNC
  GetDrmObjectProperty("OUT_FENCE_PTR", crtc_props, &out_fence_ptr_prop_);
#endif
  memset(&mode_, 0, sizeof(mode_));
  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, crtc_id_, buffer_manager_));

  kms_fence_handler_.reset(new KMSFenceEventHandler(buffer_manager_));
}

DisplayQueue::~DisplayQueue() {
  if (blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, blob_id_);

  if (old_blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);
}

bool DisplayQueue::Initialize(uint32_t width, uint32_t height, uint32_t pipe,
                              uint32_t connector,
                              const drmModeModeInfo& mode_info) {
  frame_ = 0;
  previous_layers_.clear();
  previous_plane_state_.clear();

  if (!display_plane_manager_->Initialize(pipe, width, height)) {
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
      HandleExit();
      break;
    case kDoze:
      HandleExit();
      break;
    case kDozeSuspend:
      break;
    case kOn:
      needs_modeset_ = true;
      drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                                  DRM_MODE_DPMS_ON);

      if (!kms_fence_handler_->Initialize())
        return false;
      break;
    default:
      break;
  }

  return true;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers) {
  CTRACE();
  size_t size = source_layers.size();
  std::vector<OverlayLayer> layers;
  std::vector<HwcRect<int>> layers_rects;

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    layers.emplace_back();
    OverlayLayer& overlay_layer = layers.back();
    overlay_layer.SetNativeHandle(layer->GetNativeHandle());
    overlay_layer.SetTransform(layer->GetTransform());
    overlay_layer.SetAlpha(layer->GetAlpha());
    overlay_layer.SetBlending(layer->GetBlending());
    overlay_layer.SetSourceCrop(layer->GetSourceCrop());
    overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    overlay_layer.SetIndex(layer_index);
    overlay_layer.SetAcquireFence(layer->acquire_fence.Release());
    layers_rects.emplace_back(layer->GetDisplayFrame());
    ImportedBuffer* buffer = buffer_manager_->CreateBufferFromNativeHandle(
	layer->GetNativeHandle());
    overlay_layer.SetBuffer(buffer);
    int ret = layer->release_fence.Reset(overlay_layer.GetReleaseFence());
    if (ret < 0)
      ETRACE("Failed to create fence for layer, error: %s", PRINTERROR());
  }

  // Reset any DisplayQueue Manager and Compositor state.
  display_plane_manager_->BeginFrameUpdate();

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
	  &layers, previous_layers_, previous_plane_state_,
          needs_modeset_);

  DUMP_CURRENT_COMPOSITION_PLANES();

  if (!compositor_.BeginFrame()) {
    ETRACE("Failed to initialize compositor.");
    return false;
  }

  if (render_layers) {
    // Prepare for final composition.
    if (!compositor_.Draw(current_composition_planes, layers,
			  layers_rects)) {
      ETRACE("Failed to prepare for the frame composition. ");
      return false;
    }
  }

  buffer_manager_->SignalBuffersIfReady(layers);

  uint64_t fence = 0;
  // Do the actual commit.
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());

  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  if (needs_modeset_) {
    if (!ApplyPendingModeset(pset.get())) {
      ETRACE("Failed to Modeset.");
      return false;
    }
  } else {
    GetFence(pset.get(), &fence);
  }

  kms_fence_handler_->EnsureReadyForNextFrame();

  if (!display_plane_manager_->CommitFrame(current_composition_planes,
                                           pset.get(), flags)) {
    ETRACE("Failed to Commit layers.");
    return false;
  }

  display_plane_manager_->EndFrameUpdate();

#ifdef DISABLE_EXPLICIT_SYNC
  compositor_.InsertFence(fence);
  buffer_manager_->UnRegisterLayerBuffers(previous_layers_);
#else
  if (fence > 0) {
    compositor_.InsertFence(dup(fence));
    kms_fence_handler_->WaitFence(fence, previous_layers_);
  }
#endif
  previous_layers_.swap(layers);
  previous_plane_state_.swap(current_composition_planes);
  return true;
}

void DisplayQueue::HandleExit() {
  kms_fence_handler_->ExitThread();

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
  std::vector<OverlayLayer>().swap(previous_layers_);
  previous_plane_state_.clear();
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

bool DisplayQueue::CheckPlaneFormat(uint32_t format) {
  return display_plane_manager_->CheckPlaneFormat(format);
}

}  // namespace hwcomposer
