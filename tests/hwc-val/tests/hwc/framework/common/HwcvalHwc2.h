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

#ifndef __Hwc2Iface_h__
#define __Hwc2Iface_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include "HwcTestDefs.h"

#include "HwcTestKernel.h"
#include "DrmShimBuffer.h"
#include <utils/Vector.h>

#define EXPORT_API __attribute__((visibility("default")))

namespace Hwcval {
class EXPORT_API Hwc2 {
 private:
  /// Pointer to test state
  HwcTestState* mState;
  HwcTestKernel* mTestKernel;

  // Layer validity is stored separately as it is required in onPrepare
  std::vector<Hwcval::ValidityType> mLayerValidity[HWCVAL_MAX_CRTCS];

  // Current layer lists in the main thread
  Hwcval::LayerList* mContent[HWCVAL_MAX_CRTCS];

  // OnSet sequence number for validation
  uint32_t mHwcFrame[3];

  // Number of displays with content in OnSet
  uint32_t mActiveDisplays;

 public:
  //-----------------------------------------------------------------------------
  // Constructor & Destructor
  Hwc2();

  // Public interface used by the test

  //EXPORT_API void CheckValidateDisplayEntry(size_t numDisplays,
  //                                    hwcval_display_contents_t** displays);
 EXPORT_API void CheckValidateDisplayEntry(hwc2_display_t display);
  //EXPORT_API void CheckValidateDisplayExit(size_t numDisplays,
 //                                    hwcval_display_contents_t** displays);
  EXPORT_API void CheckValidateDisplayExit();

  /// Notify entry to PresentDisplay from HWC Shim
  //void EXPORT_API
  //    CheckPresentDisplayEnter(size_t numDisplays, hwcval_display_contents_t** displays);
  void EXPORT_API
      CheckPresentDisplayEnter(hwcval_display_contents_t* displays, hwc2_display_t display);

  /// Notify exit from PresentDisplay from HWC Shim, and perform checks
  //EXPORT_API void CheckPresentDisplayExit(size_t numDisplays,
  //                             hwcval_display_contents_t** displays);
  EXPORT_API void CheckPresentDisplayExit(hwcval_display_contents_t* displays, hwc2_display_t display, int32_t *outPresentFence);
  EXPORT_API void GetDisplayConfigsExit(int disp, uint32_t* configs,
                                         uint32_t numConfigs);
  EXPORT_API void GetDisplayAttributesExit(uint32_t disp, uint32_t config, const int32_t attribute, int32_t* values);
};
}

#endif  // __Hwc2Iface_h__
