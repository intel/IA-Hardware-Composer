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

#include "displayplane.h"

#include <drm_fourcc.h>

#include <gpudevice.h>

#include "hwctrace.h"
#include "overlaylayer.h"

namespace hwcomposer {

DisplayPlane::Property::Property() {
}

bool DisplayPlane::Property::Initialize(
    uint32_t fd, const char* name,
    const ScopedDrmObjectPropertyPtr& plane_props) {
  uint32_t count_props = plane_props->count_props;
  for (uint32_t i = 0; i < count_props; i++) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(fd, plane_props->props[i]));
    if (property && !strcmp(property->name, name)) {
      id = property->prop_id;
      break;
    }
  }
  if (!id) {
    ETRACE("Could not find property %s", name);
    return false;
  }
  return true;
}

DisplayPlane::DisplayPlane(uint32_t plane_id, uint32_t possible_crtcs)
    : id_(plane_id),
      possible_crtc_mask_(possible_crtcs),
      last_valid_format_(0),
      enabled_(false) {
}

DisplayPlane::~DisplayPlane() {
}

bool DisplayPlane::Initialize(uint32_t gpu_fd,
                              const std::vector<uint32_t>& formats) {
  supported_formats_ = formats;

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

  ret = rotation_prop_.Initialize(gpu_fd, "rotation", plane_props);
  if (!ret)
    ETRACE("Could not get rotation property");

  ret = alpha_prop_.Initialize(gpu_fd, "alpha", plane_props);
  if (!ret)
    ETRACE("Could not get alpha property");

#ifndef DISABLE_EXPLICIT_SYNC
  ret = in_fence_fd_prop_.Initialize(gpu_fd, "IN_FENCE_FD", plane_props);
  if (!ret)
    ETRACE("Could not get IN_FENCE_FD property");
#else
  in_fence_fd_prop_.id = 0;
#endif

  return true;
}

bool DisplayPlane::UpdateProperties(drmModeAtomicReqPtr property_set,
                                    uint32_t crtc_id,
                                    const OverlayLayer* layer) const {
  uint64_t alpha = 0xFF;
  OverlayBuffer* buffer = layer->GetBuffer();
  const HwcRect<int>& display_frame = layer->GetDisplayFrame();
  const HwcRect<float>& source_crop = layer->GetSourceCrop();
  int fence = layer->GetAcquireFence();
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
  if (type_ == DRM_PLANE_TYPE_CURSOR) {
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_w_prop_.id,
                                        buffer->GetWidth()) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_h_prop_.id,
                                        buffer->GetHeight()) < 0;
  } else {
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_w_prop_.id,
                                        layer->GetDisplayFrameWidth()) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, crtc_h_prop_.id,
                                        layer->GetDisplayFrameHeight()) < 0;
  }

  success |=
      drmModeAtomicAddProperty(property_set, id_, src_x_prop_.id,
                               static_cast<int>(source_crop.left) << 16) < 0;
  success |=
      drmModeAtomicAddProperty(property_set, id_, src_y_prop_.id,
                               static_cast<int>(source_crop.top) << 16) < 0;
  if (type_ == DRM_PLANE_TYPE_CURSOR) {
    success |= drmModeAtomicAddProperty(property_set, id_, src_w_prop_.id,
                                        buffer->GetWidth() << 16) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, src_h_prop_.id,
                                        buffer->GetHeight() << 16) < 0;
  } else {
    success |= drmModeAtomicAddProperty(property_set, id_, src_w_prop_.id,
                                        layer->GetSourceCropWidth() << 16) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, src_h_prop_.id,
                                        layer->GetSourceCropHeight() << 16) < 0;
  }

  if (rotation_prop_.id) {
    success = drmModeAtomicAddProperty(property_set, id_, rotation_prop_.id,
                                       layer->GetRotation()) < 0;
  }

  if (alpha_prop_.id) {
    success =
        drmModeAtomicAddProperty(property_set, id_, alpha_prop_.id, alpha) < 0;
  }
#ifndef DISABLE_EXPLICIT_SYNC
  if (fence != -1 && in_fence_fd_prop_.id) {
    success = drmModeAtomicAddProperty(property_set, id_, in_fence_fd_prop_.id,
                                       fence) < 0;
  }
#endif

  if (success) {
    ETRACE("Could not update properties for plane with id: %d", id_);
    return false;
  }
  IDISPLAYMANAGERTRACE("buffer->GetFb() ---------------------- ENDS%d",
                       buffer->GetFb());
  return true;
}

bool DisplayPlane::Disable(drmModeAtomicReqPtr property_set) {
  enabled_ = false;
  int success =
      drmModeAtomicAddProperty(property_set, id_, crtc_prop_.id, 0) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, fb_prop_.id, 0) < 0;

  if (success) {
    ETRACE("Failed to disable plane with id: %d", id_);
    return false;
  }

  return true;
}

uint32_t DisplayPlane::id() const {
  return id_;
}

bool DisplayPlane::GetCrtcSupported(uint32_t pipe_id) const {
  return !!((1 << pipe_id) & possible_crtc_mask_);
}

void DisplayPlane::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

uint32_t DisplayPlane::type() const {
  return type_;
}

bool DisplayPlane::ValidateLayer(const OverlayLayer* layer) {
  uint64_t alpha = 0xFF;

  if (layer->GetBlending() == HWCBlending::kBlendingPremult)
    alpha = layer->GetAlpha();

  if (type_ == DRM_PLANE_TYPE_OVERLAY && (alpha != 0 && alpha != 0xFF) &&
      alpha_prop_.id == 0) {
    IDISPLAYMANAGERTRACE(
        "Alpha property not supported, Cannot composite layer using Overlay.");
    return false;
  }

  if (layer->GetRotation() && rotation_prop_.id == 0) {
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

  return true;
}

bool DisplayPlane::IsSupportedFormat(uint32_t format) {
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

uint32_t DisplayPlane::GetFormatForFrameBuffer(uint32_t format) {
  if (IsSupportedFormat(format))
    return format;

  if (type_ == DRM_PLANE_TYPE_PRIMARY) {
    // In case of alpha, fall back to XRGB.
    switch (format) {
      case DRM_FORMAT_ABGR8888:
        return DRM_FORMAT_XBGR8888;
      case DRM_FORMAT_ARGB8888:
        return DRM_FORMAT_XRGB8888;
      default:
        break;
    }
  }

  return format;
}

void DisplayPlane::Dump() const {
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

  DUMPTRACE("Enabled: %d", enabled_);

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

  DUMPTRACE("Plane Information Ends. -------------");
}

}  // namespace hwcomposer
