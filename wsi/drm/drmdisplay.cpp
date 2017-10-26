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

#include "drmdisplay.h"

#include <cmath>
#include <set>

#include <hwcdefs.h>
#include <hwclayer.h>
#include <hwctrace.h>

#include <algorithm>
#include <string>
#include <sstream>

#include "displayqueue.h"
#include "displayplanemanager.h"
#include "drmdisplaymanager.h"
#include "wsi_utils.h"

namespace hwcomposer {

static const int32_t kUmPerInch = 25400;

DrmDisplay::DrmDisplay(uint32_t gpu_fd, uint32_t pipe_id, uint32_t crtc_id,
                       DrmDisplayManager *manager)
    : PhysicalDisplay(gpu_fd, pipe_id),
      crtc_id_(crtc_id),
      connector_(0),
      manager_(manager) {
  memset(&current_mode_, 0, sizeof(current_mode_));
}

DrmDisplay::~DrmDisplay() {
  if (blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, blob_id_);

  if (old_blob_id_)
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);

  display_queue_->SetPowerMode(kOff);
}

bool DrmDisplay::InitializeDisplay() {
  ScopedDrmObjectPropertyPtr crtc_props(
      drmModeObjectGetProperties(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC));
  GetDrmObjectProperty("ACTIVE", crtc_props, &active_prop_);
  GetDrmObjectProperty("MODE_ID", crtc_props, &mode_id_prop_);
  GetDrmObjectProperty("CTM", crtc_props, &ctm_id_prop_);
  GetDrmObjectProperty("GAMMA_LUT", crtc_props, &lut_id_prop_);
  GetDrmObjectPropertyValue("GAMMA_LUT_SIZE", crtc_props, &lut_size_);
  GetDrmObjectProperty("OUT_FENCE_PTR", crtc_props, &out_fence_ptr_prop_);

  return true;
}

bool DrmDisplay::ConnectDisplay(const drmModeModeInfo &mode_info,
                                const drmModeConnector *connector,
                                uint32_t config) {
  IHOTPLUGEVENTTRACE("DrmDisplay::Connect recieved.");
  // TODO(kalyan): Add support for multi monitor case.
  if (connector_ && connector->connector_id == connector_) {
    IHOTPLUGEVENTTRACE(
        "Display is already connected to this connector. %d %d %p \n",
        connector->connector_id, connector_, this);
    PhysicalDisplay::Connect();
    return true;
  }

  IHOTPLUGEVENTTRACE(
      "Display is being connected to a new connector.%d %d %p \n",
      connector->connector_id, connector_, this);
  connector_ = connector->connector_id;
  mmWidth_ = connector->mmWidth;
  mmHeight_ = connector->mmHeight;
  SetDisplayAttribute(mode_info);
  config_ = config;

  ScopedDrmObjectPropertyPtr connector_props(drmModeObjectGetProperties(
      gpu_fd_, connector_, DRM_MODE_OBJECT_CONNECTOR));
  if (!connector_props) {
    ETRACE("Unable to get connector properties.");
    return false;
  }

  // GetDrmObjectProperty("DPMS", connector_props, &dpms_prop_);
  GetDrmObjectProperty("CRTC_ID", connector_props, &crtc_prop_);
  GetDrmObjectProperty("Broadcast RGB", connector_props, &broadcastrgb_id_);
  GetDrmObjectProperty("DPMS", connector_props, &dpms_prop_);

  PhysicalDisplay::Connect();

  drmModePropertyPtr broadcastrgb_props =
      drmModeGetProperty(gpu_fd_, broadcastrgb_id_);

  SetPowerMode(power_mode_);

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

bool DrmDisplay::GetDisplayAttribute(uint32_t config /*config*/,
                                     HWCDisplayAttribute attribute,
                                     int32_t *value) {
  SPIN_LOCK(display_lock_);
  if (modes_.empty()) {
    SPIN_UNLOCK(display_lock_);
    return PhysicalDisplay::GetDisplayAttribute(config, attribute, value);
  }

  float refresh;
  bool status = true;
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = modes_[config].hdisplay;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = modes_[config].vdisplay;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      refresh = (modes_[config].clock * 1000.0f) /
                (modes_[config].htotal * modes_[config].vtotal);

      if (modes_[config].flags & DRM_MODE_FLAG_INTERLACE)
        refresh *= 2;

      if (modes_[config].flags & DRM_MODE_FLAG_DBLSCAN)
        refresh /= 2;

      if (modes_[config].vscan > 1)
        refresh /= modes_[config].vscan;
      // in nanoseconds
      *value = 1e9 / refresh;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value =
          mmWidth_ ? (modes_[config].hdisplay * kUmPerInch) / mmWidth_ : -1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value =
          mmHeight_ ? (modes_[config].vdisplay * kUmPerInch) / mmHeight_ : -1;
      break;
    default:
      *value = -1;
      status = false;
  }

  SPIN_UNLOCK(display_lock_);
  return status;
}

