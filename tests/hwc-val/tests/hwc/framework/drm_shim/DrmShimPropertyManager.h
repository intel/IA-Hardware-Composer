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

#ifndef __DrmShimPropertyManager_h__
#define __DrmShimPropertyManager_h__

#include "HwcvalPropertyManager.h"
#include "HwcvalPropertyManager.h"

extern "C" {
#include <xf86drm.h>      //< For structs and types.
#include <xf86drmMode.h>  //< For structs and types.
};

class DrmShimChecks;

class DrmShimPropertyManager : public Hwcval::PropertyManager {
 public:
  DrmShimPropertyManager();
  virtual ~DrmShimPropertyManager();

  drmModeObjectPropertiesPtr ObjectGetProperties(int fd, uint32_t objectId,
                                                 uint32_t objectType);

  drmModePropertyPtr GetProperty(int fd, uint32_t propertyId);

  virtual PropType PropIdToType(uint32_t propId,
                                HwcTestKernel::ObjectClass& propClass);
  virtual const char* GetName(PropType pt);

  virtual void CheckConnectorProperties(uint32_t connId,
                                        uint32_t& connectorAttributes);
  int32_t GetPlaneType(uint32_t plane_id);
  int32_t GetPlanePropertyId(uint32_t, const char*);

  void SetFd(int fd);

private:
  void ProcessConnectorProperties(uint32_t connId,
				  drmModeObjectPropertiesPtr props);
  struct PropInfo {
    PropInfo(const char* n = 0,
	     HwcTestKernel::ObjectClass c = HwcTestKernel::ePlane);
    const char* mName;
    HwcTestKernel::ObjectClass mClass;
  };

  static PropInfo mInfo[];
  static const uint32_t mNumSpoofProperties;
  int mFd;

  // DRRS property ID per connector id
  std::map<uint32_t, uint32_t> mDRRSPropIds;
};

#endif  // __DrmShimPropertyManager_h__
