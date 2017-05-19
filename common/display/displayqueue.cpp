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

#include <math.h>
#include <hwcdefs.h>
#include <hwclayer.h>

#include <vector>

#include "displayplanemanager.h"
#include "hwctrace.h"
#include "overlaylayer.h"
#include "vblankeventhandler.h"
#include "nativesurface.h"

namespace hwcomposer {

DisplayQueue::DisplayQueue(uint32_t gpu_fd, uint32_t crtc_id,
                           OverlayBufferManager* buffer_manager)
    : frame_(0),
      dpms_prop_(0),
      out_fence_ptr_prop_(0),
      active_prop_(0),
      mode_id_prop_(0),
      lut_id_prop_(0),
      crtc_id_(crtc_id),
      connector_(0),
      crtc_prop_(0),
      blob_id_(0),
      old_blob_id_(0),
      gpu_fd_(gpu_fd),
      lut_size_(0),
      broadcastrgb_id_(0),
      broadcastrgb_full_(-1),
      broadcastrgb_automatic_(-1),
      buffer_manager_(buffer_manager) {
  compositor_.Init();
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC));
  GetDrmObjectProperty("ACTIVE", crtc_props, &active_prop_);
  GetDrmObjectProperty("MODE_ID", crtc_props, &mode_id_prop_);
  GetDrmObjectProperty("GAMMA_LUT", crtc_props, &lut_id_prop_);
  GetDrmObjectPropertyValue("GAMMA_LUT_SIZE", crtc_props, &lut_size_);
  GetDrmObjectProperty("OUT_FENCE_PTR", crtc_props, &out_fence_ptr_prop_);
  disable_overlay_usage_ = out_fence_ptr_prop_ == 0;

  memset(&mode_, 0, sizeof(mode_));
  display_plane_manager_.reset(
      new DisplayPlaneManager(gpu_fd_, crtc_id_, buffer_manager_));

  kms_fence_handler_.reset(new KMSFenceEventHandler(this));
  /* use 0x80 as default brightness for all colors */
  brightness_ = 0x808080;
  /* use 0x80 as default brightness for all colors */
  contrast_ = 0x808080;
  /* use 1 as default gamma value */
  gamma_.red = 1;
  gamma_.green = 1;
  gamma_.blue = 1;
  needs_color_correction_ = true;
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
  GetDrmObjectProperty("Broadcast RGB", connector_props, &broadcastrgb_id_);

  drmModePropertyPtr broadcastrgb_props =
      drmModeGetProperty(gpu_fd_, broadcastrgb_id_);

  // This is a valid case on DSI panels.
  if (!broadcastrgb_props)
    return true;

  if (!(broadcastrgb_props->flags & DRM_MODE_PROP_ENUM))
    return false;

  for (int i = 0; i < broadcastrgb_props->count_enums; i++) {
    if (!strcmp(broadcastrgb_props->enums[i].name, "Full")) {
      broadcastrgb_full_ = broadcastrgb_props->enums[i].value;
    } else if (!strcmp(broadcastrgb_props->enums[i].name, "Automatic")) {
      broadcastrgb_automatic_ = broadcastrgb_props->enums[i].value;
    }
  }
  drmModeFreeProperty(broadcastrgb_props);

  return true;
}

