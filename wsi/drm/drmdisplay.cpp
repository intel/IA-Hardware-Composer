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
#include <limits>
#include <set>

#include <hwcdefs.h>
#include <hwclayer.h>
#include <hwctrace.h>
#include <hwcutils.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "displayplanemanager.h"
#include "displayqueue.h"
#include "drmdisplaymanager.h"
#include "wsi_utils.h"

#define CTA_EXTENSION_TAG 0x02
#define CTA_EXTENDED_TAG_CODE 0x07
#define CTA_COLORIMETRY_CODE 0x05

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
  GetDrmObjectProperty("CTM_POST_OFFSET", crtc_props,
                       &ctm_post_offset_id_prop_);
  GetDrmObjectProperty("GAMMA_LUT", crtc_props, &lut_id_prop_);
  GetDrmObjectPropertyValue("GAMMA_LUT_SIZE", crtc_props, &lut_size_);
  GetDrmObjectProperty("OUT_FENCE_PTR", crtc_props, &out_fence_ptr_prop_);
  GetDrmObjectProperty("background_color", crtc_props, &canvas_color_prop_);

  return true;
}

std::vector<uint8_t *> DrmDisplay::FindExtendedBlocksForTag(uint8_t *edid,
                                                            uint8_t block_tag) {
  int current_block;
  uint8_t *cta_ext_blk;
  uint8_t dblen;
  uint8_t d;
  uint8_t *cta_db_start;
  uint8_t *cta_db_end;
  uint8_t *dbptr;
  uint8_t tag;
  std::vector<uint8_t *> addrs;

  int num_blocks = edid[126];
  if (!num_blocks) {
    return addrs;
  }

  for (current_block = 1; current_block <= num_blocks; current_block++) {
    cta_ext_blk = edid + 128 * current_block;
    if (cta_ext_blk[0] != CTA_EXTENSION_TAG)
      continue;

    d = cta_ext_blk[2];
    cta_db_start = cta_ext_blk + 4;
    cta_db_end = cta_ext_blk + d - 1;
    for (dbptr = cta_db_start; dbptr < cta_db_end; dbptr++) {
      tag = dbptr[0] >> 0x05;
      dblen = dbptr[0] & 0x1F;

      // Check if the extension has an extended block
      if (tag == block_tag)
        addrs.emplace_back(dbptr);
    }
  }

  return addrs;
}

