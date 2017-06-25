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

#ifndef WSI_DISPLAYPLANE_H_
#define WSI_DISPLAYPLANE_H_

#include <stdlib.h>
#include <stdint.h>

namespace hwcomposer {

struct OverlayLayer;

class DisplayPlane {
 public:
  virtual ~DisplayPlane() {
  }

  virtual uint32_t id() const = 0;
  virtual void SetEnabled(bool enabled) = 0;

  virtual bool IsEnabled() const = 0;

  virtual bool ValidateLayer(const OverlayLayer* layer) = 0;

  virtual bool IsSupportedFormat(uint32_t format) = 0;

  virtual uint32_t GetFormatForFrameBuffer(uint32_t format) = 0;

  virtual void Dump() const = 0;
};

}  // namespace hwcomposer
#endif  // WSI_DISPLAYPLANE_H_
