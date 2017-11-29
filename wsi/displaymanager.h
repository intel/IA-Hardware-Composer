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

#ifndef WSI_MANAGER_H_
#define WSI_MANAGER_H_

#include <stdint.h>

#include "nativedisplay.h"

namespace hwcomposer {

class DisplayManager {
 public:
  static DisplayManager *CreateDisplayManager();
  DisplayManager() = default;
  virtual ~DisplayManager() {
  }

  virtual bool Initialize() = 0;

  virtual NativeDisplay *GetVirtualDisplay() = 0;
  virtual NativeDisplay *GetNestedDisplay() = 0;

  virtual std::vector<NativeDisplay *> GetAllDisplays() = 0;

  virtual void RegisterHotPlugEventCallback(
      std::shared_ptr<DisplayHotPlugEventCallback> callback) = 0;
};

}  // namespace hwcomposer
#endif  // WSI_MANAGER_H_
