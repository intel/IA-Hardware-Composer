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

  context.mControls = context.mHwcService->GetControls();
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

  static String8 version = pContext->mHwcService->GetHwcVersion();
  if (version.length() == 0) {
    return NULL;
  }
  return version;
}

status_t HwcService_Display_SetOverscan(HWCSHANDLE hwcs, uint32_t display,
                                        int32_t xoverscan, int32_t yoverscan) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplaySetOverscan(display, xoverscan, yoverscan);
}

status_t HwcService_Display_GetOverscan(HWCSHANDLE hwcs, uint32_t display,
                                        int32_t* xoverscan,
                                        int32_t* yoverscan) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayGetOverscan(display, xoverscan, yoverscan);
}

status_t HwcService_Display_SetScaling(HWCSHANDLE hwcs, uint32_t display,
                                       EHwcsScalingMode eScalingMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplaySetScaling(display, eScalingMode);
}

status_t HwcService_Display_GetScaling(HWCSHANDLE hwcs, uint32_t display,
                                       EHwcsScalingMode* eScalingMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayGetScaling(display, eScalingMode);
}

status_t HwcService_Display_EnableBlank(HWCSHANDLE hwcs, uint32_t display,
                                        EHwcsBool blank) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayEnableBlank(display, blank);
}

status_t HwcService_Display_RestoreDefaultColorParam(HWCSHANDLE hwcs,
                                                     uint32_t display,
                                                     EHwcsColorControl color) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayRestoreDefaultColorParam(display, color);
}

status_t HwcService_Display_RestoreDefaultDeinterlaceParam(HWCSHANDLE hwcs,
                                                           uint32_t display) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayRestoreDefaultDeinterlaceParam(display);
}

status_t HwcService_Display_GetColorParam(HWCSHANDLE hwcs, uint32_t display,
                                          EHwcsColorControl color, float* value,
                                          float* startvalue, float* endvalue) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayGetColorParam(display, color, value,
                                                   startvalue, endvalue);
}

status_t HwcService_Display_SetColorParam(HWCSHANDLE hwcs, uint32_t display,
                                          EHwcsColorControl color,
                                          float value) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplaySetColorParam(display, color, value);
}

status_t HwcService_Display_SetDeinterlaceParam(HWCSHANDLE hwcs,
                                                uint32_t display,
                                                uint32_t mode) {
  EHwcsDeinterlaceControl de_mode;

  switch (mode) {
    case 0:
      de_mode = HWCS_DEINTERLACE_NONE;
      break;
    case 1:
      de_mode = HWCS_DEINTERLACE_BOB;
      break;
    case 2:
      de_mode = HWCS_DEINTERLACE_WEAVE;
      break;
    case 3:
      de_mode = HWCS_DEINTERLACE_MOTIONADAPTIVE;
      break;
    case 4:
      de_mode = HWCS_DEINTERLACE_MOTIONCOMPENSATED;
      break;
    default:
      de_mode = HWCS_DEINTERLACE_NONE;
      break;
  }
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplaySetDeinterlaceParam(display, de_mode);
}

status_t HwcService_DisplayMode_GetAvailableModes(
    HWCSHANDLE hwcs, uint32_t display,
    std::vector<HwcsDisplayModeInfo>& pModeList) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  pModeList = pContext->mControls->DisplayModeGetAvailableModes(display);
  return OK;
}

status_t HwcService_DisplayMode_GetMode(HWCSHANDLE hwcs, uint32_t display,
                                        HwcsDisplayModeInfo* pMode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayModeGetMode(display, pMode);
}

status_t HwcService_DisplayMode_SetMode(HWCSHANDLE hwcs, uint32_t display,
                                        const uint32_t config) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->DisplayModeSetMode(display, config);
}

status_t HwcService_Video_EnableHDCPSession_ForDisplay(
    HWCSHANDLE hwcs, uint32_t connector, EHwcsContentType content_type) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }

  return pContext->mControls->EnableHDCPSessionForDisplay(connector,
                                                          content_type);
}