void DrmDisplay::DrmConnectorGetDCIP3Support(
    const ScopedDrmObjectPropertyPtr &props) {
  uint8_t *edid = NULL;
  uint64_t edid_blob_id;
  drmModePropertyBlobPtr blob;
  uint8_t block_tag;
  std::vector<uint8_t *> blocks;

  dcip3_ = false;

  GetDrmObjectPropertyValue("EDID", props, &edid_blob_id);
  blob = drmModeGetPropertyBlob(gpu_fd_, edid_blob_id);
  if (!blob) {
    return;
  }

  edid = (uint8_t *)blob->data;
  blocks = FindExtendedBlocksForTag(edid, CTA_EXTENDED_TAG_CODE);

  for (uint8_t *ext_block : blocks) {
    block_tag = ext_block[1];

    if (block_tag == CTA_COLORIMETRY_CODE) {
      dcip3_ = !!(ext_block[3] & 0x80);
      if (dcip3_)
        break;
    }
  }

  drmModeFreePropertyBlob(blob);

  return;
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

  int value = -1;
  GetDrmHDCPObjectProperty("Content Protection", connector, connector_props,
                           &hdcp_id_prop_, &value);

  if (value >= 0) {
    switch (value) {
      case 0:
        current_protection_support_ = HWCContentProtection::kUnDesired;
        break;
      case 1:
        current_protection_support_ = HWCContentProtection::kDesired;
        break;
      default:
        break;
    }

    if (desired_protection_support_ == HWCContentProtection::kUnSupported) {
      desired_protection_support_ = current_protection_support_;
    }
  }

  GetDrmHDCPObjectProperty("CP_SRM", connector, connector_props,
                           &hdcp_srm_id_prop_, &value);

  GetDrmObjectProperty("CRTC_ID", connector_props, &crtc_prop_);
  GetDrmObjectProperty("Broadcast RGB", connector_props, &broadcastrgb_id_);
  GetDrmObjectProperty("DPMS", connector_props, &dpms_prop_);

  DrmConnectorGetDCIP3Support(connector_props);
  if (dcip3_)
    ITRACE("DCIP3 support available");
  else
    ITRACE("DCIP3 support not available");

  PhysicalDisplay::Connect();
  SetHDCPState(desired_protection_support_, content_type_);

  drmModePropertyPtr broadcastrgb_props =
      drmModeGetProperty(gpu_fd_, broadcastrgb_id_);

  SetPowerMode(power_mode_);

  // This is a valid case on DSI panels.
  if (broadcastrgb_props == NULL) {
    WTRACE("Unable to get Broadcast RGB properties\n");
    return true;
  }

  if (!(broadcastrgb_props->flags & DRM_MODE_PROP_ENUM)) {
    drmModeFreeProperty(broadcastrgb_props);
    return false;
  }

  if (broadcastrgb_props->enums != NULL) {
    for (int i = 0; i < broadcastrgb_props->count_enums; i++) {
      if (!strcmp(broadcastrgb_props->enums[i].name, "Full")) {
        broadcastrgb_full_ = broadcastrgb_props->enums[i].value;
      } else if (!strcmp(broadcastrgb_props->enums[i].name, "Automatic")) {
        broadcastrgb_automatic_ = broadcastrgb_props->enums[i].value;
      }
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
      if (!custom_resolution_) {
        *value = modes_[config].hdisplay;
      } else {
        *value = rect_.right - rect_.left;
      }
      IHOTPLUGEVENTTRACE("GetDisplayAttribute: width %d set", *value);
      break;
    case HWCDisplayAttribute::kHeight:
      if (!custom_resolution_) {
        *value = modes_[config].vdisplay;
      } else {
        *value = rect_.bottom - rect_.top;
      }
      IHOTPLUGEVENTTRACE("GetDisplayAttribute: height %d set", *value);
      break;
    case HWCDisplayAttribute::kRefreshRate:
      if (!custom_resolution_) {
        refresh = (modes_[config].clock * 1000.0f) /
                  (modes_[config].htotal * modes_[config].vtotal);
      } else {
        refresh = (modes_[config].clock * 1000.0f) /
                  ((rect_.right - rect_.left) * (rect_.bottom - rect_.top));
      }

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
      if (!custom_resolution_) {
        *value =
          mmWidth_ ? (modes_[config].hdisplay * kUmPerInch) / mmWidth_ : -1;
      } else {
        *value =
          mmWidth_ ? ((rect_.right - rect_.left) * kUmPerInch) / mmWidth_ : -1;
      }
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      if (!custom_resolution_) {
        *value =
          mmHeight_ ? (modes_[config].vdisplay * kUmPerInch) / mmHeight_ : -1;
      } else {
        *value =
          mmHeight_ ? ((rect_.bottom - rect_.top) * kUmPerInch) /mmHeight_: -1;
      }
      break;
    default:
      *value = -1;
      status = false;
  }

  SPIN_UNLOCK(display_lock_);
  return status;
}

bool DrmDisplay::GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
  if (!num_configs)
    return false;

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

  uint32_t size = *num_configs > modes_size ? modes_size : *num_configs;
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

  *size = std::min<uint32_t>(static_cast<uint32_t>(length + 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

void DrmDisplay::UpdateDisplayConfig() {
  // update the activeConfig
  SPIN_LOCK(display_lock_);
  if (modes_.empty()) {
    SPIN_UNLOCK(display_lock_);
    return;
  }
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

void DrmDisplay::SetHDCPState(HWCContentProtection state,
                              HWCContentType content_type) {
  desired_protection_support_ = state;
  content_type_ = content_type;
  if (desired_protection_support_ == current_protection_support_)
    return;

  if (hdcp_id_prop_ <= 0) {
    ETRACE("Cannot set HDCP state as Connector property is not supported \n");
    return;
  }

  if (!(connection_state_ & kConnected)) {
    return;
  }

  current_protection_support_ = desired_protection_support_;
  uint32_t value = 0;
  if (current_protection_support_ == kDesired) {
    value = 1;
  }

  drmModeConnectorSetProperty(gpu_fd_, connector_, hdcp_id_prop_, value);
  ETRACE("Ignored Content type. \n");
}

void DrmDisplay::SetHDCPSRM(const int8_t *SRM, uint32_t SRMLength) {
  if (hdcp_srm_id_prop_ <= 0) {
    ETRACE("Cannot set HDCP state as Connector property is not supported \n");
    return;
  }

  if (!(connection_state_ & kConnected)) {
    return;
  }

  uint32_t srm_id = 0;
  drmModeCreatePropertyBlob(gpu_fd_, SRM, SRMLength, &srm_id);
  if (srm_id == 0) {
    ETRACE("srm_id == 0");
    return;
  }

  drmModeConnectorSetProperty(gpu_fd_, connector_, hdcp_srm_id_prop_, srm_id);
  drmModeDestroyPropertyBlob(gpu_fd_, srm_id);
}

bool DrmDisplay::ContainConnector(const uint32_t connector_id) {
  return (connector_ == connector_id);
}

bool DrmDisplay::Commit(
    const DisplayPlaneStateList &composition_planes,
    const DisplayPlaneStateList &previous_composition_planes,
    bool disable_explicit_fence, int32_t previous_fence, int32_t *commit_fence,
    bool *previous_fence_released) {
  // Do the actual commit.
  ScopedDrmAtomicReqPtr pset(drmModeAtomicAlloc());
  *previous_fence_released = false;

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
                   flags_, previous_fence, previous_fence_released)) {
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

#ifdef ENABLE_DOUBLE_BUFFERING
  int32_t fence = *commit_fence;
  if (fence > 0) {
    HWCPoll(fence, -1);
    close(fence);
    *commit_fence = 0;
  }
#endif

  return true;
}

bool DrmDisplay::CommitFrame(
    const DisplayPlaneStateList &comp_planes,
    const DisplayPlaneStateList &previous_composition_planes,
    drmModeAtomicReqPtr pset, uint32_t flags, int32_t previous_fence,
    bool *previous_fence_released) {
  CTRACE();
  if (!pset) {
    ETRACE("Failed to allocate property set %d", -ENOMEM);
    return false;
  }

  for (const DisplayPlaneState &comp_plane : comp_planes) {
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.GetDisplayPlane());

    OverlayLayer *layer = (OverlayLayer *)comp_plane.GetOverlayLayer();
    const HwcRect<int> &display_rect = layer->GetDisplayFrame();

    // Recalculate the layer's display frame position before drm commit
    // if there is plane transform with the type display rotation.
    uint32_t plane_transform = layer->GetPlaneTransform();
    hwcomposer::DisplayPlaneState::RotationType rotation_type =
        comp_plane.GetRotationType();
    if ((plane_transform != kIdentity) &&
        (rotation_type == DisplayPlaneState::RotationType::kDisplayRotation)) {
      HwcRect<int> rotated_rect =
          RotateScaleRect(display_rect, width_, height_, plane_transform);
      layer->SetDisplayFrame(rotated_rect);
    }

    int32_t fence = layer->GetAcquireFence();
    if (fence > 0) {
      plane->SetNativeFence(dup(fence));
    } else {
      plane->SetNativeFence(-1);
    }

    if (comp_plane.Scanout() && !comp_plane.IsSurfaceRecycled())
      plane->SetBuffer(layer->GetSharedBuffer());

    if (!plane->UpdateProperties(pset, crtc_id_, layer))
      return false;
  }

  for (const DisplayPlaneState &comp_plane : previous_composition_planes) {
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.GetDisplayPlane());
    if (plane->InUse())
      continue;

    plane->Disable(pset);
  }

#ifndef ENABLE_DOUBLE_BUFFERING
  if (previous_fence > 0) {
    HWCPoll(previous_fence, -1);
    close(previous_fence);
    *previous_fence_released = true;
  }
#endif

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
#ifdef ENABLE_ANDROID_WA
    //FIXME: SurfaceFlinger can't distinguish interlace mode config.
    //interlace mode is not requirement for android, ignore them.
    if (!(mode_info[i].flags & DRM_MODE_FLAG_INTERLACE))
#endif
      modes_.emplace_back(mode_info[i]);
  }

  SPIN_UNLOCK(display_lock_);
}

