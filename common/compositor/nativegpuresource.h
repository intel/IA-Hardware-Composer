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

#ifndef NATIVE_GPU_RESOURCE_H_
#define NATIVE_GPU_RESOURCE_H_

#include <vector>

#include "compositordefs.h"

namespace hwcomposer {

struct OverlayLayer;
class OverlayBuffer;

class NativeGpuResource {
 public:
  NativeGpuResource() = default;
  virtual ~NativeGpuResource() {
  }

  NativeGpuResource(const NativeGpuResource& rhs) = delete;
  NativeGpuResource(NativeGpuResource&& rhs) = delete;

  NativeGpuResource& operator=(const NativeGpuResource& rhs) = delete;

  NativeGpuResource& operator=(NativeGpuResource&& rhs) = delete;

  virtual bool PrepareResources(const std::vector<OverlayLayer>& layers) = 0;
  virtual GpuResourceHandle GetResourceHandle(uint32_t layer_index) const = 0;
};

}  // namespace hwcomposer
#endif  // NATIVE_GPU_RESOURCE_H_
