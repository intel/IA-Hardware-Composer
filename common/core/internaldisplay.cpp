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

#include <internaldisplay.h>

#include <hwcdefs.h>
#include <hwclayer.h>
#include <hwctrace.h>

#include "displayplanemanager.h"
#include "nativesync.h"
#include "overlaylayer.h"

namespace hwcomposer {

static const int32_t kUmPerInch = 25400;

InternalDisplay::InternalDisplay(uint32_t gpu_fd,
                                 NativeBufferHandler &buffer_handler,
                                 uint32_t pipe_id, uint32_t crtc_id)
    : buffer_handler_(buffer_handler),
      crtc_id_(crtc_id),
      pipe_(pipe_id),
      connector_(0),
      blob_id_(0),
      old_blob_id_(0),
      gpu_fd_(gpu_fd),
      is_connected_(false),
      is_powered_off_(true) {
}

InternalDisplay::~InternalDisplay() {
  if (blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, blob_id_);

  if (old_blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);
}

bool InternalDisplay::Initialize() {
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC));
  GetDrmObjectProperty("ACTIVE", crtc_props, &active_prop_);
  GetDrmObjectProperty("MODE_ID", crtc_props, &mode_id_prop_);
#ifndef DISABLE_EXPLICIT_SYNC
  GetDrmObjectProperty("OUT_FENCE_PTR", crtc_props, &out_fence_ptr_prop_);
#endif
  frame_ = 0;
  flip_handler_.reset(new PageFlipEventHandler());

  return true;
}

bool InternalDisplay::Connect(const drmModeModeInfo &mode_info,
                              const drmModeConnector *connector) {
  IHOTPLUGEVENTTRACE("InternalDisplay::Connect recieved.");
  // TODO(kalyan): Add support for multi monitor case.
  if (connector->connector_id == connector_ && !is_powered_off_) {
    IHOTPLUGEVENTTRACE("Display is already connected to this connector.");
    is_connected_ = true;
    return true;
  }
  ScopedSpinLock lock(spin_lock_);
  IHOTPLUGEVENTTRACE("Display is being connected to a new connector.");
  mode_ = mode_info;
  connector_ = connector->connector_id;
  width_ = mode_.hdisplay;
  height_ = mode_.vdisplay;
  refresh_ = (mode_.clock * 1000.0f) / (mode_.htotal * mode_.vtotal);
  dpix_ = connector->mmWidth ? (width_ * kUmPerInch) / connector->mmWidth : -1;
  dpiy_ =
      connector->mmHeight ? (height_ * kUmPerInch) / connector->mmHeight : -1;

  ScopedDrmObjectPropertyPtr connector_props(drmModeObjectGetProperties(
      gpu_fd_, connector_, DRM_MODE_OBJECT_CONNECTOR));
  if (!connector_props) {
    ETRACE("Unable to get connector properties.");
    return false;
  }

  GetDrmObjectProperty("DPMS", connector_props, &dpms_prop_);
  GetDrmObjectProperty("CRTC_ID", connector_props, &crtc_prop_);
  is_powered_off_ = false;
  is_connected_ = true;
  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, pipe_, crtc_id_));

  if (!display_plane_manager_->Initialize()) {
    ETRACE("Failed to initialize Display Manager.");
    return false;
  }

  compositor_.Init(&buffer_handler_, width_, height_, gpu_fd_);
  flip_handler_->Init(refresh_, gpu_fd_, pipe_);
  dpms_mode_ = DRM_MODE_DPMS_ON;
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_ON);
  pending_operations_ |= PendingModeset::kModeset;

  return true;
}

void InternalDisplay::DisConnect() {
  IHOTPLUGEVENTTRACE("InternalDisplay::DisConnect recieved.");
  is_connected_ = false;
}

void InternalDisplay::ShutDown() {
  if (is_powered_off_)
    return;
  ScopedSpinLock lock(spin_lock_);
  IHOTPLUGEVENTTRACE("InternalDisplay::ShutDown recieved.");
  is_powered_off_ = true;
  dpms_mode_ = DRM_MODE_DPMS_OFF;
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_OFF);

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
  display_plane_manager_.reset(nullptr);
}

bool InternalDisplay::GetDisplayAttribute(uint32_t /*config*/,
                                          HWCDisplayAttribute attribute,
                                          int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = width_;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = height_;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *value = 1e9/refresh_;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = dpix_;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value = dpiy_;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

bool InternalDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                        uint32_t *configs) {
  *num_configs = 1;
  if (!configs)
    return true;

  configs[0] = 1;

  return true;
}

bool InternalDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "InternalDisplay-" << connector_;
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length - 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

bool InternalDisplay::SetActiveConfig(uint32_t /*config*/) {
  return true;
}

