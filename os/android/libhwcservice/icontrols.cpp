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
#include <binder/IPCThreadState.h>
#include <utils/String8.h>

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
    TRANSACT_DISPLAY_SET_DEINTERLACE_PARAM,
    TRANSACT_DISPLAY_RESTORE_DEFAULT_DEINTERLACE_PARAM,
    TRANSACT_DISPLAYMODE_GET_AVAILABLE_MODES,
    TRANSACT_DISPLAYMODE_GET_MODE,
    TRANSACT_DISPLAYMODE_SET_MODE,
    TRANSACT_VIDEO_ENABLE_HDCP_SESSION_FOR_DISPLAY,
    TRANSACT_VIDEO_ENABLE_HDCP_SESSION_FOR_ALL_DISPLAYS,
    TRANSACT_VIDEO_DISABLE_HDCP_SESSION_FOR_DISPLAY,
    TRANSACT_VIDEO_DISABLE_HDCP_SESSION_FOR_ALL_DISPLAYS,
    TRANSACT_VIDEO_SET_HDCP_SRM_FOR_ALL_DISPLAYS,
    TRANSACT_VIDEO_SET_HDCP_SRM_FOR_DISPLAY,
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

  status_t DisplaySetOverscan(uint32_t display, int32_t xoverscan,
                              int32_t yoverscan) override {
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

  status_t DisplayGetOverscan(uint32_t display, int32_t *xoverscan,
                              int32_t *yoverscan) override {
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

  status_t DisplaySetScaling(uint32_t display,
                             EHwcsScalingMode eScalingMode) override {
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

  status_t DisplayGetScaling(uint32_t display,
                             EHwcsScalingMode *eScalingMode) override {
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

  status_t DisplayEnableBlank(uint32_t display, bool blank) override {
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

  status_t DisplayRestoreDefaultColorParam(uint32_t display,
                                           EHwcsColorControl color) override {
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

  status_t DisplayRestoreDefaultDeinterlaceParam(uint32_t display) override {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    status_t ret = remote()->transact(
        TRANSACT_DISPLAY_RESTORE_DEFAULT_DEINTERLACE_PARAM, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  status_t DisplayGetColorParam(uint32_t display, EHwcsColorControl color,
                                float *value, float *startvalue,
                                float *endvalue) override {
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
    *value = reply.readFloat();
    *startvalue = reply.readFloat();
    *endvalue = reply.readFloat();
    return reply.readInt32();
  }

  status_t DisplaySetColorParam(uint32_t display, EHwcsColorControl color,
                                float value) override {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(color);
    data.writeFloat(value);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAY_SET_COLOR_PARAM, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  status_t DisplaySetDeinterlaceParam(uint32_t display,
                                      EHwcsDeinterlaceControl mode) override {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(mode);
    status_t ret = remote()->transact(TRANSACT_DISPLAY_SET_DEINTERLACE_PARAM,
                                      data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  std::vector<HwcsDisplayModeInfo> DisplayModeGetAvailableModes(
      uint32_t display) override {
    std::vector<HwcsDisplayModeInfo> vector;
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
    while (n--) {
      HwcsDisplayModeInfo info;
      info.width = reply.readInt32();
      info.height = reply.readInt32();
      info.refresh = reply.readInt32();
      info.xdpi = reply.readInt32();
      info.ydpi = reply.readInt32();
      vector.push_back(info);
    }
    return vector;
  }

  status_t DisplayModeGetMode(uint32_t display,
                              HwcsDisplayModeInfo *pMode) override {
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
    pMode->xdpi = reply.readInt32();
    pMode->ydpi = reply.readInt32();
    return reply.readInt32();
  }

  status_t DisplayModeSetMode(uint32_t display,
                              const uint32_t config) override {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(config);
    status_t ret =
        remote()->transact(TRANSACT_DISPLAYMODE_SET_MODE, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  status_t EnableHDCPSessionForDisplay(uint32_t display,
                                       EHwcsContentType content_type) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeInt32(content_type);
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_ENABLE_HDCP_SESSION_FOR_DISPLAY, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  status_t EnableHDCPSessionForAllDisplays(EHwcsContentType content_type) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(content_type);
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_ENABLE_HDCP_SESSION_FOR_ALL_DISPLAYS, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  status_t DisableHDCPSessionForDisplay(uint32_t display) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_DISABLE_HDCP_SESSION_FOR_DISPLAY, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
      return ret;
    }
    return reply.readInt32();
  }

  status_t DisableHDCPSessionForAllDisplays() {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_DISABLE_HDCP_SESSION_FOR_ALL_DISPLAYS, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  status_t SetHDCPSRMForAllDisplays(const int8_t *SRM, uint32_t SRMLength) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeByteArray(SRMLength, (const uint8_t *)SRM);
    status_t ret = remote()->transact(
        TRANSACT_VIDEO_SET_HDCP_SRM_FOR_ALL_DISPLAYS, data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  status_t SetHDCPSRMForDisplay(uint32_t display, const int8_t *SRM,
                                uint32_t SRMLength) {
    Parcel data;
    Parcel reply;
    data.writeInterfaceToken(IControls::getInterfaceDescriptor());
    data.writeInt32(display);
    data.writeByteArray(SRMLength, (const uint8_t *)SRM);
    status_t ret = remote()->transact(TRANSACT_VIDEO_SET_HDCP_SRM_FOR_DISPLAY,
                                      data, &reply);
    if (ret != NO_ERROR) {
      ALOGW("%s() transact failed: %d", __FUNCTION__, ret);
    }
    return reply.readInt32();
  }

  status_t VideoEnableEncryptedSession(uint32_t sessionID,
                                       uint32_t instanceID) override {
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

  status_t VideoDisableAllEncryptedSessions(uint32_t sessionID) override {
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

  status_t VideoDisableAllEncryptedSessions() override {
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

  bool VideoIsEncryptedSessionEnabled(uint32_t sessionID,
                                      uint32_t instanceID) override {
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

  status_t VideoSetOptimizationMode(EHwcsOptimizationMode mode) override {
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

  status_t MdsUpdateVideoState(int64_t videoSessionID,
                               bool isPrepared) override {
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

  status_t MdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps) override {
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

  status_t MdsUpdateInputState(bool state) override {
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

  status_t WidiGetSingleDisplay(bool *pEnabled) override {
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

  status_t WidiSetSingleDisplay(bool enable) override {
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

IMPLEMENT_META_INTERFACE(Controls, "iahwc.controls");

status_t BnControls::onTransact(uint32_t code, const Parcel &data,
                                Parcel *reply, uint32_t flags) {
  switch (code) {
    case BpControls::TRANSACT_DISPLAY_SET_OVERSCAN: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      int32_t xoverscan = data.readInt32();
      int32_t yoverscan = data.readInt32();
      status_t ret = this->DisplaySetOverscan(display, xoverscan, yoverscan);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_GET_OVERSCAN: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      int32_t xoverscan;
      int32_t yoverscan;
      status_t ret = this->DisplayGetOverscan(display, &xoverscan, &yoverscan);
      reply->writeInt32(ret);
      reply->writeInt32(xoverscan);
      reply->writeInt32(yoverscan);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_SET_SCALING: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsScalingMode scaling = (EHwcsScalingMode)data.readInt32();
      status_t ret = this->DisplaySetScaling(display, scaling);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_GET_SCALING: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsScalingMode scaling;
      status_t ret = this->DisplayGetScaling(display, &scaling);
      reply->writeInt32(ret);
      reply->writeInt32((int32_t)scaling);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_ENABLE_BLANK: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      bool blank = (bool)data.readInt32();
      status_t ret = this->DisplayEnableBlank(display, blank);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_RESTORE_DEFAULT_COLOR_PARAM: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsColorControl color = (EHwcsColorControl)data.readInt32();
      status_t ret = this->DisplayRestoreDefaultColorParam(display, color);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_RESTORE_DEFAULT_DEINTERLACE_PARAM: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      status_t ret = this->DisplayRestoreDefaultDeinterlaceParam(display);
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
      status_t ret = this->DisplayGetColorParam(display, color, &value,
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
      status_t ret = this->DisplaySetColorParam(display, color, value);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAY_SET_DEINTERLACE_PARAM: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsDeinterlaceControl mode = (EHwcsDeinterlaceControl)data.readInt32();
      status_t ret = this->DisplaySetDeinterlaceParam(display, mode);
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    case BpControls::TRANSACT_DISPLAYMODE_GET_AVAILABLE_MODES: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();

      std::vector<HwcsDisplayModeInfo> vector =
          this->DisplayModeGetAvailableModes(display);
      reply->writeInt32(vector.size());
      for (uint32_t i = 0; i < vector.size(); i++) {
        reply->writeInt32(vector[i].width);
        reply->writeInt32(vector[i].height);
        reply->writeInt32(vector[i].refresh);
        reply->writeInt32(vector[i].xdpi);
        reply->writeInt32(vector[i].ydpi);
      }
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAYMODE_GET_MODE: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      HwcsDisplayModeInfo info;
      status_t ret = this->DisplayModeGetMode(display, &info);
      reply->writeInt32(info.width);
      reply->writeInt32(info.height);
      reply->writeInt32(info.refresh);
      reply->writeInt32(info.xdpi);
      reply->writeInt32(info.ydpi);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_DISPLAYMODE_SET_MODE: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      uint32_t config = data.readInt32();
      status_t ret = this->DisplayModeSetMode(display, config);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_ENABLE_HDCP_SESSION_FOR_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      EHwcsContentType content_type = (EHwcsContentType)data.readInt32();
      status_t ret = this->EnableHDCPSessionForDisplay(display, content_type);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_ENABLE_HDCP_SESSION_FOR_ALL_DISPLAYS: {
      CHECK_INTERFACE(IControls, data, reply);
      EHwcsContentType content_type = (EHwcsContentType)data.readInt32();
      status_t ret = this->EnableHDCPSessionForAllDisplays(content_type);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_DISABLE_HDCP_SESSION_FOR_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t display = data.readInt32();
      status_t ret = this->DisableHDCPSessionForDisplay(display);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_DISABLE_HDCP_SESSION_FOR_ALL_DISPLAYS: {
      CHECK_INTERFACE(IControls, data, reply);
      status_t ret = this->DisableHDCPSessionForAllDisplays();
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_SET_HDCP_SRM_FOR_ALL_DISPLAYS: {
      CHECK_INTERFACE(IControls, data, reply);
      std::vector<int8_t> srmvec;
      data.readByteVector(&srmvec);
      status_t ret = this->SetHDCPSRMForAllDisplays(
          (const int8_t *)(srmvec.data()), srmvec.size());
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_SET_HDCP_SRM_FOR_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      std::vector<int8_t> srmvec;
      uint32_t display = data.readInt32();
      data.readByteVector(&srmvec);
      status_t ret = this->SetHDCPSRMForDisplay(
          display, (const int8_t *)(srmvec.data()), srmvec.size());
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    case BpControls::TRANSACT_VIDEO_ENABLE_ENCRYPTED_SESSION: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t sessionID = data.readInt32();
      uint32_t instanceID = data.readInt32();
      status_t ret = this->VideoEnableEncryptedSession(sessionID, instanceID);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_DISABLE_ENCRYPTED_SESSION: {
      CHECK_INTERFACE(IControls, data, reply);
      int32_t sessionID = data.readInt32();
      status_t ret = this->VideoDisableAllEncryptedSessions(sessionID);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_DISABLE_ALL_ENCRYPTED_SESSIONS: {
      CHECK_INTERFACE(IControls, data, reply);
      status_t ret = this->VideoDisableAllEncryptedSessions();
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_IS_ENCRYPTED_SESSION_ENABLED: {
      CHECK_INTERFACE(IControls, data, reply);
      uint32_t sessionID = data.readInt32();
      uint32_t instanceID = data.readInt32();
      bool bEnabled =
          this->VideoIsEncryptedSessionEnabled(sessionID, instanceID);
      reply->writeInt32(bEnabled);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_VIDEO_SET_OPTIMIZATION_MODE: {
      CHECK_INTERFACE(IControls, data, reply);
      EHwcsOptimizationMode mode = (EHwcsOptimizationMode)data.readInt32();
      status_t ret = this->VideoSetOptimizationMode(mode);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_MDS_UPDATE_VIDEO_STATE: {
      CHECK_INTERFACE(IControls, data, reply);
      int64_t videoSessionID = data.readInt64();
      bool isPrepared = data.readInt32();
      status_t ret = this->MdsUpdateVideoState(videoSessionID, isPrepared);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_MDS_UPDATE_VIDEO_FPS: {
      CHECK_INTERFACE(IControls, data, reply);
      int64_t videoSessionID = data.readInt64();
      int32_t fps = data.readInt32();
      status_t ret = this->MdsUpdateVideoFPS(videoSessionID, fps);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_MDS_UPDATE_INPUT_STATE: {
      CHECK_INTERFACE(IControls, data, reply);
      bool state = data.readInt32();
      status_t ret = this->MdsUpdateInputState(state);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_WIDI_GET_SINGLE_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      bool enable = false;
      status_t ret = this->WidiGetSingleDisplay(&enable);
      reply->writeInt32(enable);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    case BpControls::TRANSACT_WIDI_SET_SINGLE_DISPLAY: {
      CHECK_INTERFACE(IControls, data, reply);
      bool enable = data.readInt32();
      status_t ret = this->WidiSetSingleDisplay(enable);
      reply->writeInt32(ret);
      return NO_ERROR;
    }

    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}

}  // namespace hwcomposer
