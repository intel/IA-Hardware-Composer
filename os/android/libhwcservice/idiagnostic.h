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

#ifndef OS_ANDROID_HWC_IDIAGNOSTIC_H
#define OS_ANDROID_HWC_IDIAGNOSTIC_H

#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace hwcomposer {

using namespace android;

class IDiagnostic : public IInterface {
 public:
  DECLARE_META_INTERFACE(Diagnostic);

  // This class is shared with the validation team. Take care when changing it
  // that
  // the validation tests understand how we have changed this.
  enum {
    eLogTruncated =
        101,  // Status to indicate log entries have been overwritten
  };

  virtual status_t ReadLogParcel(android::Parcel* reply) = 0;

  // Debug API
  virtual void EnableDisplay(uint32_t d) = 0;
  virtual void DisableDisplay(uint32_t d, bool bBlank) = 0;
  virtual void MaskLayer(uint32_t d, uint32_t layer, bool bHide) = 0;
  virtual void DumpFrames(uint32_t d, int32_t frames, bool bSync) = 0;
};

class BnDiagnostic : public android::BnInterface<IDiagnostic> {
 public:
  BnDiagnostic() {
  }

  virtual ~BnDiagnostic() {
  }

  status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t) override;
};

}  // namespace hwcomposer

#endif  // OS_ANDROID_HWC_IDIAGNOSTIC_H