bool DisplayQueue::GetFence(drmModeAtomicReqPtr property_set,
                            uint64_t* out_fence) {
  int ret = drmModeAtomicAddProperty(property_set, crtc_id_,
                                     out_fence_ptr_prop_, (uintptr_t)out_fence);
  if (ret < 0) {
    ETRACE("Failed to add OUT_FENCE_PTR property to pset: %d", ret);
    return false;
  }

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
      needs_color_correction_ = true;
      flags_ = DRM_MODE_ATOMIC_ALLOW_MODESET;
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

void DisplayQueue::GetCachedLayers(const std::vector<OverlayLayer>& layers,
                                   DisplayPlaneStateList* composition,
                                   bool* render_layers) {
  CTRACE();
  bool needs_gpu_composition = false;
  for (const DisplayPlaneState& plane : previous_plane_state_) {
    bool region_changed = false;
    composition->emplace_back(plane.plane());
    DisplayPlaneState& last_plane = composition->back();
    last_plane.AddLayers(plane.source_layers(), plane.GetDisplayFrame(),
                         plane.GetCompositionState());

    if (plane.GetCompositionState() == DisplayPlaneState::State::kRender) {
      needs_gpu_composition = true;
      const std::vector<size_t>& source_layers = plane.source_layers();
      size_t layers_size = source_layers.size();
      for (size_t i = 0; i < layers_size; i++) {
        size_t source_index = source_layers.at(i);
        if (layers.at(source_index).HasLayerPositionChanged()) {
          region_changed = true;
          break;
        }
      }

      display_plane_manager_->EnsureOffScreenTarget(last_plane);
      if (!region_changed) {
        const std::vector<CompositionRegion>& comp_regions =
            plane.GetCompositionRegion();
        last_plane.GetCompositionRegion().assign(comp_regions.begin(),
                                                 comp_regions.end());
      }
    } else {
      const OverlayLayer* layer =
          &(*(layers.begin() + last_plane.source_layers().front()));
      layer->GetBuffer()->CreateFrameBuffer(gpu_fd_);
      last_plane.SetOverlayLayer(layer);
    }
  }

  *render_layers = needs_gpu_composition;
}

bool DisplayQueue::QueueUpdate(std::vector<HwcLayer*>& source_layers,
                               int32_t* retire_fence) {
  CTRACE();
  size_t size = source_layers.size();
  size_t previous_size = previous_layers_.size();
  std::vector<OverlayLayer> layers;
  std::vector<HwcRect<int>> layers_rects;
  bool layers_changed = false;
  spin_lock_.lock();
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer* layer = source_layers.at(layer_index);
    const HwcRegion& current_surface_damage = layer->GetSurfaceDamage();
    layers.emplace_back();
    OverlayLayer& overlay_layer = layers.back();
    overlay_layer.SetTransform(layer->GetTransform());
    overlay_layer.SetAlpha(layer->GetAlpha());
    overlay_layer.SetBlending(layer->GetBlending());
    overlay_layer.SetSourceCrop(layer->GetSourceCrop());
    overlay_layer.SetDisplayFrame(layer->GetDisplayFrame());
    overlay_layer.SetIndex(layer_index);
    overlay_layer.SetAcquireFence(layer->acquire_fence.Release());
    layers_rects.emplace_back(layer->GetDisplayFrame());
    ImportedBuffer* buffer =
        buffer_manager_->CreateBufferFromNativeHandle(layer->GetNativeHandle());
    overlay_layer.SetBuffer(buffer);
    int ret = layer->release_fence.Reset(overlay_layer.GetReleaseFence());
    if (ret < 0)
      ETRACE("Failed to create fence for layer, error: %s", PRINTERROR());

    if (!use_layer_cache_)
      continue;

    if (previous_size > layer_index) {
      overlay_layer.SetSurfaceDamage(current_surface_damage,
                                     previous_layers_.at(layer_index));
    }

    if (overlay_layer.HasLayerAttributesChanged()) {
      layers_changed = true;
    }
  }

  spin_lock_.unlock();

  if (!use_layer_cache_ || size != previous_size) {
    layers_changed = true;
  }

  if (needs_modeset_) {
    layers_changed = true;
    use_layer_cache_ = false;
  } else {
    use_layer_cache_ = true;
  }

  DisplayPlaneStateList current_composition_planes;
  bool render_layers;
  // Validate Overlays and Layers usage.
  if (!layers_changed) {
    GetCachedLayers(layers, &current_composition_planes, &render_layers);
  } else {
    std::tie(render_layers, current_composition_planes) =
        display_plane_manager_->ValidateLayers(layers, needs_modeset_,
                                               disable_overlay_usage_);
  }

  DUMP_CURRENT_COMPOSITION_PLANES();

  if (render_layers) {
      if (!compositor_.BeginFrame(disable_overlay_usage_)) {
	ETRACE("Failed to initialize compositor.");
	return false;
      }

    // Prepare for final composition.
    if (!compositor_.Draw(current_composition_planes, layers, layers_rects)) {
      ETRACE("Failed to prepare for the frame composition. ");
      return false;
    }
  }

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
  } else if (!disable_overlay_usage_) {
    GetFence(pset.get(), &fence);
  }

  if (needs_color_correction_) {
    SetColorCorrection(gamma_, contrast_, brightness_);
    needs_color_correction_ = false;
  }

  kms_fence_handler_->EnsureReadyForNextFrame();

  if (!display_plane_manager_->CommitFrame(current_composition_planes,
                                           pset.get(), flags_)) {
    ETRACE("Failed to Commit layers.");
    return false;
  }

  if (fence > 0) {
    if (render_layers)
      compositor_.InsertFence(dup(fence));
    *retire_fence = dup(fence);
    kms_fence_handler_->WaitFence(fence, previous_layers_);
  } else {
    // This is the best we can do in this case, flush any 3D
    // operations and release buffers of previous layers.
    if (render_layers)
      compositor_.InsertFence(fence);

    spin_lock_.lock();
    buffer_manager_->UnRegisterLayerBuffers(previous_layers_);
    spin_lock_.unlock();
    if (!disable_overlay_usage_) {
      flags_ = 0;
      flags_ |= DRM_MODE_ATOMIC_NONBLOCK;
    }

    needs_modeset_ = false;
  }

  for (NativeSurface* surface : in_flight_surfaces_) {
    surface->SetInUse(false);
  }

  previous_layers_.swap(layers);
  previous_plane_state_.swap(current_composition_planes);

  std::vector<NativeSurface*>().swap(in_flight_surfaces_);

  for (DisplayPlaneState& plane_state : previous_plane_state_) {
    if (plane_state.GetCompositionState() ==
        DisplayPlaneState::State::kRender) {
      in_flight_surfaces_.emplace_back(plane_state.GetOffScreenTarget());
    }
  }

  return true;
}