status_t HwcService_Video_EnableHDCPSession_AllDisplays(
    HWCSHANDLE hwcs, EHwcsContentType content_type) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }

  return pContext->mControls->EnableHDCPSessionForAllDisplays(content_type);
}

status_t HwcService_Video_SetHDCPSRM_AllDisplays(HWCSHANDLE hwcs,
                                                 const int8_t* SRM,
                                                 uint32_t SRMLength) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }

  return pContext->mControls->SetHDCPSRMForAllDisplays(SRM, SRMLength);
}

status_t HwcService_Video_SetHDCPSRM_ForDisplay(HWCSHANDLE hwcs,
                                                uint32_t connector,
                                                const int8_t* SRM,
                                                uint32_t SRMLength) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);

  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->SetHDCPSRMForDisplay(connector, SRM, SRMLength);
}

status_t HwcService_Video_DisableHDCPSession_ForDisplay(HWCSHANDLE hwcs,
                                                        uint32_t connector) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }

  return pContext->mControls->DisableHDCPSessionForDisplay(connector);
}

uint32_t HwcService_GetDisplayIDFromConnectorID(HWCSHANDLE hwcs,
                                                uint32_t connector_id) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }

  return pContext->mControls->GetDisplayIDFromConnectorID(connector_id);
}

status_t HwcService_Video_DisableHDCPSession_AllDisplays(HWCSHANDLE hwcs) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }

  return pContext->mControls->DisableHDCPSessionForAllDisplays();
}

status_t HwcService_Video_EnableEncryptedSession(HWCSHANDLE hwcs,
                                                 uint32_t sessionID,
                                                 uint32_t instanceID) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->VideoEnableEncryptedSession(sessionID,
                                                          instanceID);
}

status_t HwcService_Video_DisableEncryptedSession(HWCSHANDLE hwcs,
                                                  uint32_t sessionID) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->VideoDisableAllEncryptedSessions(sessionID);
}

status_t HwcService_Video_DisableAllEncryptedSessions(HWCSHANDLE hwcs) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->VideoDisableAllEncryptedSessions();
}

EHwcsBool HwcService_Video_IsEncryptedSessionEnabled(HWCSHANDLE hwcs,
                                                     uint32_t sessionID,
                                                     uint32_t instanceID) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return HWCS_FALSE;
  }
  return pContext->mControls->VideoIsEncryptedSessionEnabled(sessionID,
                                                             instanceID)
             ? HWCS_TRUE
             : HWCS_FALSE;
}

status_t HwcService_Video_SetOptimizationMode(HWCSHANDLE hwcs,
                                              EHwcsOptimizationMode mode) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->VideoSetOptimizationMode(mode);
}

status_t HwcService_MDS_UpdateVideoState(HWCSHANDLE hwcs,
                                         int64_t videoSessionID,
                                         EHwcsBool isPrepared) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->MdsUpdateVideoState(videoSessionID, isPrepared);
}

status_t HwcService_MDS_UpdateVideoFPS(HWCSHANDLE hwcs, int64_t videoSessionID,
                                       int32_t fps) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->MdsUpdateVideoFPS(videoSessionID, fps);
}

status_t HwcService_MDS_UpdateInputState(HWCSHANDLE hwcs, EHwcsBool state) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->MdsUpdateInputState(state);
}

status_t HwcService_Widi_GetSingleDisplay(HWCSHANDLE hwcs, EHwcsBool* enable) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  if (!enable) {
    return android::BAD_VALUE;
  }
  bool bEnabled = false;
  status_t ret = pContext->mControls->WidiGetSingleDisplay(&bEnabled);
  *enable = bEnabled ? HWCS_TRUE : HWCS_FALSE;
  return ret;
}

status_t HwcService_Widi_SetSingleDisplay(HWCSHANDLE hwcs, EHwcsBool enable) {
  HwcsContext* pContext = static_cast<HwcsContext*>(hwcs);
  if (!pContext) {
    return android::BAD_VALUE;
  }
  return pContext->mControls->WidiSetSingleDisplay(enable);
}
}
