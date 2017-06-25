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

#include <gpudevice.h>

namespace hwcomposer {

GpuDevice::GpuDevice() : initialized_(false) {
}

GpuDevice::~GpuDevice() {
  display_manager_.reset(nullptr);
}

bool GpuDevice::Initialize() {
  if (initialized_)
    return true;

  initialized_ = true;
  display_manager_.reset(DisplayManager::CreateDisplayManager());

  return display_manager_->Initialize();
}

NativeDisplay *GpuDevice::GetDisplay(uint32_t display_id) {
  return display_manager_->GetDisplay(display_id);
}

NativeDisplay *GpuDevice::GetVirtualDisplay() {
  return display_manager_->GetVirtualDisplay();
}

std::vector<NativeDisplay *> GpuDevice::GetConnectedPhysicalDisplays() {
  return display_manager_->GetConnectedPhysicalDisplays();
}

void GpuDevice::RegisterHotPlugEventCallback(
    std::shared_ptr<DisplayHotPlugEventCallback> callback) {
  display_manager_->RegisterHotPlugEventCallback(callback);
}

}  // namespace hwcomposer
