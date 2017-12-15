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

#ifndef COMMON_COMPOSITOR_VA_VASURFACE_H_
#define COMMON_COMPOSITOR_VA_VASURFACE_H_

#include "nativesurface.h"

#include <va/va.h>
#include <va/va_vpp.h>

namespace hwcomposer {

class VASurface : public NativeSurface {
 public:
  VASurface() = default;
  ~VASurface() override;
  VASurface(uint32_t width, uint32_t height);

  bool MakeCurrent() override;
  const VASurfaceID& GetSurfaceID() const {
    return surface_;
  }

  VARectangle* GetOutputRegion() {
    return &output_region_;
  }

  bool CreateVASurface(void* va_display);

 private:
  VADisplay display_;
  VASurfaceID surface_ = VA_INVALID_ID;
  VARectangle output_region_;
  uint32_t previous_width_ = 0;
  uint32_t previous_height_ = 0;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_VA_VASURFACE_H_
