/*
// Copyright (c) 2018 Intel Corporation
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

#include "DrmShimPropertyManager.h"
#include "HwcTestState.h"
#include "DrmShimChecks.h"
#include <string.h>

#undef LOG_TAG
#define LOG_TAG "DRM_SHIM"

// External references to pointers to real DRM functions
extern void* (*fpDrmMalloc)(int size);

extern drmModeObjectPropertiesPtr (*fpDrmModeObjectGetProperties)(
    int fd, uint32_t object_id, uint32_t object_type);

extern void (*fpDrmModeFreeObjectProperties)(drmModeObjectPropertiesPtr ptr);

extern drmModePropertyPtr (*fpDrmModeGetProperty)(int fd, uint32_t propertyId);

DrmShimPropertyManager::PropInfo::PropInfo(const char* n,
                                           HwcTestKernel::ObjectClass c)
    : mName(n), mClass(c) {
}

#define DECLARE_PLANE_PROPERTY_2(ID, NAME) \
  PropInfo(NAME, HwcTestKernel::ePlane),
#define DECLARE_PLANE_PROPERTY(X) PropInfo(#X, HwcTestKernel::ePlane),
#define DECLARE_CRTC_PROPERTY(X) PropInfo(#X, HwcTestKernel::eCrtc),
DrmShimPropertyManager::PropInfo DrmShimPropertyManager::mInfo[] = {
#include "DrmShimPropertyList.h"
    PropInfo()};
#undef DECLARE_PLANE_PROPERTY_2
#undef DECLARE_PLANE_PROPERTY
#undef DECLARE_CRTC_PROPERTY

const uint32_t DrmShimPropertyManager::mNumSpoofProperties =
    Hwcval::PropertyManager::eDrmPropLast - HWCVAL_SPOOF_PROPERTY_OFFSET;

DrmShimPropertyManager::DrmShimPropertyManager() : mFd(0) {
}

DrmShimPropertyManager::~DrmShimPropertyManager() {
}

drmModeObjectPropertiesPtr DrmShimPropertyManager::ObjectGetProperties(
    int fd, uint32_t objectId, uint32_t objectType) {
  HWCLOGV_COND(eLogNuclear,
               "DrmShimPropertyManager::ObjectGetProperties fd %d objectId %d "
               "objectType 0x%x",
               fd, objectId, objectType);
  drmModeObjectPropertiesPtr props =
      fpDrmModeObjectGetProperties(fd, objectId, objectType);

  return props;
}

void DrmShimPropertyManager::ProcessConnectorProperties(
    uint32_t connId, drmModeObjectPropertiesPtr props) {

  std::map<uint32_t, uint32_t>::iterator itr = mDRRSPropIds.find(connId);
  if (itr != mDRRSPropIds.end()) {
    uint32_t drrsPropId = itr->first;

    for (uint32_t i = 0; i < props->count_props; ++i) {
      if (props->props[i] == drrsPropId) {
        // This is the DRRS property
        if (HwcTestState::getInstance()->IsOptionEnabled(eOptSpoofDRRS)) {
          // We want to spoof, so force the property on
          props->prop_values[i] = HWCVAL_SEAMLESS_DRRS_SUPPORT;
        }
      }
    }
  }
}

drmModePropertyPtr DrmShimPropertyManager::GetProperty(int fd,
                                                       uint32_t propertyId) {
  HWCLOGV_COND(eLogNuclear,
               "DrmShimPropertyManager::GetProperty fd %d propertyId 0x%x", fd,
               propertyId);

  if ((propertyId < HWCVAL_SPOOF_PROPERTY_OFFSET) ||
      (propertyId > eDrmPropLast)) {
    // Property id out of spoof range - use normal GetProperty
    drmModePropertyPtr prop = fpDrmModeGetProperty(fd, propertyId);

    if (prop) {
      HWCLOGV_COND(
          eLogNuclear,
          "DrmShimPropertyManager::GetProperty prop %d %s is not spoofed",
          propertyId, prop->name);
    } else {
      HWCLOGV_COND(eLogNuclear,
                   "DrmShimPropertyManager::GetProperty prop %d not spoofed, "
                   "returns NULL",
                   propertyId);
    }

    return prop;
  } else {
    uint32_t ix = propertyId - HWCVAL_SPOOF_PROPERTY_OFFSET;

    HWCLOGV_COND(
        eLogNuclear,
        "DrmShimPropertyManager::GetProperty prop 0x%x spoofed prop ix %d",
        propertyId, ix);
    drmModePropertyPtr prop =
        (drmModePropertyPtr)fpDrmMalloc(sizeof(drmModePropertyRes));
    prop->prop_id = propertyId;
    const char* name = mInfo[ix].mName;
    strcpy(prop->name, name);

    HWCLOGV_COND(
        eLogNuclear,
        "DrmShimPropertyManager::GetProperty name %s returning prop @%p", name,
        prop);
    return prop;
  }
}

Hwcval::PropertyManager::PropType DrmShimPropertyManager::PropIdToType(
    uint32_t propId, HwcTestKernel::ObjectClass& propClass) {
  if ((propId >= HWCVAL_SPOOF_PROPERTY_OFFSET) && (propId < eDrmPropLast)) {
    // It's already one of our spoof properties, so just return the value
    HWCLOGV_COND(eLogNuclear,
                 "DrmShimPropertyManager::PropIdToType passthrough 0x%x",
                 propId);
    propClass = mInfo[propId - HWCVAL_SPOOF_PROPERTY_OFFSET].mClass;
    return (PropType)propId;
  } else {
    // This is a "real" DRM property
    // So get the property name and look it up in our list to obtain the enum.
    ALOG_ASSERT(mFd);
    drmModePropertyPtr prop = fpDrmModeGetProperty(mFd, propId);

    for (uint32_t i = 0; i < mNumSpoofProperties; ++i) {
      if (strcmp(prop->name, mInfo[i].mName) == 0) {
        propClass = mInfo[i].mClass;
        HWCLOGV_COND(eLogNuclear,
                     "DrmShimPropertyManager::PropIdToType %d %s -> offset %d",
                     propId, mInfo[i].mName, i);
        return static_cast<Hwcval::PropertyManager::PropType>(
            HWCVAL_SPOOF_PROPERTY_OFFSET + i);
      }
    }

    return Hwcval::PropertyManager::eDrmPropNone;
  }
}

const char* DrmShimPropertyManager::GetName(PropType pt) {
  if ((pt >= HWCVAL_SPOOF_PROPERTY_OFFSET) && (pt < eDrmPropLast)) {
    return mInfo[pt - HWCVAL_SPOOF_PROPERTY_OFFSET].mName;
  } else {
    // Property id out of spoof range - use normal GetProperty
    drmModePropertyPtr prop = fpDrmModeGetProperty(mFd, pt);

    if (prop) {
      static char propName[256];
      strcpy(propName, "Real DRM property: ");
      strcat(propName, prop->name);
      return propName;
    }

    return "Real DRM property";
  }
}

void DrmShimPropertyManager::SetFd(int fd) {
  mFd = fd;
}

void DrmShimPropertyManager::CheckConnectorProperties(
    uint32_t connId, uint32_t& connectorAttributes) {
  drmModeObjectPropertiesPtr props = nullptr;

  props = drmModeObjectGetProperties(mFd, connId, DRM_MODE_OBJECT_CONNECTOR);
  ALOG_ASSERT(props);
  ProcessConnectorProperties(connId, props);

  // Find the Id of the property
  for (uint32_t i = 0; i < props->count_props; ++i) {
    drmModePropertyPtr prop = drmModeGetProperty(mFd, props->props[i]);
    ALOG_ASSERT(prop);

    if (strcmp(prop->name, "ddr_freq") == 0) {
      connectorAttributes |= DrmShimChecks::eDDRFreq;
    } else if (strcmp(prop->name, "drrs_capability") == 0) {
      // Determine property setting for validation
      switch (props->prop_values[i]) {
        case HWCVAL_SEAMLESS_DRRS_SUPPORT:
        case HWCVAL_SEAMLESS_DRRS_SUPPORT_SW:
          connectorAttributes |= DrmShimChecks::eDRRS;
      }

      // Save the DRRS property ID, so when HWC asks for it we can change the
      // value
      mDRRSPropIds[connId] = props->props[i];
    }

    drmModeFreeProperty(prop);
  }
}

int32_t DrmShimPropertyManager::GetPlanePropertyId(uint32_t plane_id,
                                                   const char* prop_name) {
  drmModeObjectPropertiesPtr props = nullptr;
  int32_t prop_id = -1;

  props = drmModeObjectGetProperties(mFd, plane_id, DRM_MODE_OBJECT_PLANE);
  ALOG_ASSERT(props);

  // Find the Id of the property
  for (uint32_t i = 0; i < props->count_props; ++i) {
    drmModePropertyPtr prop = nullptr;
    prop = drmModeGetProperty(mFd, props->props[i]);
    ALOG_ASSERT(prop);

    if (!strcmp(prop->name, "type")) {
      HWCLOGV_COND(eLogNuclear,
                   "DrmShimPropertyManager::GetPlanePropertyId - %s property "
                   "for plane %d is: %d",
                   prop_name, plane_id, prop->prop_id);
      prop_id = prop->prop_id;
      break;
    }

    drmModeFreeProperty(prop);
  }

  return prop_id;
}

int32_t DrmShimPropertyManager::GetPlaneType(uint32_t plane_id) {
  drmModeObjectPropertiesPtr props = nullptr;

  int32_t prop_id = GetPlanePropertyId(plane_id, "type");
  if (prop_id == -1) {
    HWCLOGV_COND(eLogNuclear,
                 "DrmShimPropertyManager::GetPlaneType - could not find id for "
                 "'type' property");
    return -1;
  }

  // Get a pointer to the properties and look for the plane type
  props = drmModeObjectGetProperties(mFd, plane_id, DRM_MODE_OBJECT_PLANE);
  if (!props) {
    HWCLOGV_COND(
        eLogNuclear,
        "DrmShimPropertyManager::GetPlaneType - could not get properties");
    return -1;
  }

  int32_t plane_type = -1;
  for (uint32_t i = 0; i < props->count_props; ++i) {
    if (props->props[i] == (uint32_t)prop_id) {
      HWCLOGV_COND(eLogNuclear,
                   "DrmShimPropertyManager::GetPlaneType - 'type' property for "
                   "plane %d has value: %d",
                   plane_id, props->prop_values[i]);
      plane_type = props->prop_values[i];
      break;
    }
  }

  drmModeFreeObjectProperties(props);
  return plane_type;
}