void DrmDisplay::SetDisplayAttribute(const drmModeModeInfo &mode_info) {
  // Default resolution of the display
  if (!custom_resolution_) {
    width_ = mode_info.hdisplay;
    height_ = mode_info.vdisplay;
  } else {
    width_ = rect_.right - rect_.left;
    height_ = rect_.bottom - rect_.top;
  }
  IHOTPLUGEVENTTRACE("SetDisplayAttribute: width %d, height %d", width_,
                     height_);

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

void DrmDisplay::GetDrmHDCPObjectProperty(
    const char *name, const drmModeConnector *connector,
    const ScopedDrmObjectPropertyPtr &props, uint32_t *id, int *value) const {
  uint32_t count_props = props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(gpu_fd_, props->props[i]));
    if (property && !strcmp(property->name, name)) {
      *id = property->prop_id;
      if (value) {
        for (int prop_idx = 0; prop_idx < connector->count_props; ++prop_idx) {
          if (connector->props[prop_idx] != property->prop_id)
            continue;

          for (int enum_idx = 0; enum_idx < property->count_enums; ++enum_idx) {
            const drm_mode_property_enum &property_enum =
                property->enums[enum_idx];
            if (property_enum.value == connector->prop_values[prop_idx]) {
              *value = property_enum.value;
            }
          }
        }
      }
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
         (__s64)((*(float *)pointer) * (double)(1ll << 32));
}

void DrmDisplay::ApplyPendingCTM(
    struct drm_color_ctm *ctm,
    struct drm_color_ctm_post_offset *ctm_post_offset) const {
  if (ctm_id_prop_ == 0) {
    ETRACE("ctm_id_prop_ == 0");
    return;
  }

  if (ctm_post_offset_id_prop_ == 0) {
    ETRACE("ctm_post_offset_id_prop_ == 0");
    return;
  }

  uint32_t ctm_id = 0;
  drmModeCreatePropertyBlob(gpu_fd_, ctm, sizeof(drm_color_ctm), &ctm_id);
  if (ctm_id == 0) {
    ETRACE("ctm_id == 0");
    return;
  }

  uint32_t ctm_post_offset_id = 0;
  drmModeCreatePropertyBlob(gpu_fd_, ctm_post_offset,
                            sizeof(drm_color_ctm_post_offset),
                            &ctm_post_offset_id);
  if (ctm_post_offset_id == 0) {
    ETRACE("ctm_post_offset_id == 0");
    return;
  }

  drmModeObjectSetProperty(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC,
                           ctm_id_prop_, ctm_id);
  drmModeDestroyPropertyBlob(gpu_fd_, ctm_id);

  drmModeObjectSetProperty(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC,
                           ctm_post_offset_id_prop_, ctm_post_offset_id);
  drmModeDestroyPropertyBlob(gpu_fd_, ctm_post_offset_id);
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

uint64_t DrmDisplay::DrmRGBA(uint16_t bpc, uint16_t red, uint16_t green,
                             uint16_t blue, uint16_t alpha) const {
  if (bpc > 16)
    bpc = 16;

  /*
   * If we were provided with fewer than 16 bpc, shift the value we
   * received into the most significant bits.
   */
  int shift = 16 - bpc;

  uint64_t val = 0;
  val = red << shift;
  val <<= 16;
  val |= green << shift;
  val <<= 16;
  val |= blue << shift;
  val <<= 16;
  val |= alpha << shift;

  return val;
}

void DrmDisplay::SetPipeCanvasColor(uint16_t bpc, uint16_t red, uint16_t green,
                                    uint16_t blue, uint16_t alpha) const {
  if (canvas_color_prop_ == 0)
    return;

  uint64_t canvas_color = 0;
  if (bpc == 8)
    canvas_color = DRM_RGBA8888(red, green, blue, alpha);
  else if (bpc == 16)
    canvas_color = DRM_RGBA16161616(red, green, blue, alpha);

  drmModeObjectSetProperty(gpu_fd_, crtc_id_, DRM_MODE_OBJECT_CRTC,
                           canvas_color_prop_, canvas_color);
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

void DrmDisplay::SetColorTransformMatrix(
    const float *color_transform_matrix,
    HWCColorTransform color_transform_hint) const {
  struct drm_color_ctm *ctm =
      (struct drm_color_ctm *)malloc(sizeof(struct drm_color_ctm));
  if (!ctm) {
    ETRACE("Cannot allocate CTM memory");
    return;
  }

  struct drm_color_ctm_post_offset *ctm_post_offset =
      (struct drm_color_ctm_post_offset *)malloc(
          sizeof(struct drm_color_ctm_post_offset));
  if (!ctm_post_offset) {
    free(ctm);
    ETRACE("Cannot allocate ctm_post_offset memory");
    return;
  }

  switch (color_transform_hint) {
    case HWCColorTransform::kIdentical: {
      memset(ctm->matrix, 0, sizeof(ctm->matrix));
      for (int i = 0; i < 3; i++) {
        ctm->matrix[i * 3 + i] = (1ll << 32);
      }
      ctm_post_offset->red = 0;
      ctm_post_offset->green = 0;
      ctm_post_offset->blue = 0;
      ApplyPendingCTM(ctm, ctm_post_offset);
      break;
    }
    case HWCColorTransform::kArbitraryMatrix: {
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
          ctm->matrix[i * 3 + j] =
              FloatToFixedPoint(color_transform_matrix[j * 4 + i]);
        }
      }
      ctm_post_offset->red = color_transform_matrix[12] * 0xffff;
      ctm_post_offset->green = color_transform_matrix[13] * 0xffff;
      ctm_post_offset->blue = color_transform_matrix[14] * 0xffff;
      ApplyPendingCTM(ctm, ctm_post_offset);
      break;
    }
  }
  free(ctm);
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
    DrmPlane *plane = static_cast<DrmPlane *>(comp_plane.GetDisplayPlane());
    plane->SetInUse(false);
    plane->SetNativeFence(-1);
  }

  drmModeConnectorSetProperty(gpu_fd_, connector_, dpms_prop_,
                              DRM_MODE_DPMS_OFF);
}