bool DrmDisplay::GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
  SPIN_LOCK(display_lock_);
  size_t modes_size = modes_.size();
  SPIN_UNLOCK(display_lock_);

  if (modes_size == 0) {
    return PhysicalDisplay::GetDisplayConfigs(num_configs, configs);
  }

  if (!configs) {
    *num_configs = modes_size;
    IHOTPLUGEVENTTRACE(
        "GetDisplayConfigs: Total Configs: %d pipe: %d display: %p",
        *num_configs, pipe_, this);
    return true;
  }

  IHOTPLUGEVENTTRACE(
      "GetDisplayConfigs: Populating Configs: %d pipe: %d display: %p",
      *num_configs, pipe_, this);

  uint32_t size = *num_configs;
  for (uint32_t i = 0; i < size; i++)
    configs[i] = i;

  return true;
}

bool DrmDisplay::GetDisplayName(uint32_t *size, char *name) {
  SPIN_LOCK(display_lock_);
  if (modes_.empty()) {
    SPIN_UNLOCK(display_lock_);
    return PhysicalDisplay::GetDisplayName(size, name);
  }
  SPIN_UNLOCK(display_lock_)
  std::ostringstream stream;
  stream << "Display-" << connector_;
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

void DrmDisplay::UpdateDisplayConfig() {
  // update the activeConfig
  SPIN_LOCK(display_lock_);
  flags_ |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  SetDisplayAttribute(modes_[config_]);
  SPIN_UNLOCK(display_lock_);
}

void DrmDisplay::PowerOn() {
  flags_ = 0;
  flags_ |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_ON);
  IHOTPLUGEVENTTRACE("PowerOn: Powered on Pipe: %d display: %p", pipe_, this);
}

bool DrmDisplay::SetBroadcastRGB(const char *range_property) {
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

bool DrmDisplay::Commit(
    const DisplayPlaneStateList &composition_planes,
    const DisplayPlaneStateList &previous_composition_planes,
    bool disable_explicit_fence, int32_t *commit_fence) {
  // Do the actual commit.
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());

  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  if (display_state_ & kNeedsModeset) {
    if (!ApplyPendingModeset(pset.get())) {
      ETRACE("Failed to Modeset.");
      return false;
    }
  } else if (!disable_explicit_fence && out_fence_ptr_prop_) {
    GetFence(pset.get(), commit_fence);
  }

  if (!CommitFrame(composition_planes, previous_composition_planes, pset.get(),
                   flags_)) {
    ETRACE("Failed to Commit layers.");
    return false;
  }

  if (display_state_ & kNeedsModeset) {
    display_state_ &= ~kNeedsModeset;
    if (!disable_explicit_fence) {
      flags_ = 0;
      flags_ |= DRM_MODE_ATOMIC_NONBLOCK;
    }
  }

  return true;
}

bool DrmDisplay::CommitFrame(
    const DisplayPlaneStateList &comp_planes,
    const DisplayPlaneStateList &previous_composition_planes,
    drmModeAtomicReqPtr pset, uint32_t flags) {
  CTRACE();
  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  for (const DisplayPlaneState &comp_plane : previous_composition_planes) {
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.plane());
    plane->SetEnabled(false);
  }

  for (const DisplayPlaneState &comp_plane : comp_planes) {
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.plane());
    const OverlayLayer *layer = comp_plane.GetOverlayLayer();
    int32_t fence = layer->GetAcquireFence();
    if (fence > 0) {
      plane->SetNativeFence(dup(fence));
    } else {
      plane->SetNativeFence(-1);
    }
    if (!plane->UpdateProperties(pset, crtc_id_, layer))
      return false;

    plane->SetEnabled(true);
  }

  for (const DisplayPlaneState &comp_plane : previous_composition_planes) {
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.plane());
    if (plane->IsEnabled())
      continue;

    plane->Disable(pset);
  }

  int ret = drmModeAtomicCommit(gpu_fd_, pset, flags, NULL);
  if (ret) {
    ETRACE("Failed to commit pset ret=%s\n", PRINTERROR());
    return false;
  }

  return true;
}

void DrmDisplay::SetDrmModeInfo(const std::vector<drmModeModeInfo> &mode_info) {
  SPIN_LOCK(display_lock_);
  uint32_t size = mode_info.size();
  std::vector<drmModeModeInfo>().swap(modes_);
  for (uint32_t i = 0; i < size; ++i) {
    modes_.emplace_back(mode_info[i]);
  }

  SPIN_UNLOCK(display_lock_);
}