void DisplayQueue::HandleCommitUpdate(
    const std::vector<const OverlayBuffer*>& buffers) {
  spin_lock_.lock();
  buffer_manager_->UnRegisterBuffers(buffers);
  spin_lock_.unlock();
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

  std::vector<NativeSurface*>().swap(in_flight_surfaces_);
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

void DisplayQueue::GetDrmObjectPropertyValue(
    const char* name, const ScopedDrmObjectPropertyPtr& props,
    uint64_t* value) const {
  uint32_t count_props = props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(gpu_fd_, props->props[i]));
    if (property && !strcmp(property->name, name)) {
      *value = props->prop_values[i];
      break;
    }
  }
  if (!(*value))
    ETRACE("Could not find property value %s", name);
}

void DisplayQueue::ApplyPendingLUT(struct drm_color_lut* lut) const {
  if (lut_id_prop_ == 0)
    return;

  uint32_t lut_blob_id = 0;

  drmModeCreatePropertyBlob(
      gpu_fd_, lut, sizeof(struct drm_color_lut) * lut_size_, &lut_blob_id);
  if (lut_blob_id == 0) {
    return;
  }

  drmModeObjectSetProperty(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC,
                           lut_id_prop_, lut_blob_id);
  drmModeDestroyPropertyBlob(gpu_fd_, lut_blob_id);
}

void DisplayQueue::SetGamma(float red, float green, float blue) {
  gamma_.red = red;
  gamma_.green = green;
  gamma_.blue = blue;
  needs_color_correction_ = true;
}

void DisplayQueue::SetContrast(uint32_t red, uint32_t green, uint32_t blue) {
  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;
  contrast_ = (red << 16) | (green << 8) | (blue);
  needs_color_correction_ = true;
}

void DisplayQueue::SetBrightness(uint32_t red, uint32_t green, uint32_t blue) {
  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;
  brightness_ = (red << 16) | (green << 8) | (blue);
  needs_color_correction_ = true;
}

float DisplayQueue::TransformContrastBrightness(float value, float brightness,
                                                float contrast) const {
  float result;
  result = (value - 0.5) * contrast + 0.5 + brightness;

  if (result < 0.0)
    result = 0.0;
  if (result > 1.0)
    result = 1.0;
  return result;
}

