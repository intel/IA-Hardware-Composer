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

#include "drmplane.h"

#include <drm_fourcc.h>
#include <cmath>

#include <gpudevice.h>

#include "hwctrace.h"
#include "hwcutils.h"
#include "overlaylayer.h"

namespace hwcomposer {

DrmPlane::Property::Property() {
}

bool DrmPlane::Property::Initialize(
    uint32_t fd, const char* name,
    const ScopedDrmObjectPropertyPtr& plane_props, uint32_t* rotation,
    uint64_t* in_formats_prop_value) {
  uint32_t count_props = plane_props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(fd, plane_props->props[i]));
    if (property && !strcmp(property->name, name)) {
      id = property->prop_id;
      if (rotation) {
        uint32_t temp = 0;
        for (int enum_index = 0; enum_index < property->count_enums;
             enum_index++) {
          struct drm_mode_property_enum* penum = &(property->enums[enum_index]);
          if (!strcmp(penum->name, "rotate-90")) {
            temp |= DRM_MODE_ROTATE_90;
          }
          if (!strcmp(penum->name, "rotate-180"))
            temp |= DRM_MODE_ROTATE_180;
          else if (!strcmp(penum->name, "rotate-270"))
            temp |= DRM_MODE_ROTATE_270;
          else if (!strcmp(penum->name, "rotate-0"))
            temp |= DRM_MODE_ROTATE_0;
        }

        *rotation = temp;
      }
      if (!strcmp(property->name, "IN_FORMATS")) {
        if (in_formats_prop_value) {
          *in_formats_prop_value = plane_props->prop_values[i];
        }
      }
      break;
    }
  }
  if (!id) {
    ETRACE("Could not find property %s", name);
    return false;
  }
  return true;
}

DrmPlane::DrmPlane(uint32_t plane_id, uint32_t possible_crtcs)
    : id_(plane_id),
      possible_crtc_mask_(possible_crtcs),
      type_(0),
      last_valid_format_(0),
      in_use_(false) {
}

DrmPlane::~DrmPlane() {
  SetNativeFence(-1);
}

