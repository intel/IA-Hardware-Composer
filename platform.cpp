/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "hwc-platform"

#include "platform.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

std::vector<DrmPlane *> Planner::GetUsablePlanes(
    DrmCrtc *crtc, std::vector<DrmPlane *> *primary_planes,
    std::vector<DrmPlane *> *overlay_planes) {
  std::vector<DrmPlane *> usable_planes;
  std::copy_if(primary_planes->begin(), primary_planes->end(),
               std::back_inserter(usable_planes),
               [=](DrmPlane *plane) { return plane->GetCrtcSupported(*crtc); });
  std::copy_if(overlay_planes->begin(), overlay_planes->end(),
               std::back_inserter(usable_planes),
               [=](DrmPlane *plane) { return plane->GetCrtcSupported(*crtc); });
  return usable_planes;
}

int Planner::PlanStage::ValidatePlane(DrmPlane *plane, DrmHwcLayer *layer) {
  int ret = 0;
  uint64_t blend;

  if ((plane->rotation_property().id() == 0) &&
      layer->transform != DrmHwcTransform::kIdentity) {
    ALOGE("Rotation is not supported on plane %d", plane->id());
    return -EINVAL;
  }

  if (plane->alpha_property().id() == 0 && layer->alpha != 0xffff) {
    ALOGE("Alpha is not supported on plane %d", plane->id());
    return -EINVAL;
  }

  if (plane->blend_property().id() == 0) {
    if ((layer->blending != DrmHwcBlending::kNone) &&
        (layer->blending != DrmHwcBlending::kPreMult)) {
      ALOGE("Blending is not supported on plane %d", plane->id());
      return -EINVAL;
    }
  } else {
    switch (layer->blending) {
      case DrmHwcBlending::kPreMult:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "Pre-multiplied");
        break;
      case DrmHwcBlending::kCoverage:
        std::tie(blend, ret) = plane->blend_property().GetEnumValueWithName(
            "Coverage");
        break;
      case DrmHwcBlending::kNone:
      default:
        std::tie(blend,
                 ret) = plane->blend_property().GetEnumValueWithName("None");
        break;
    }
    if (ret)
      ALOGE("Expected a valid blend mode on plane %d", plane->id());
  }

  return ret;
}

std::tuple<int, std::vector<DrmCompositionPlane>> Planner::ProvisionPlanes(
    std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
    std::vector<DrmPlane *> *primary_planes,
    std::vector<DrmPlane *> *overlay_planes) {
  std::vector<DrmCompositionPlane> composition;
  std::vector<DrmPlane *> planes = GetUsablePlanes(crtc, primary_planes,
                                                   overlay_planes);
  if (planes.empty()) {
    ALOGE("Display %d has no usable planes", crtc->display());
    return std::make_tuple(-ENODEV, std::vector<DrmCompositionPlane>());
  }

  // Go through the provisioning stages and provision planes
  for (auto &i : stages_) {
    int ret = i->ProvisionPlanes(&composition, layers, crtc, &planes);
    if (ret) {
      ALOGE("Failed provision stage with ret %d", ret);
      return std::make_tuple(ret, std::vector<DrmCompositionPlane>());
    }
  }

  return std::make_tuple(0, std::move(composition));
}

int PlanStageProtected::ProvisionPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
    std::vector<DrmPlane *> *planes) {
  int ret;
  int protected_zorder = -1;
  for (auto i = layers.begin(); i != layers.end();) {
    if (!i->second->protected_usage()) {
      ++i;
      continue;
    }

    ret = Emplace(composition, planes, DrmCompositionPlane::Type::kLayer, crtc,
                  std::make_pair(i->first, i->second));
    if (ret) {
      ALOGE("Failed to dedicate protected layer! Dropping it.");
      return ret;
    }

    protected_zorder = i->first;
    i = layers.erase(i);
  }

  return 0;
}

int PlanStageGreedy::ProvisionPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
    std::vector<DrmPlane *> *planes) {
  // Fill up the remaining planes
  for (auto i = layers.begin(); i != layers.end(); i = layers.erase(i)) {
    int ret = Emplace(composition, planes, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second));
    // We don't have any planes left
    if (ret == -ENOENT)
      break;
    else if (ret) {
      ALOGE("Failed to emplace layer %zu, dropping it", i->first);
      return ret;
    }
  }

  return 0;
}
}  // namespace android
