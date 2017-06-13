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

#include "hwcserviceapi.h"

#include "icontrols.h"
#include "iservice.h"

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <utils/RefBase.h>
#include <utils/String8.h>

using namespace std;
using namespace android;

using namespace hwcomposer;

extern "C" {
struct HwcsContext {
  sp<IService> mHwcService;
  sp<IControls> mControls;
};

HWCSHANDLE HwcService_Connect() {
  ProcessState::self()
      ->startThreadPool();  // Required for starting binder threads

  HwcsContext context;
  context.mHwcService = interface_cast<IService>(
      defaultServiceManager()->getService(String16(IA_HWC_SERVICE_NAME)));
  if (context.mHwcService == NULL) {
    return NULL;
  }

  context.mControls = context.mHwcService->getControls();
  if (context.mControls == NULL) {
    return NULL;
  }

  return new HwcsContext(context);
}

void HwcService_Disconnect(HWCSHANDLE hwcs) {
  if (hwcs != NULL) {
    delete static_cast<HwcsContext*>(hwcs);
  }
}

const char* HwcService_GetHwcVersion(HWCSHANDLE hwcs) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return NULL;
  }

  static String8 version = pContext->mHwcService->getHwcVersion();
  if (version.length() == 0) {
    return NULL;
  }
  return version;
}

status_t HwcService_Display_SetOverscan(HWCSHANDLE hwcs, uint32_t display,
                                        int32_t xoverscan, int32_t yoverscan) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displaySetOverscan(display, xoverscan, yoverscan);
}

status_t HwcService_Display_GetOverscan(HWCSHANDLE hwcs, uint32_t display,
                                        int32_t* xoverscan,
                                        int32_t* yoverscan) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayGetOverscan(display, xoverscan, yoverscan);
}

status_t HwcService_Display_SetScaling(HWCSHANDLE hwcs, uint32_t display,
                                       EHwcsScalingMode eScalingMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displaySetScaling(display, eScalingMode);
}

status_t HwcService_Display_GetScaling(HWCSHANDLE hwcs, uint32_t display,
                                       EHwcsScalingMode* eScalingMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayGetScaling(display, eScalingMode);
}

status_t HwcService_Display_EnableBlank(HWCSHANDLE hwcs, uint32_t display,
                                        EHwcsBool blank) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayEnableBlank(display, blank);
}

status_t HwcService_Display_RestoreDefaultColorParam(HWCSHANDLE hwcs,
                                                     uint32_t display,
                                                     EHwcsColorControl color) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayRestoreDefaultColorParam(display, color);
}

status_t HwcService_Display_GetColorParam(HWCSHANDLE hwcs, uint32_t display,
                                          EHwcsColorControl color, float* value,
                                          float* startvalue, float* endvalue) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayGetColorParam(display, color, value,
                                                   startvalue, endvalue);
}

status_t HwcService_Display_SetColorParam(HWCSHANDLE hwcs, uint32_t display,
                                          EHwcsColorControl color,
                                          float value) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displaySetColorParam(display, color, value);
}

status_t HwcService_DisplayMode_GetAvailableModes(
    HWCSHANDLE hwcs, uint32_t display, unsigned modeCount,
    HwcsDisplayModeInfo* pModeList) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  Vector<HwcsDisplayModeInfo> modes =
      pContext->mControls->displayModeGetAvailableModes(display);
  if (pModeList && (modeCount > 0)) {
    size_t count = modes.size();
    if (count > modeCount)
      count = modeCount;
    memcpy(pModeList, modes.array(), sizeof(HwcsDisplayModeInfo) * count);
  }
  return modes.size();
}

status_t HwcService_DisplayMode_GetMode(HWCSHANDLE hwcs, uint32_t display,
                                        HwcsDisplayModeInfo* pMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayModeGetMode(display, pMode);
}

status_t HwcService_DisplayMode_SetMode(HWCSHANDLE hwcs, uint32_t display,
                                        const HwcsDisplayModeInfo* pMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->displayModeSetMode(display, pMode);
}

status_t HwcService_Video_EnableEncryptedSession(HWCSHANDLE hwcs,
                                                 uint32_t sessionID,
                                                 uint32_t instanceID) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->videoEnableEncryptedSession(sessionID,
                                                          instanceID);
}

status_t HwcService_Video_DisableEncryptedSession(HWCSHANDLE hwcs,
                                                  uint32_t sessionID) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->videoDisableEncryptedSession(sessionID);
}

status_t HwcService_Video_DisableAllEncryptedSessions(HWCSHANDLE hwcs) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->videoDisableAllEncryptedSessions();
}

EHwcsBool HwcService_Video_IsEncryptedSessionEnabled(HWCSHANDLE hwcs,
                                                     uint32_t sessionID,
                                                     uint32_t instanceID) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return HWCS_FALSE;
  }
  return pContext->mControls->videoIsEncryptedSessionEnabled(sessionID,
                                                             instanceID)
             ? HWCS_TRUE
             : HWCS_FALSE;
}

status_t HwcService_Video_SetOptimizationMode(HWCSHANDLE hwcs,
                                              EHwcsOptimizationMode mode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->videoSetOptimizationMode(mode);
}

status_t HwcService_MDS_UpdateVideoState(HWCSHANDLE hwcs,
                                         int64_t videoSessionID,
                                         EHwcsBool isPrepared) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->mdsUpdateVideoState(videoSessionID, isPrepared);
}

status_t HwcService_MDS_UpdateVideoFPS(HWCSHANDLE hwcs, int64_t videoSessionID,
                                       int32_t fps) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->mdsUpdateVideoFPS(videoSessionID, fps);
}

status_t HwcService_MDS_UpdateInputState(HWCSHANDLE hwcs, EHwcsBool state) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->mdsUpdateInputState(state);
}

status_t HwcService_Widi_GetSingleDisplay(HWCSHANDLE hwcs, EHwcsBool* enable) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  if (!enable) {
    return android::BAD_VALUE;
  }
  bool bEnabled = false;
  status_t ret = pContext->mControls->widiGetSingleDisplay(&bEnabled);
  *enable = bEnabled ? HWCS_TRUE : HWCS_FALSE;
  return ret;
}

status_t HwcService_Widi_SetSingleDisplay(HWCSHANDLE hwcs, EHwcsBool enable) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return -1;
  }
  return pContext->mControls->widiSetSingleDisplay(enable);
}
}