bool DrmPlane::Initialize(uint32_t gpu_fd, const std::vector<uint32_t>& formats,
                          bool use_modifier) {
  supported_formats_ = formats;
  use_modifier_ = use_modifier;
  uint32_t total_size = supported_formats_.size();
  for (uint32_t j = 0; j < total_size; j++) {
    uint32_t format = supported_formats_.at(j);
    if (IsSupportedMediaFormat(format)) {
      prefered_video_format_ = format;
      break;
    }
  }

  for (uint32_t j = 0; j < total_size; j++) {
    uint32_t format = supported_formats_.at(j);
    switch (format) {
      case DRM_FORMAT_BGRA8888:
      case DRM_FORMAT_RGBA8888:
      case DRM_FORMAT_ABGR8888:
      case DRM_FORMAT_ARGB8888:
      case DRM_FORMAT_RGB888:
      case DRM_FORMAT_XRGB8888:
      case DRM_FORMAT_XBGR8888:
      case DRM_FORMAT_RGBX8888:
        prefered_format_ = format;
        break;
    }
  }

  if (type_ == DRM_PLANE_TYPE_PRIMARY) {
    if (prefered_format_ != DRM_FORMAT_XBGR8888 &&
        IsSupportedFormat(DRM_FORMAT_XBGR8888)) {
      prefered_format_ = DRM_FORMAT_XBGR8888;
    }
  }

  if (prefered_video_format_ == 0) {
    prefered_video_format_ = prefered_format_;
  }

  ScopedDrmObjectPropertyPtr plane_props(
      drmModeObjectGetProperties(gpu_fd, id_, DRM_MODE_OBJECT_PLANE));
  if (!plane_props) {
    ETRACE("Unable to get plane properties.");
    return false;
  }
  uint32_t count_props = plane_props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(gpu_fd, plane_props->props[i]));
    if (property && !strcmp(property->name, "type")) {
      type_ = plane_props->prop_values[i];
      break;
    }
  }

  bool ret = crtc_prop_.Initialize(gpu_fd, "CRTC_ID", plane_props);
  if (!ret)
    return false;

  ret = fb_prop_.Initialize(gpu_fd, "FB_ID", plane_props);
  if (!ret)
    return false;

  ret = crtc_x_prop_.Initialize(gpu_fd, "CRTC_X", plane_props);
  if (!ret)
    return false;

  ret = crtc_y_prop_.Initialize(gpu_fd, "CRTC_Y", plane_props);
  if (!ret)
    return false;

  ret = crtc_w_prop_.Initialize(gpu_fd, "CRTC_W", plane_props);
  if (!ret)
    return false;

  ret = crtc_h_prop_.Initialize(gpu_fd, "CRTC_H", plane_props);
  if (!ret)
    return false;

  ret = src_x_prop_.Initialize(gpu_fd, "SRC_X", plane_props);
  if (!ret)
    return false;

  ret = src_y_prop_.Initialize(gpu_fd, "SRC_Y", plane_props);
  if (!ret)
    return false;

  ret = src_w_prop_.Initialize(gpu_fd, "SRC_W", plane_props);
  if (!ret)
    return false;

  ret = src_h_prop_.Initialize(gpu_fd, "SRC_H", plane_props);
  if (!ret)
    return false;

  ret = rotation_prop_.Initialize(gpu_fd, "rotation", plane_props, &rotation_);
  if (!ret)
    ETRACE("Could not get rotation property");

  ret = alpha_prop_.Initialize(gpu_fd, "alpha", plane_props);
  if (!ret)
    ETRACE("Could not get alpha property");

  ret = in_fence_fd_prop_.Initialize(gpu_fd, "IN_FENCE_FD", plane_props);
  if (!ret) {
    ETRACE("Could not get IN_FENCE_FD property");
    in_fence_fd_prop_.id = 0;
  }

  // query and store supported modifiers for format, from in_formats
  // property
  uint64_t in_formats_prop_value = 0;
  ret = in_formats_prop_.Initialize(gpu_fd, "IN_FORMATS", plane_props, NULL,
                                    &in_formats_prop_value);
  if (!ret) {
    ETRACE("Could not get IN_FORMATS property");
  }

  if (in_formats_prop_value != 0) {
    drmModePropertyBlobPtr blob =
        drmModeGetPropertyBlob(gpu_fd, in_formats_prop_value);
    if (blob == nullptr || blob->data == nullptr) {
      ETRACE("Unable to get property data\n");
      return false;
    }

    struct drm_format_modifier_blob* m =
        (struct drm_format_modifier_blob*)(blob->data);
    struct drm_format_modifier* mod_o =
        (struct drm_format_modifier*)(void*)(((char*)m) + m->modifiers_offset);

    bool y_tiled_ccs_supported = false;
    bool y_tiled_yf_ccs_supported = false;

    for (uint32_t j = 0; j < total_size; j++) {
      uint32_t format = supported_formats_.at(j);
      format_mods modifiers_obj;
      modifiers_obj.format = format;
      uint32_t format_index = j;

      struct drm_format_modifier* mod = mod_o;
      for (int i = 0; i < (int)m->count_modifiers; i++, mod++) {
        if (mod->formats & (1ULL << format_index)) {
          modifiers_obj.mods.emplace_back(mod->modifier);
          if (mod->modifier == I915_FORMAT_MOD_Y_TILED_CCS) {
            y_tiled_ccs_supported = true;
          } else if (mod->modifier == I915_FORMAT_MOD_Yf_TILED_CCS) {
            y_tiled_yf_ccs_supported = true;
          }
        }
      }

      if (modifiers_obj.mods.size() == 0) {
        modifiers_obj.mods.emplace_back(DRM_FORMAT_MOD_NONE);
        prefered_modifier_ = DRM_FORMAT_MOD_NONE;
      } else {
        if (y_tiled_ccs_supported) {
          prefered_modifier_ = I915_FORMAT_MOD_Y_TILED_CCS;
        } else if (y_tiled_yf_ccs_supported) {
          prefered_modifier_ = I915_FORMAT_MOD_Yf_TILED_CCS;
        } else {
          prefered_modifier_ = modifiers_obj.mods.at(0);
        }
      }

      formats_modifiers_.emplace_back(modifiers_obj);
    }

    drmModeFreePropertyBlob(blob);
  }
  return true;
}