float DisplayQueue::TransformGamma(float value, float gamma) const {
  float result;

  result = pow(value, gamma);
  if (result < 0.0)
    result = 0.0;
  if (result > 1.0)
    result = 1.0;

  return result;
}

void DisplayQueue::SetColorCorrection(struct gamma_colors gamma,
                                      uint32_t contrast_c,
                                      uint32_t brightness_c) const {
  struct drm_color_lut* lut;
  float brightness[3];
  float contrast[3];
  uint8_t temp[3];
  int32_t ret;

  /* reset lut when contrast and brightness are all 0 */
  if (contrast_c == 0 && brightness_c == 0) {
    lut = NULL;
    ApplyPendingLUT(lut);
    free(lut);
    return;
  }

  lut = (struct drm_color_lut*)malloc(sizeof(struct drm_color_lut) * lut_size_);
  if (!lut) {
    ETRACE("Cannot allocate LUT memory");
    return;
  }

  /* Unpack brightness values for each channel */
  temp[0] = (brightness_c >> 16) & 0xFF;
  temp[1] = (brightness_c >> 8) & 0xFF;
  temp[2] = (brightness_c)&0xFF;

  /* Map brightness from -128 - 127 range into -0.5 - 0.5 range */
  brightness[0] = (float)(temp[0]) / 255 - 0.5;
  brightness[1] = (float)(temp[1]) / 255 - 0.5;
  brightness[2] = (float)(temp[2]) / 255 - 0.5;

  /* Unpack contrast values for each channel */
  temp[0] = (contrast_c >> 16) & 0xFF;
  temp[1] = (contrast_c >> 8) & 0xFF;
  temp[2] = (contrast_c)&0xFF;

  /* Map contrast from 0 - 255 range into 0.0 - 2.0 range */
  contrast[0] = (float)(temp[0]) / 128;
  contrast[1] = (float)(temp[1]) / 128;
  contrast[2] = (float)(temp[2]) / 128;

  uint32_t max_value = (1 << 16) - 1;
  uint32_t mask = ((1 << 8) - 1) << 8;
  for (uint64_t i = 0; i < lut_size_; i++) {
    /* Set lut[0] as 0 always as the darkest color should has brightness 0 */
    if (i == 0) {
      lut[i].red = 0;
      lut[i].green = 0;
      lut[i].blue = 0;
      continue;
    }

    lut[i].red = 0xFFFF * TransformGamma(TransformContrastBrightness(
                                             (float)(i) / lut_size_,
                                             brightness[0], contrast[0]),
                                         gamma.red);
    lut[i].green = 0xFFFF * TransformGamma(TransformContrastBrightness(
                                               (float)(i) / lut_size_,
                                               brightness[1], contrast[1]),
                                           gamma.green);
    lut[i].blue = 0xFFFF * TransformGamma(TransformContrastBrightness(
                                              (float)(i) / lut_size_,
                                              brightness[2], contrast[2]),
                                          gamma.blue);
  }

  ApplyPendingLUT(lut);
  free(lut);
}

bool DisplayQueue::SetBroadcastRGB(const char* range_property) {
  int64_t p_value = -1;

  if (!strcmp(range_property, "Full")) {
    p_value = broadcastrgb_full_;
  } else if (!strcmp(range_property, "Automatic")) {
    p_value = broadcastrgb_automatic_;
  } else {
    ETRACE("Wrong Broadcast RGB value %s", range_property);
    return false;
  }

  if (p_value < 0)
    return false;

  if (drmModeObjectSetProperty(gpu_fd_, connector_, DRM_MODE_OBJECT_CONNECTOR,
                               broadcastrgb_id_, (uint64_t)p_value) != 0)
    return false;

  return true;
}

void DisplayQueue::SetExplicitSyncSupport(bool disable_explicit_sync) {
  if (disable_explicit_sync == true) {
    disable_overlay_usage_ = true;
  } else {
    disable_overlay_usage_ = out_fence_ptr_prop_ == 0;
  }
}

bool DisplayQueue::SetActiveConfig(drmModeModeInfo& mode_info) {
  // update the Acive Mode
  mode_ = mode_info;

  return SetPowerMode(kOn);
}

}  // namespace hwcomposer