void DrmDisplay::SetDisplayAttribute(const drmModeModeInfo &mode_info) {
  width_ = mode_info.hdisplay;
  height_ = mode_info.vdisplay;
  current_mode_ = mode_info;
}

void DrmDisplay::GetDrmObjectProperty(const char *name,
                                      const ScopedDrmObjectPropertyPtr &props,
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

void DrmDisplay::GetDrmObjectPropertyValue(
    const char *name, const ScopedDrmObjectPropertyPtr &props,
    uint64_t *value) const {
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

int64_t DrmDisplay::FloatToFixedPoint(float value) const {
  uint32_t *pointer = (uint32_t *)&value;
  uint32_t negative = (*pointer & (1u << 31)) >> 31;
  *pointer &= 0x7fffffff; /* abs of value*/
  return (negative ? (1ll << 63) : 0) |
          (__s64)((*(float *)pointer) * (double)(1ll << 31));
}

void DrmDisplay::ApplyPendingCTM(struct drm_color_ctm *ctm) const {
  if (ctm_id_prop_ == 0) {
    ETRACE("ctm_id_prop_ == 0");
    return;
  }

  uint32_t ctm_id = 0;
  drmModeCreatePropertyBlob(gpu_fd_, ctm, sizeof(drm_color_ctm), &ctm_id);
  if (ctm_id == 0) {
    ETRACE("ctm_id == 0");
    return;
  }

  drmModeObjectSetProperty(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC,
                           ctm_id_prop_, ctm_id);
  drmModeDestroyPropertyBlob(gpu_fd_, ctm_id);
}

void DrmDisplay::ApplyPendingLUT(struct drm_color_lut *lut) const {
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

float DrmDisplay::TransformContrastBrightness(float value, float brightness,
                                              float contrast) const {
  float result;
  result = (value - 0.5) * contrast + 0.5 + brightness;

  if (result < 0.0)
    result = 0.0;
  if (result > 1.0)
    result = 1.0;
  return result;
}

float DrmDisplay::TransformGamma(float value, float gamma) const {
  float result;

  result = pow(value, gamma);
  if (result < 0.0)
    result = 0.0;
  if (result > 1.0)
    result = 1.0;

  return result;
}

void DrmDisplay::DoSetColorTransformMatrix(const float *color_transform_matrix,
                                         HWCColorTransform color_transform_hint) const {
  struct drm_color_ctm *ctm = (struct drm_color_ctm *)malloc(sizeof(struct drm_color_ctm));
  if (!ctm) {
    ETRACE("Cannot allocate CTM memory");
    return;
  }

  switch (color_transform_hint) {
    case HWCColorTransform::kIdentical: {
      memset(ctm->matrix, 0, sizeof(ctm->matrix));
      for (int i = 0; i < 3; i++) {
        ctm->matrix[i * 3 + i] = (1ll << 31);
      }
      ApplyPendingCTM(ctm);
      break;
    }
    case HWCColorTransform::kArbitraryMatrix: {
      // Extract the coefficients from 4x4 CTM to the DRM 3x3 CTM. |Tr Tg Tb| row
      // will lost, so Color Inversion won't work with current implementation.
      //
      // TODO: Add drm interface to set CSC post offset (Tr Tg Tb) from HWC CTM .
      if (*(uint32_t*)(color_transform_matrix + 12) != 0 ||
          *(uint32_t*)(color_transform_matrix + 13) != 0 ||
          *(uint32_t*)(color_transform_matrix + 14) != 0) {
        // Bypass CTM conversion if |Tr Tg Tb| is not |0 0 0|.
        memset(ctm->matrix, 0, sizeof(ctm->matrix));
        for (int i = 0; i < 3; i++) {
          ctm->matrix[i * 3 + i] = (1ll << 31);
        }
      } else {
        for (int i = 0; i < 3; i++) {
          for (int j = 0; j < 3; j++) {
            ctm->matrix[i * 3 + j] = FloatToFixedPoint(color_transform_matrix[j * 4 + i]);
          }
        }
      }
      ApplyPendingCTM(ctm);
      break;
    }
  }
  free(ctm);
}

bool DrmDisplay::IsCTMSupported() const {
  if (ctm_id_prop_ == 0) {
    return false;
  }
  return true;
}

void DrmDisplay::SetColorCorrection(struct gamma_colors gamma,
                                    uint32_t contrast_c,
                                    uint32_t brightness_c) const {
  struct drm_color_lut *lut;
  float brightness[3];
  float contrast[3];
  uint8_t temp[3];

  /* reset lut when contrast and brightness are all 0 */
  if (contrast_c == 0 && brightness_c == 0) {
    lut = NULL;
    ApplyPendingLUT(lut);
    free(lut);
    return;
  }

  lut =
      (struct drm_color_lut *)malloc(sizeof(struct drm_color_lut) * lut_size_);
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

bool DrmDisplay::ApplyPendingModeset(drmModeAtomicReqPtr property_set) {
  if (old_blob_id_) {
    drmModeDestroyPropertyBlob(gpu_fd_, old_blob_id_);
    old_blob_id_ = 0;
  }

  drmModeCreatePropertyBlob(gpu_fd_, &current_mode_, sizeof(drmModeModeInfo),
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

bool DrmDisplay::GetFence(drmModeAtomicReqPtr property_set,
                          int32_t *out_fence) {
  int ret = drmModeAtomicAddProperty(property_set, crtc_id_,
                                     out_fence_ptr_prop_, (uintptr_t)out_fence);
  if (ret < 0) {
    ETRACE("Failed to add OUT_FENCE_PTR property to pset: %d", ret);
    return false;
  }

  return true;
}

void DrmDisplay::Disable(const DisplayPlaneStateList &composition_planes) {
  IHOTPLUGEVENTTRACE("Disable: Disabling Display: %p", this);

  for (const DisplayPlaneState &comp_plane : composition_planes) {
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.plane());
    plane->SetEnabled(false);
    plane->SetNativeFence(-1);
  }

  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_OFF);
}

bool DrmDisplay::PopulatePlanes(
    std::unique_ptr<DisplayPlane> &primary_plane,
    std::unique_ptr<DisplayPlane> &cursor_plane,
    std::vector<std::unique_ptr<DisplayPlane>> &overlay_planes) {
  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(gpu_fd_));
  if (!plane_resources) {
    ETRACE("Failed to get plane resources");
    return false;
  }

  uint32_t num_planes = plane_resources->count_planes;
  uint32_t pipe_bit = 1 << pipe_;
  std::set<uint32_t> plane_ids;
  for (uint32_t i = 0; i < num_planes; ++i) {
    ScopedDrmPlanePtr drm_plane(
        drmModeGetPlane(gpu_fd_, plane_resources->planes[i]));
    if (!drm_plane) {
      ETRACE("Failed to get plane ");
      return false;
    }

    if (!(pipe_bit & drm_plane->possible_crtcs))
      continue;

    uint32_t formats_size = drm_plane->count_formats;
    plane_ids.insert(drm_plane->plane_id);
    std::unique_ptr<DrmPlane> plane(
        CreatePlane(drm_plane->plane_id, drm_plane->possible_crtcs));
    std::vector<uint32_t> supported_formats(formats_size);
    for (uint32_t j = 0; j < formats_size; j++)
      supported_formats[j] = drm_plane->formats[j];

    if (plane->Initialize(gpu_fd_, supported_formats)) {
      if (plane->type() == DRM_PLANE_TYPE_CURSOR) {
        cursor_plane.reset(plane.release());
      } else if (plane->type() == DRM_PLANE_TYPE_PRIMARY) {
        plane->SetEnabled(true);
        primary_plane.reset(plane.release());
      } else if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        overlay_planes.emplace_back(plane.release());
      }
    }
  }

  if (!primary_plane) {
    ETRACE("Failed to get primary plane for display %d", crtc_id_);
    return false;
  }

  // We expect layers to be in ascending order.
  std::sort(
      overlay_planes.begin(), overlay_planes.end(),
      [](const std::unique_ptr<DisplayPlane> &l,
         const std::unique_ptr<DisplayPlane> &r) { return l->id() < r->id(); });

  return true;
}

void DrmDisplay::ForceRefresh() {
  display_queue_->ForceRefresh();
}

void DrmDisplay::NotifyClientsOfDisplayChangeStatus() {
  manager_->NotifyClientsOfDisplayChangeStatus();
}

bool DrmDisplay::TestCommit(
    const std::vector<OverlayPlane> &commit_planes) const {
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());
  for (auto i = commit_planes.begin(); i != commit_planes.end(); i++) {
    DrmPlane *plane = static_cast<DrmPlane *>(i->plane);
    if (!(plane->UpdateProperties(pset.get(), crtc_id_, i->layer, true))) {
      return false;
    }
  }

  if (drmModeAtomicCommit(gpu_fd_, pset.get(), DRM_MODE_ATOMIC_TEST_ONLY,
                          NULL)) {
    IDISPLAYMANAGERTRACE("Test Commit Failed. %s ", PRINTERROR());
    return false;
  }

  return true;
}

std::unique_ptr<DrmPlane> DrmDisplay::CreatePlane(uint32_t plane_id,
                                                  uint32_t possible_crtcs) {
  return std::unique_ptr<DrmPlane>(new DrmPlane(plane_id, possible_crtcs));
}

}  // namespace hwcomposer
