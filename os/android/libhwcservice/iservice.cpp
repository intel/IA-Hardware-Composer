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

#include "iservice.h"
#include "icontrols.h"
#include "idiagnostic.h"

#include <utils/String8.h>

namespace hwcomposer {

using namespace android;

/**
 */
class BpService : public BpInterface<IService> {
 public:
  BpService(const sp<IBinder>& impl) : BpInterface<IService>(impl) {
  }

  enum {
    // ==============================================
    // Public APIs - try not to reorder these

    GET_HWC_VERSION = IBinder::FIRST_CALL_TRANSACTION,

    // Dump options and current settings to logcat.
    DUMP_OPTIONS,

    // Override an option.
    SET_OPTION,

    // Disable hwc logviewer output to logcat
    DISABLE_LOG_TO_LOGCAT = 98,
    // Enable hwclogviewer output to logcat
    ENABLE_LOG_TO_LOGCAT = 99,

    // accessor for IBinder interface functions
    TRANSACT_GET_DIAGNOSTIC = 100,
    TRANSACT_GET_CONTROLS,
  };

  virtual String8 GetHwcVersion() {
    Parcel data, reply;
    data.writeInterfaceToken(IService::getInterfaceDescriptor());
    remote()->transact(GET_HWC_VERSION, data, &reply);
    String8 ret = reply.readString8();
    return ret;
  }

  virtual void dumpOptions(void) {
    Parcel data, reply;
    remote()->transact(DUMP_OPTIONS, data, &reply);
  }

  virtual status_t setOption(String8 option, String8 optionValue) {
    Parcel data, reply;
    data.writeInterfaceToken(IService::getInterfaceDescriptor());
    data.writeString16(String16(option));
    data.writeString16(String16(optionValue));
    remote()->transact(SET_OPTION, data, &reply);
    status_t ret = reply.readInt32();
    return ret;
  }

  virtual status_t enableLogviewToLogcat(bool enable = true) {
    Parcel data, reply;
    data.writeInterfaceToken(IService::getInterfaceDescriptor());
    if (enable) {
      remote()->transact(ENABLE_LOG_TO_LOGCAT, data, &reply);
    } else {
      remote()->transact(DISABLE_LOG_TO_LOGCAT, data, &reply);
    }
    status_t ret = reply.readInt32();
    return ret;
  }

  virtual sp<IDiagnostic> getDiagnostic() {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    status_t ret = remote()->transact(TRANSACT_GET_DIAGNOSTIC, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return interface_cast<IDiagnostic>(reply.readStrongBinder());
  }

  virtual sp<IControls> getControls() {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    status_t ret = remote()->transact(TRANSACT_GET_CONTROLS, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return interface_cast<IControls>(reply.readStrongBinder());
  }
};

IMPLEMENT_META_INTERFACE(Service, "ia.hwc.IService");

status_t BnService::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
                               uint32_t flags) {
  switch (code) {
    case BpService::GET_HWC_VERSION: {
      CHECK_INTERFACE(IService, data, reply);
      reply->writeString8(GetHwcVersion());
      return NO_ERROR;
    }

    case BpService::SET_OPTION: {
      CHECK_INTERFACE(IService, data, reply);
      String16 option = data.readString16();
      String16 optionValue = data.readString16();
      status_t ret = setOption(String8(option), String8(optionValue));
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    case BpService::DUMP_OPTIONS: {
      CHECK_INTERFACE(IService, data, reply);
      dumpOptions();
      return NO_ERROR;
    }

    case BpService::DISABLE_LOG_TO_LOGCAT: {
      CHECK_INTERFACE(IService, data, reply);
      status_t ret = enableLogviewToLogcat(false);
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    case BpService::ENABLE_LOG_TO_LOGCAT: {
      CHECK_INTERFACE(IService, data, reply);
      status_t ret = enableLogviewToLogcat();
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    case BpService::TRANSACT_GET_DIAGNOSTIC: {
      CHECK_INTERFACE(IService, data, reply);
      sp<IBinder> b = IInterface::asBinder(this->getDiagnostic());
      reply->writeStrongBinder(b);
      return NO_ERROR;
    }

    case BpService::TRANSACT_GET_CONTROLS: {
      CHECK_INTERFACE(IService, data, reply);
      sp<IBinder> b = IInterface::asBinder(this->getControls());
      reply->writeStrongBinder(b);
      return NO_ERROR;
    }

    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}

}  // namespace hwcomposer