bool DrmPlane::UpdateProperties(drmModeAtomicReqPtr property_set,
                                uint32_t crtc_id, const OverlayLayer* layer,
                                bool test_commit) const {
  uint64_t alpha = 0xFF;
  OverlayBuffer* buffer = layer->GetBuffer();
  const HwcRect<int>& display_frame = layer->GetDisplayFrame();
  const HwcRect<float>& source_crop = layer->GetSourceCrop();
  int fence = kms_fence_;
  if (test_commit) {
    fence = layer->GetAcquireFence();
  }

  if (layer->GetBlending() == HWCBlending::kBlendingPremult)
    alpha = layer->GetAlpha();

  IDISPLAYMANAGERTRACE("buffer->GetFb() ---------------------- STARTS %d",
                       buffer->GetFb());
  int success =
      drmModeAtomicAddProperty(property_set, id_, crtc_prop_.id, crtc_id) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, fb_prop_.id,
                                      buffer->GetFb()) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, crtc_x_prop_.id,
                                      display_frame.left) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, crtc_y_prop_.id,
                                      display_frame.top) < 0;

  if (layer->IsCursorLayer()) {
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_w_prop_.id,
                                        buffer->GetWidth()) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_h_prop_.id,
                                        buffer->GetHeight()) < 0;
    success |=
        drmModeAtomicAddProperty(property_set, id_, src_x_prop_.id, 0) < 0;
    success |=
        drmModeAtomicAddProperty(property_set, id_, src_y_prop_.id, 0) < 0;

    success |= drmModeAtomicAddProperty(property_set, id_, src_w_prop_.id,
                                        buffer->GetWidth() << 16) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, src_h_prop_.id,
                                        buffer->GetHeight() << 16) < 0;
  } else {
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_w_prop_.id,
                                        layer->GetDisplayFrameWidth()) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_h_prop_.id,
                                        layer->GetDisplayFrameHeight()) < 0;
    success |= drmModeAtomicAddProperty(
                   property_set, id_, src_x_prop_.id,
                   static_cast<int>(ceilf(source_crop.left)) << 16) < 0;
    success |= drmModeAtomicAddProperty(
                   property_set, id_, src_y_prop_.id,
                   static_cast<int>(ceilf((source_crop.top))) << 16) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, src_w_prop_.id,
                                        layer->GetSourceCropWidth() << 16) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, src_h_prop_.id,
                                        layer->GetSourceCropHeight() << 16) < 0;
  }

  if (rotation_prop_.id) {
    uint32_t rotation = 0;
    uint32_t transform = layer->GetPlaneTransform();
    if (transform & kTransform90) {
      rotation |= DRM_MODE_ROTATE_90;
      if (transform & kReflectX)
        rotation |= DRM_MODE_REFLECT_X;
      if (transform & kReflectY)
        rotation |= DRM_MODE_REFLECT_Y;
    } else if (transform & kTransform180)
      rotation |= DRM_MODE_ROTATE_180;
    else if (transform & kTransform270)
      rotation |= DRM_MODE_ROTATE_270;
    else
      rotation |= DRM_MODE_ROTATE_0;

    success = drmModeAtomicAddProperty(property_set, id_, rotation_prop_.id,
                                       rotation) < 0;
  }

  if (alpha_prop_.id) {
    success =
        drmModeAtomicAddProperty(property_set, id_, alpha_prop_.id, alpha) < 0;
  }

  if (fence > 0 && in_fence_fd_prop_.id) {
    success = drmModeAtomicAddProperty(property_set, id_, in_fence_fd_prop_.id,
                                       fence) < 0;
  }

  if (success) {
    ETRACE("Could not update properties for plane with id: %d", id_);
    return false;
  }
  IDISPLAYMANAGERTRACE("buffer->GetFb() ---------------------- ENDS%d",
                       buffer->GetFb());
  return true;
}

