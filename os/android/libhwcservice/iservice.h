/*
// Copyright (c) 2017 Intel Corporation
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

#ifndef OS_ANDROID_ISERVICE_H_
#define OS_ANDROID_ISERVICE_H_

#include <binder/IInterface.h>
#include <binder/Parcel.h>

#define IA_HWC_SERVICE_NAME "hwc.info"

namespace hwcomposer {

class IControls;
class IDiagnostic;

using namespace android;

/** Maintenance interface to control HWC activity.
 */
class IService : public IInterface {
 public:
  DECLARE_META_INTERFACE(Service);

  virtual sp<IDiagnostic> GetDiagnostic() = 0;
  virtual sp<IControls> GetControls() = 0;

  virtual String8 GetHwcVersion() = 0;
  virtual void DumpOptions(void) = 0;
  virtual status_t SetOption(String8 option, String8 optionValue) = 0;
  virtual status_t EnableLogviewToLogcat(bool enable = true) = 0;
};

/**
 */
class BnService : public BnInterface<IService> {
 public:
  status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t) override;
};

}  // namespace hwcomposer

#endif  // OS_ANDROID_ISERVICE_H_
