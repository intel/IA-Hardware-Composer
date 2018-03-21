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

#ifndef __HwcvalPropertyManager_h__
#define __HwcvalPropertyManager_h__

#include "HwcTestKernel.h"
class DrmShimChecks;

// We are assuming that DRM will not create properties with ids in the range
// (spoofProprtyOffset) to (spoofPropertyOffset + numberOfProperties)
#define HWCVAL_SPOOF_PROPERTY_OFFSET 0x12340000

namespace Hwcval {

class PropertyManager {
 public:
  PropertyManager() : mChecks(0) {
  }

  virtual ~PropertyManager() {
  }

// Generate enum
#define DECLARE_PLANE_PROPERTY_2(ID, NAME) eDrmPlaneProp_##ID,
#define DECLARE_PLANE_PROPERTY(X) eDrmPlaneProp_##X,
#define DECLARE_CRTC_PROPERTY(X) eDrmCrtcProp_##X,
  enum PropType {
    eDrmPropNone = HWCVAL_SPOOF_PROPERTY_OFFSET - 1,
#include "DrmShimPropertyList.h"
    eDrmPropLast
  };
#undef DECLARE_CRTC_PROPERTY
#undef DECLARE_PLANE_PROPERTY
#undef DECLARE_PLANE_PROPERTY_2

  virtual void CheckConnectorProperties(uint32_t connId,
                                        uint32_t& attributes) = 0;
  virtual PropType PropIdToType(uint32_t propId,
                                HwcTestKernel::ObjectClass& propClass) = 0;
  virtual const char* GetName(PropType pt) = 0;
  virtual int32_t GetPlaneType(uint32_t plane_id) {
    HWCVAL_UNUSED(plane_id);
    return -1;
  }
  void SetTestKernel(DrmShimChecks* testKernel);

 protected:
  DrmShimChecks* mChecks;
  bool mDRRS;
};

inline void PropertyManager::SetTestKernel(DrmShimChecks* checks) {
  HWCLOGV_COND(eLogNuclear, "Hwcval::PropertyManager has DrmShimChecks @%p",
               checks);
  mChecks = checks;
}
}

#endif  // __HwcvalPropertyManager_h__