void DrmPlane::SetNativeFence(int32_t fd) {
  // Release any existing fence.
  if (kms_fence_ > 0) {
    close(kms_fence_);
  }

  kms_fence_ = fd;
}

void DrmPlane::SetBuffer(std::shared_ptr<OverlayBuffer>& buffer) {
  buffer_ = buffer;
}

void DrmPlane::BlackListPreferredFormatModifier() {
  if (!prefered_modifier_succeeded_)
    prefered_modifier_ = 0;
}

void DrmPlane::PreferredFormatModifierValidated() {
  prefered_modifier_succeeded_ = true;
}

bool DrmPlane::Disable(drmModeAtomicReqPtr property_set) {
  in_use_ = false;
  int success =
      drmModeAtomicAddProperty(property_set, id_, crtc_prop_.id, 0) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, fb_prop_.id, 0) < 0;
  success |=
      drmModeAtomicAddProperty(property_set, id_, crtc_x_prop_.id, 0) < 0;
  success |=
      drmModeAtomicAddProperty(property_set, id_, crtc_y_prop_.id, 0) < 0;
  success |=
      drmModeAtomicAddProperty(property_set, id_, crtc_w_prop_.id, 0) < 0;
  success |=
      drmModeAtomicAddProperty(property_set, id_, crtc_h_prop_.id, 0) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, src_x_prop_.id, 0) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, src_y_prop_.id, 0) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, src_w_prop_.id, 0) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, src_h_prop_.id, 0) < 0;

  if (success) {
    ETRACE("Could not update properties for plane with id: %d", id_);
    return false;
  }

  SetNativeFence(-1);
  buffer_.reset();

  return true;
}

uint32_t DrmPlane::id() const {
  return id_;
}

bool DrmPlane::GetCrtcSupported(uint32_t pipe_id) const {
  return !!((1 << pipe_id) & possible_crtc_mask_);
}

uint32_t DrmPlane::type() const {
  return type_;
}

bool DrmPlane::ValidateLayer(const OverlayLayer* layer) {
  uint64_t alpha = 0xFF;

  if (layer->GetBlending() == HWCBlending::kBlendingPremult)
    alpha = layer->GetAlpha();

  if (type_ == DRM_PLANE_TYPE_OVERLAY && (alpha != 0 && alpha != 0xFF) &&
      alpha_prop_.id == 0) {
    IDISPLAYMANAGERTRACE(
        "Alpha property not supported, Cannot composite layer using Overlay.");
    return false;
  }

  bool zero_rotation = false;
  uint32_t transform = layer->GetPlaneTransform();
  if (transform == kIdentity) {
    zero_rotation = true;
  }

  if (!zero_rotation && rotation_prop_.id == 0) {
    IDISPLAYMANAGERTRACE(
        "Rotation property not supported, Cannot composite layer using "
        "Overlay.");
    return false;
  }

  if (!IsSupportedFormat(layer->GetBuffer()->GetFormat())) {
    IDISPLAYMANAGERTRACE(
        "Layer cannot be supported as format is not supported.");
    return false;
  }

  return IsSupportedTransform(transform);
}

bool DrmPlane::IsSupportedFormat(uint32_t format) {
  if (last_valid_format_ == format)
    return true;

  for (auto& element : supported_formats_) {
    if (element == format) {
      last_valid_format_ = format;
      return true;
    }
  }

  return false;
}

