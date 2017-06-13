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

#include "icontrols.h"
#include <utils/String8.h>
#include <binder/IPCThreadState.h>

// For AID_ROOT & AID_MEDIA - various vendor code and utils include this despite
// the path.
#include <private/android_filesystem_config.h>

namespace hwcomposer {

using namespace android;

/**
 */
class BpControls : public BpInterface<IControls> {
 public:
  BpControls(const sp<IBinder> &impl) : BpInterface<IControls>(impl) {
  }

  enum {
    // ==============================================
    // Public APIs - try not to reorder these

    TRANSACT_DISPLAY_SET_OVERSCAN = IBinder::FIRST_CALL_TRANSACTION,
    TRANSACT_DISPLAY_GET_OVERSCAN,
    TRANSACT_DISPLAY_SET_SCALING,
    TRANSACT_DISPLAY_GET_SCALING,
    TRANSACT_DISPLAY_ENABLE_BLANK,
    TRANSACT_DISPLAY_RESTORE_DEFAULT_COLOR_PARAM,
    TRANSACT_DISPLAY_GET_COLOR_PARAM,
    TRANSACT_DISPLAY_SET_COLOR_PARAM,
    TRANSACT_DISPLAYMODE_GET_AVAILABLE_MODES,
    TRANSACT_DISPLAYMODE_GET_MODE,
    TRANSACT_DISPLAYMODE_SET_MODE,
    TRANSACT_VIDEO_ENABLE_ENCRYPTED_SESSION,
    TRANSACT_VIDEO_DISABLE_ENCRYPTED_SESSION,
    TRANSACT_VIDEO_DISABLE_ALL_ENCRYPTED_SESSIONS,
    TRANSACT_VIDEO_IS_ENCRYPTED_SESSION_ENABLED,
    TRANSACT_VIDEO_SET_OPTIMIZATION_MODE,
    TRANSACT_MDS_UPDATE_VIDEO_STATE,
    TRANSACT_MDS_UPDATE_VIDEO_FPS,
    TRANSACT_MDS_UPDATE_INPUT_STATE,
    TRANSACT_WIDI_GET_SINGLE_DISPLAY,
    TRANSACT_WIDI_SET_SINGLE_DISPLAY,
  };

  virtual status_t displaySetOverscan(uint32_t display, int32_t xoverscan,
                                      int32_t yoverscan) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(xoverscan);
    data.writeInt32(yoverscan);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_SET_OVERSCAN, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t displayGetOverscan(uint32_t display, int32_t *xoverscan,
                                      int32_t *yoverscan) {
    if (!xoverscan || !yoverscan) {
      return android::BAD_VALUE;
    }
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_GET_OVERSCAN, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    status_t res = reply.readInt32();
    if (res != OK)
      return res;
    *xoverscan = reply.readInt32();
    *yoverscan = reply.readInt32();
    return OK;
  }

  virtual status_t displaySetScaling(uint32_t display,
                                     EHwcsScalingMode eScalingMode) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32((int32_t)eScalingMode);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_SET_SCALING, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t displayGetScaling(uint32_t display,
                                     EHwcsScalingMode *eScalingMode) {
    if (!eScalingMode) {
      return android::BAD_VALUE;
    }
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_GET_SCALING, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    status_t res = reply.readInt32();
    if (res != OK)
      return res;
    *eScalingMode = (EHwcsScalingMode)reply.readInt32();
    return OK;
  }