bool DrmDisplay::PopulatePlanes(
    std::vector<std::unique_ptr<DisplayPlane>> &overlay_planes) {
  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(gpu_fd_));
  if (!plane_resources) {
    ETRACE("Failed to get plane resources");
    return false;
  }

  uint32_t num_planes = plane_resources->count_planes;
  uint32_t pipe_bit = 1 << pipe_;
  std::set<uint32_t> plane_ids;
  std::unique_ptr<DisplayPlane> cursor_plane;
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

    bool use_modifier = true;
#ifdef THREEDIS_UNDERRUN_WA
    use_modifier = (manager_->GetConnectedPhysicalDisplayCount() < 3);
#endif
    if (plane->Initialize(gpu_fd_, supported_formats, use_modifier)) {
      if (plane->type() == DRM_PLANE_TYPE_CURSOR) {
        cursor_plane.reset(plane.release());
      } else {
        overlay_planes.emplace_back(plane.release());
      }
    }
  }

  if (overlay_planes.empty()) {
    ETRACE("Failed to get primary plane for display %d", crtc_id_);
    return false;
  }

  // We expect layers to be in ascending order.
  std::sort(
      overlay_planes.begin(), overlay_planes.end(),
      [](const std::unique_ptr<DisplayPlane> &l,
         const std::unique_ptr<DisplayPlane> &r) { return l->id() < r->id(); });

  if (cursor_plane) {
    overlay_planes.emplace_back(cursor_plane.release());
  }

  return true;
}

void DrmDisplay::ForceRefresh() {
  display_queue_->ForceRefresh();
}

void DrmDisplay::IgnoreUpdates() {
  display_queue_->IgnoreUpdates();
}

void DrmDisplay::HandleLazyInitialization() {
  manager_->HandleLazyInitialization();
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