bool DrmPlane::IsSupportedTransform(uint32_t transform) const {
  if (transform & kTransform90) {
    if (!(rotation_ & DRM_MODE_ROTATE_90)) {
      return false;
    }
  } else if (transform & kTransform180) {
    if (!(rotation_ & DRM_MODE_ROTATE_180)) {
      return false;
    }
  } else if (transform & kTransform270) {
    if (!(rotation_ & DRM_MODE_ROTATE_270)) {
      return false;
    }
  } else {
    if (!(rotation_ & DRM_MODE_ROTATE_0)) {
      return false;
    }
  }

  return true;
}

uint32_t DrmPlane::GetPreferredVideoFormat() const {
  return prefered_video_format_;
}

uint32_t DrmPlane::GetPreferredFormat() const {
  return prefered_format_;
}

uint64_t DrmPlane::GetPreferredFormatModifier() const {
  if (!use_modifier_)
    return DRM_FORMAT_MOD_NONE;
  else
    return prefered_modifier_;
}

void DrmPlane::SetInUse(bool in_use) {
  in_use_ = in_use;
}

bool DrmPlane::IsSupportedModifier(uint64_t modifier, uint32_t format) {
  uint32_t count = formats_modifiers_.size();
  for (uint32_t i = 0; i < count; i++) {
    const format_mods& obj = formats_modifiers_.at(i);
    if (obj.format == format) {
      std::vector<uint64_t>::const_iterator it;
      it = std::find(obj.mods.begin(), obj.mods.end(), modifier);
      if (it != obj.mods.end()) {
        return true;
      }
    }
  }
  return false;
}

void DrmPlane::Dump() const {
  DUMPTRACE("Plane Information Starts. -------------");
  DUMPTRACE("Plane ID: %d", id_);
  switch (type_) {
    case DRM_PLANE_TYPE_OVERLAY:
      DUMPTRACE("Type: Overlay.");
      break;
    case DRM_PLANE_TYPE_PRIMARY:
      DUMPTRACE("Type: Primary.");
      break;
    case DRM_PLANE_TYPE_CURSOR:
      DUMPTRACE("Type: Cursor.");
      break;
    default:
      ETRACE("Invalid plane type %d", type_);
  }

  for (uint32_t j = 0; j < supported_formats_.size(); j++)
    DUMPTRACE("Format: %4.4s", (char*)&supported_formats_[j]);

  DUMPTRACE("Enabled: %d", in_use_);

  if (alpha_prop_.id != 0)
    DUMPTRACE("Alpha property is supported.");

  if (rotation_prop_.id != 0)
    DUMPTRACE("Rotation property is supported.");

  if (crtc_prop_.id != 0)
    DUMPTRACE("CRTC_ID property is supported.");

  if (fb_prop_.id != 0)
    DUMPTRACE("FB_ID property is supported.");

  if (crtc_x_prop_.id != 0)
    DUMPTRACE("CRTC_X property is supported.");

  if (crtc_y_prop_.id != 0)
    DUMPTRACE("CRTC_Y property is supported.");

  if (crtc_w_prop_.id != 0)
    DUMPTRACE("CRTC_W property is supported.");

  if (crtc_h_prop_.id != 0)
    DUMPTRACE("CRTC_H property is supported.");

  if (src_x_prop_.id != 0)
    DUMPTRACE("SRC_X property is supported.");

  if (src_y_prop_.id != 0)
    DUMPTRACE("SRC_Y property is supported.");

  if (src_w_prop_.id != 0)
    DUMPTRACE("SRC_W property is supported.");

  if (src_h_prop_.id != 0)
    DUMPTRACE("SRC_H property is supported.");

  if (in_fence_fd_prop_.id != 0)
    DUMPTRACE("IN_FENCE_FD is supported.");

  if (in_formats_prop_.id != 0)
    DUMPTRACE("IN_FORMATS property is supported.");

  DUMPTRACE("Preferred Video Format: %4.4s", (char*)&(prefered_video_format_));
  DUMPTRACE("Preferred Video Format: %4.4s", (char*)&(prefered_format_));

  DUMPTRACE("Plane Information Ends. -------------");
}

}  // namespace hwcomposer
