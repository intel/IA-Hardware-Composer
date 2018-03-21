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

#ifndef __HwcShimInitializer_h__
#define __HwcShimInitializer_h__

class HwcShimInitializer {
 public:
  /// struct of pointer to drm shim function as hwc is linked against real
  /// drm
  typedef struct {
    /// pointer to drm shim drmShimInit
    void (*fpDrmShimInit)(bool isHwc, bool isDrm);
    void (*fpDrmShimEnableVSyncInterception)(bool intercept);
    void (*fpDrmShimRegisterCallback)(void* cbk);

  } drmShimFunctionsType;

  /// struct of pointer to real drm functions
  drmShimFunctionsType drmShimFunctions;

 public:
  virtual ~HwcShimInitializer() {
  }

  // pointer to HWC State
  HwcTestState* state;

  /// Complete initialization of shim in DRM mode
  virtual void HwcShimInitDrm(void) = 0;
};

#endif  // __HwcShimInitializer_h__