  virtual status_t displayEnableBlank(uint32_t display, bool blank) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32((int32_t)blank);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_ENABLE_BLANK, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t displayRestoreDefaultColorParam(uint32_t display,
                                                   EHwcsColorControl color) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(color);
    status_t ret = remote()->transact(
        TRANSACT_DISPLAY_RESTORE_DEFAULT_COLOR_PARAM, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t displayGetColorParam(uint32_t display,
                                        EHwcsColorControl color, float *value,
                                        float *startvalue, float *endvalue) {
    if (!value || !startvalue || !endvalue) {
      return android::BAD_VALUE;
    }
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(color);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_GET_COLOR_PARAM, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    *value = reply.readInt32();
    *startvalue = reply.readInt32();
    *endvalue = reply.readInt32();
    return reply.readInt32();
  }

  virtual status_t displaySetColorParam(uint32_t display,
                                        EHwcsColorControl color, float value) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(color);
    data.writeInt32(value);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_SET_COLOR_PARAM, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual Vector<HwcsDisplayModeInfo> displayModeGetAvailableModes(
      uint32_t display) {
    Vector<HwcsDisplayModeInfo> vector;
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    status_t ret = remote()->transact(TRANSACT_DISPLAYMODE_GET_AVAILABLE_MODES,
                                      data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return vector;
    }
    int32_t n = reply.readInt32();
    vector.setCapacity(n);
    while (n--) {
      HwcsDisplayModeInfo info;
      info.width = reply.readInt32();
      info.height = reply.readInt32();
      info.refresh = reply.readInt32();
      info.flags = reply.readInt32();
      info.ratio = reply.readInt32();
      vector.add(info);
    }
    return vector;
  }

  virtual status_t displayModeGetMode(uint32_t display,
                                      HwcsDisplayModeInfo *pMode) {
    if (!pMode) {
      return android::BAD_VALUE;
    }
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAYMODE_GET_MODE, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    pMode->width = reply.readInt32();
    pMode->height = reply.readInt32();
    pMode->refresh = reply.readInt32();
    pMode->flags = reply.readInt32();
    pMode->ratio = reply.readInt32();
    return reply.readInt32();
  }

  virtual status_t displayModeSetMode(uint32_t display,
                                      const HwcsDisplayModeInfo *pMode) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(pMode->width);
    data.writeInt32(pMode->height);
    data.writeInt32(pMode->refresh);
    data.writeInt32(pMode->flags);
    data.writeInt32(pMode->ratio);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAYMODE_SET_MODE, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t videoEnableEncryptedSession(uint32_t sessionID,
                                               uint32_t instanceID) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(sessionID);
    data.writeInt32(instanceID);
    status_t ret = remote()->transact(TRANSACT_VIDEO_ENABLE_ENCRYPTED_SESSION,
                                      data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t videoDisableEncryptedSession(uint32_t sessionID) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(sessionID);
    status_t ret = remote()->transact(TRANSACT_VIDEO_DISABLE_ENCRYPTED_SESSION,
                                      data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  virtual status_t videoDisableAllEncryptedSessions() {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_DISABLE_ALL_ENCRYPTED_SESSIONS, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  virtual bool videoIsEncryptedSessionEnabled(uint32_t sessionID,
                                              uint32_t instanceID) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(sessionID);
    data.writeInt32(instanceID);
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_IS_ENCRYPTED_SESSION_ENABLED, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return false;
    }
    return reply.readInt32();
  }

  virtual status_t videoSetOptimizationMode(EHwcsOptimizationMode mode) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(mode);
    status_t ret =
        remote()->transact(TRANSACT_VIDEO_SET_OPTIMIZATION_MODE, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t mdsUpdateVideoState(int64_t videoSessionID,
                                       bool isPrepared) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt64(videoSessionID);
    data.writeInt32(isPrepared);
    status_t ret =
        remote()->transact(TRANSACT_MDS_UPDATE_VIDEO_STATE, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  virtual status_t mdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt64(videoSessionID);
    data.writeInt32(fps);
    status_t ret =
        remote()->transact(TRANSACT_MDS_UPDATE_VIDEO_FPS, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  virtual status_t mdsUpdateInputState(bool state) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(state);
    status_t ret =
        remote()->transact(TRANSACT_MDS_UPDATE_INPUT_STATE, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  virtual status_t widiGetSingleDisplay(bool *pEnabled) {
    if (!pEnabled) {
      return android::BAD_VALUE;
    }
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    status_t ret =
        remote()->transact(TRANSACT_WIDI_GET_SINGLE_DISPLAY, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    *pEnabled = reply.readInt32();
    return reply.readInt32();
  }

  virtual status_t widiSetSingleDisplay(bool enable) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(enable);
    status_t ret =
        remote()->transact(TRANSACT_WIDI_SET_SINGLE_DISPLAY, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }
};

IMPLEMENT_META_INTERFACE(Controls, "intel.ufo.hwc.controls");

status_t BnControls::onTransact(uint32_t code, const Parcel &data,
                                Parcel *reply, uint32_t flags) {
  switch (code) {
    case BpControls::TRANSACT_DISPLAY_SET_OVERSCAN: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      int32_t xoverscan = data.readInt32();
      int32_t yoverscan = data.readInt32();
      status_t ret = this->displaySetOverscan(display, xoverscan, yoverscan);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_GET_OVERSCAN: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      int32_t xoverscan;
      int32_t yoverscan;
      status_t ret = this->displayGetOverscan(display, &xoverscan, &yoverscan);
      reply->writeInt32(ret);
      reply->writeInt32(xoverscan);
      reply->writeInt32(yoverscan);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_SET_SCALING: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsScalingMode scaling = (EHwcsScalingMode)data.readInt32();
      status_t ret = this->displaySetScaling(display, scaling);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_GET_SCALING: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsScalingMode scaling;
      status_t ret = this->displayGetScaling(display, &scaling);
      reply->writeInt32(ret);
      reply->writeInt32((int32_t)scaling);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_ENABLE_BLANK: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      bool blank = (bool)data.readInt32();
      status_t ret = this->displayEnableBlank(display, blank);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_RESTORE_DEFAULT_COLOR_PARAM: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsColorControl color = (EHwcsColorControl)data.readInt32();
      status_t ret = this->displayRestoreDefaultColorParam(display, color);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_GET_COLOR_PARAM: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsColorControl color = (EHwcsColorControl)data.readInt32();
      float value;
      float startvalue;
      float endvalue;
      status_t ret = this->displayGetColorParam(display, color, &value,
                                                &startvalue, &endvalue);
      reply->writeFloat(value);
      reply->writeFloat(startvalue);
      reply->writeFloat(endvalue);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_SET_COLOR_PARAM: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsColorControl color = (EHwcsColorControl)data.readInt32();
      float value = data.readFloat();
      status_t ret = this->displaySetColorParam(display, color, value);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAYMODE_GET_AVAILABLE_MODES: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();

      Vector<HwcsDisplayModeInfo> vector =
          this->displayModeGetAvailableModes(display);
      reply->writeInt32(vector.size());
      for (uint32_t i = 0; i < vector.size(); i++) {
        reply->writeInt32(vector[i].width);
        reply->writeInt32(vector[i].height);
        reply->writeInt32(vector[i].refresh);
        reply->writeInt32(vector[i].flags);
        reply->writeInt32(vector[i].ratio);
      }
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAYMODE_GET_MODE: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      HwcsDisplayModeInfo info;
      status_t ret = this->displayModeGetMode(display, &info);
      reply->writeInt32(info.width);
      reply->writeInt32(info.height);
      reply->writeInt32(info.refresh);
      reply->writeInt32(info.flags);
      reply->writeInt32(info.ratio);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAYMODE_SET_MODE: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      HwcsDisplayModeInfo info;
      info.width = data.readInt32();
      info.height = data.readInt32();
      info.refresh = data.readInt32();
      info.flags = data.readInt32();
      info.ratio = data.readInt32();
      status_t ret = this->displayModeSetMode(display, &info);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_ENABLE_ENCRYPTED_SESSION: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t sessionID = data.readInt32();
      uint32_t instanceID = data.readInt32();
      status_t ret = this->videoEnableEncryptedSession(sessionID, instanceID);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_DISABLE_ENCRYPTED_SESSION: {
      CHECK_INTERFACE(IControls, data, reply);
      int32_t sessionID = data.readInt32();
      status_t ret = this->videoDisableEncryptedSession(sessionID);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_DISABLE_ALL_ENCRYPTED_SESSIONS: {
      CHECK_INTERFACE(IControls, data, reply);
      status_t ret = this->videoDisableAllEncryptedSessions();
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_IS_ENCRYPTED_SESSION_ENABLED: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t sessionID = data.readInt32();
      uint32_t instanceID = data.readInt32();
      bool bEnabled =
          this->videoIsEncryptedSessionEnabled(sessionID, instanceID);
      reply->writeInt32(bEnabled);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_SET_OPTIMIZATION_MODE: {
      CHECK_INTERFACE(IControls, data, reply);
      EHwcsOptimizationMode mode = (EHwcsOptimizationMode)data.readInt32();
      status_t ret = this->videoSetOptimizationMode(mode);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_MDS_UPDATE_VIDEO_STATE: {
      CHECK_INTERFACE(IControls, data, reply);
      int64_t videoSessionID = data.readInt64();
      bool isPrepared = data.readInt32();
      status_t ret = this->mdsUpdateVideoState(videoSessionID, isPrepared);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_MDS_UPDATE_VIDEO_FPS: {
      CHECK_INTERFACE(IControls, data, reply);
      int64_t videoSessionID = data.readInt64();
      int32_t fps = data.readInt32();
      status_t ret = this->mdsUpdateVideoFPS(videoSessionID, fps);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_MDS_UPDATE_INPUT_STATE: {
      CHECK_INTERFACE(IControls, data, reply);
      bool state = data.readInt32();
      status_t ret = this->mdsUpdateInputState(state);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_WIDI_GET_SINGLE_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      bool enable = false;
      status_t ret = this->widiGetSingleDisplay(&enable);
      reply->writeInt32(enable);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_WIDI_SET_SINGLE_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      bool enable = data.readInt32();
      status_t ret = this->widiSetSingleDisplay(enable);
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}

}  // namespace hwcomposer
