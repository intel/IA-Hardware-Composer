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

#include "hwcutils.h"

#include <poll.h>

#include "hwctrace.h"

#include <drm_fourcc.h>

namespace hwcomposer {

int HWCPoll(int fd, int timeout) {
  CTRACE();
  int ret;
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;

  if ((ret = poll(fds, 1, timeout)) <= 0) {
    ETRACE("Poll Failed in HWCPoll %s", PRINTERROR());
  }
  return ret;
}

bool IsLayerAlphaBlendingCommitted(OverlayLayer* layer) {
  uint64_t alpha = 0xFF;

  if (layer->GetBlending() == HWCBlending::kBlendingPremult)
    alpha = layer->GetAlpha();

  if (layer->GetZorder() > 0 && alpha != 0xFF) {
    return false;
  }
  return true;
}

void ResetRectToRegion(const HwcRegion& hwc_region, HwcRect<int>& rect) {
  size_t total_rects = hwc_region.size();
  if (total_rects == 0) {
    rect.left = 0;
    rect.top = 0;
    rect.right = 0;
    rect.bottom = 0;
    return;
  }

  const HwcRect<int>& new_rect = hwc_region.at(0);
  rect.left = new_rect.left;
  rect.top = new_rect.top;
  rect.right = new_rect.right;
  rect.bottom = new_rect.bottom;

  for (uint32_t r = 1; r < total_rects; r++) {
    const HwcRect<int>& temp = hwc_region.at(r);
    rect.left = std::min(rect.left, temp.left);
    rect.top = std::min(rect.top, temp.top);
    rect.right = std::max(rect.right, temp.right);
    rect.bottom = std::max(rect.bottom, temp.bottom);
  }
}

void CalculateRect(const HwcRect<int>& target_rect, HwcRect<int>& new_rect) {
  if (new_rect.empty()) {
    new_rect = target_rect;
    return;
  }

  if (target_rect.empty()) {
    return;
  }

  new_rect.left = std::min(target_rect.left, new_rect.left);
  new_rect.top = std::min(target_rect.top, new_rect.top);
  new_rect.right = std::max(target_rect.right, new_rect.right);
  new_rect.bottom = std::max(target_rect.bottom, new_rect.bottom);
}

void CalculateSourceRect(const HwcRect<float>& target_rect,
                         HwcRect<float>& new_rect) {
  if (new_rect.empty()) {
    new_rect = target_rect;
    return;
  }

  if (target_rect.empty()) {
    return;
  }

  new_rect.left = std::min(target_rect.left, new_rect.left);
  new_rect.top = std::min(target_rect.top, new_rect.top);
  new_rect.right = std::max(target_rect.right, new_rect.right);
  new_rect.bottom = std::max(target_rect.bottom, new_rect.bottom);
}

bool IsSupportedMediaFormat(uint32_t format) {
  switch (format) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_P010:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_AYUV:
    case DRM_FORMAT_NV12_Y_TILED_INTEL:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_YVU420_ANDROID:
      return true;
    default:
      break;
  }

  return false;
}

uint32_t GetTotalPlanesForFormat(uint32_t format) {
  switch (format) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_P010:
      return 2;
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YUV444:
      return 3;
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_AYUV:
      return 1;
    default:
      break;
  }

  return 1;
}

#ifdef KVM_HWC_PROPERTY
bool IsKvmPlatform() {
  const char* key = KVM_HWC_PROPERTY;
  char* value = new char[20];
  const char* property_true = "true";
  int len = property_get(key, value, "");
  if (len > 0 && strcmp(value, property_true) == 0) {
    delete[] value;
    return true;
  } else {
    delete[] value;
    return false;
  }
}
#endif

bool IsEdidFilting() {
  const char* key = ALL_EDID_FLAG_PROPERTY;
  char* value = new char[20];
  const char* property_true = "1";
  int len = property_get(key, value, "0");
  if (len > 0 && strcmp(value, property_true) == 0) {
    delete[] value;
    return false;
  } else {
    delete[] value;
    return true;
  }
}

std::string StringifyRect(HwcRect<int> rect) {
  std::stringstream ss;
  ss << "{(" << rect.left << "," << rect.top << ") "
     << "(" << rect.right << "," << rect.bottom << ")}";

  return ss.str();
}

std::string StringifyRegion(HwcRegion region) {
  std::stringstream ss;
  std::string separator = "";

  ss << "[";
  for (auto& rect : region) {
    ss << separator << StringifyRect(rect);
    separator = ", ";
  }
  ss << "]";

  return ss.str();
}

HwcRect<int> RotateRect(const HwcRect<int>& rect, int disp_width,
                        int disp_height, uint32_t transform) {
  int ox = 0, oy = 0;
  HwcRect<int> rotated_rect;
  if (transform == 0)
    return rect;
  if (transform == hwcomposer::HWCTransform::kTransform270) {
    ox = 0;
    oy = disp_width;
    rotated_rect.left = ox + rect.top;
    rotated_rect.top = oy - rect.right;
    rotated_rect.right = ox + rect.bottom;
    rotated_rect.bottom = oy - rect.left;
  } else if (transform == hwcomposer::HWCTransform::kTransform180) {
    ox = disp_width;
    oy = disp_height;
    rotated_rect.left = ox - rect.right;
    rotated_rect.top = oy - rect.bottom;
    rotated_rect.right = ox - rect.left;
    rotated_rect.bottom = oy - rect.top;
  } else if (transform & hwcomposer::HWCTransform::kTransform90) {
    if (transform & hwcomposer::HWCTransform::kReflectY) {
      ox = 0;
      oy = 0;
      rotated_rect.left = ox + rect.top;
      rotated_rect.top = oy + rect.left;
      rotated_rect.right = ox + rect.bottom;
      rotated_rect.bottom = oy + rect.right;
    } else if (transform & hwcomposer::HWCTransform::kReflectX) {
      ox = disp_height;
      oy = disp_width;
      rotated_rect.left = ox - rect.bottom;
      rotated_rect.top = oy - rect.right;
      rotated_rect.right = ox - rect.top;
      rotated_rect.bottom = oy - rect.left;
    } else {
      ox = disp_height;
      oy = 0;
      rotated_rect.left = ox - rect.bottom;
      rotated_rect.top = oy + rect.left;
      rotated_rect.right = ox - rect.top;
      rotated_rect.bottom = oy + rect.right;
    }
  }
  return rotated_rect;
}

HwcRect<int> ScaleRect(HwcRect<int> rect, float x_scale, float y_scale) {
  rect.left = rect.left * x_scale;
  rect.top = rect.top * y_scale;
  rect.right = rect.right * x_scale;
  rect.bottom = rect.bottom * y_scale;
  return rect;
}

HwcRect<int> RotateScaleRect(HwcRect<int> rect, int width, int height,
                             uint32_t plane_transform) {
  HwcRect<int> rotate_scale_rect =
      RotateRect(rect, width, height, plane_transform);
  if (plane_transform & (hwcomposer::HWCTransform::kTransform270 |
                         hwcomposer::HWCTransform::kTransform90)) {
    float x_scale = float(width) / height;
    float y_scale = float(height) / width;
    rotate_scale_rect = ScaleRect(rotate_scale_rect, x_scale, y_scale);
  }
  return rotate_scale_rect;
}
}  // namespace hwcomposer
