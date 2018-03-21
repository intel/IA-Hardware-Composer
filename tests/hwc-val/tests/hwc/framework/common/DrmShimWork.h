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

#ifndef __DrmShimWork_h__
#define __DrmShimWork_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include "HwcvalWork.h"

class HwcTestState;
class DrmShimBuffer;
class HwcTestKernel;

namespace Hwcval {
namespace Work {
class AddFbItem : public Item {
 public:
  uint32_t mBoHandle;
  uint32_t mFbId;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mPixelFormat;

  uint32_t mAuxPitch;
  uint32_t mAuxOffset;
  bool mHasAuxBuffer;
  __u64 mModifier;

 public:
  AddFbItem(int fd, uint32_t boHandle, uint32_t fbId, uint32_t width,
            uint32_t height, uint32_t pixelFormat);
  AddFbItem(int fd, uint32_t boHandle, uint32_t fbId, uint32_t width,
            uint32_t height, uint32_t pixelFormat, uint32_t auxPitch,
            uint32_t auxOffset, __u64 modifier);
  virtual ~AddFbItem();
  virtual void Process();
};

class RmFbItem : public Item {
 public:
  uint32_t mFbId;

 public:
  RmFbItem(int fd, uint32_t fbId);
  virtual ~RmFbItem();
  virtual void Process();
};

}  // namespace Work
}  // namespace Hwcval

#endif  // __DrmShimWork_h__
