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

#ifndef GPU_DEVICE_H_
#define GPU_DEVICE_H_

#include <memory>

#include <vector>

#include <scopedfd.h>

namespace hwcomposer {

class NativeDisplay;

class DisplayHotPlugEventCallback {
 public:
  virtual ~DisplayHotPlugEventCallback() {
  }
  virtual void Callback(std::vector<NativeDisplay*> connected_displays) = 0;
};

class GpuDevice {
 public:
  GpuDevice();

  virtual ~GpuDevice();

  // Open device.
  bool Initialize();

  NativeDisplay* GetDisplay(uint32_t display);

  NativeDisplay* GetVirtualDisplay();

  std::vector<NativeDisplay*> GetConnectedPhysicalDisplays();

  void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback);

 private:
  class DisplayManager;
  std::unique_ptr<DisplayManager> display_manager_;
  ScopedFd fd_;
  bool initialized_;
};

}  // namespace hwcomposer
#endif  // GPU_DEVICE_H_