bool InternalDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 1;
  return true;
}

bool InternalDisplay::SetDpmsMode(uint32_t dpms_mode) {
  ScopedSpinLock lock(spin_lock_);
  if (dpms_mode_ == dpms_mode)
    return true;

  dpms_mode_ = dpms_mode;
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              dpms_mode);
  return true;
}

bool InternalDisplay::ApplyPendingModeset(drmModeAtomicReqPtr property_set,
                                          NativeSync *sync,
                                          uint64_t *out_fence) {
  if (pending_operations_ & kModeset) {
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

    pending_operations_ &= ~kModeset;
    old_blob_id_ = blob_id_;
    blob_id_ = 0;
  } else {
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
    *out_fence = sync->CreateNextTimelineFence();
#endif
  }

  return true;
}

void InternalDisplay::GetDrmObjectProperty(
    const char *name, const ScopedDrmObjectPropertyPtr &props,
    uint32_t *id) const {
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

bool InternalDisplay::Present(
    std::vector<hwcomposer::HwcLayer *> &source_layers) {
  CTRACE();
  ScopedSpinLock lock(spin_lock_);
  if (is_powered_off_) {
    IHOTPLUGEVENTTRACE("Trying to update an Disconnected Display.");
    return false;
  }

  bool needs_modeset = pending_operations_ & kModeset;
  // Create a Sync object for this Composition.
  std::unique_ptr<NativeSync> sync_object(new NativeSync());
  if (!sync_object->Init()) {
    ETRACE("Failed to create sync object.");
    return false;
  }

  std::vector<OverlayLayer> layers;
  std::vector<HwcRect<int>> layers_rects;

  int ret = 0;
  size_t size = source_layers.size();
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer *layer = source_layers.at(layer_index);
    layers.emplace_back();
    OverlayLayer &overlay_layer = layers.back();
    overlay_layer.SetNativeHandle(layer->GetNativeHandle());
    overlay_layer.SetTransform(layer->GetTransform());
    overlay_layer.SetAlpha(layer->GetAlpha());
    overlay_layer.SetBlending(layer->GetBlending());
    overlay_layer.SetSourceCrop(layer->GetSourceCrop());
    overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    overlay_layer.SetIndex(layer_index);
    overlay_layer.SetAcquireFence(layer->acquire_fence.Release());
    overlay_layer.SetReleaseFence(layer->release_fence.Release());
    layers_rects.emplace_back(layer->GetDisplayFrame());
  }

  // Reset any Display Manager and Compositor state.
  if (!display_plane_manager_->BeginFrameUpdate(layers, &buffer_handler_)) {
    ETRACE("Failed to import needed buffers in DisplayManager.");
    return false;
  }

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  // Validate Overlays and Layers usage.
  std::tie(render_layers, current_composition_planes) =
      display_plane_manager_->ValidateLayers(layers, needs_modeset);

  DUMP_CURRENT_COMPOSITION_PLANES();

  if (!compositor_.BeginFrame()) {
    ETRACE("Failed to initialize compositor.");
    return false;
  }

  if (render_layers) {
    // Prepare for final composition.
    if (!compositor_.Draw(current_composition_planes, layers, layers_rects)) {
      ETRACE("Failed to prepare for the frame composition ret=%d", ret);
      return false;
    }
  }

  // Do the actual commit.
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());

  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer *layer = source_layers.at(layer_index);
    int ret =
        layer->release_fence.Reset(sync_object->CreateNextTimelineFence());
    if (ret < 0)
      ETRACE("Failed to create fence for layer, error: %s", PRINTERROR());
  }

  uint64_t fence = 0;
  if (!ApplyPendingModeset(pset.get(), sync_object.get(), &fence)) {
    ETRACE("Failed to Modeset");
    return false;
  }

  bool succesful_commit = true;

  if (!display_plane_manager_->CommitFrame(current_composition_planes,
                                           pset.get(), needs_modeset,
                                           sync_object, out_fence_)) {
    succesful_commit = false;
  } else {
    display_plane_manager_->EndFrameUpdate();
  }

  if (render_layers)
    compositor_.EndFrame(succesful_commit);

  if (!succesful_commit || needs_modeset) {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      HwcLayer *layer = source_layers.at(layer_index);
      layer->release_fence.Reset(-1);
    }
    return succesful_commit;
  } else {
    compositor_.InsertFence(dup(fence));
  }

  if (fence > 0)
    out_fence_.Reset(fence);

  return true;
}

int InternalDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> callback, uint32_t display_id) {
  return flip_handler_->RegisterCallback(callback, display_id);
}

void InternalDisplay::VSyncControl(bool enabled) {
  flip_handler_->VSyncControl(enabled);
}

}  // namespace hwcomposer
