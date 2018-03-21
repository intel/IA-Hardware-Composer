/*
// Copyright (c) 2018 Intel Corporation
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

#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <math.h>
#include "hwcdefs.h"
#include "DrmShimChecks.h"
#include "DrmShimCrtc.h"
#include "DrmShimBuffer.h"
#include "BufferObject.h"
#include "DrmShimWork.h"
#include "HwcTestState.h"
#include "HwcTestConfig.h"
#include "HwcTestDebug.h"
#include "HwcTestUtil.h"
#include "HwcvalPropertyManager.h"
#include "HwcvalThreadTable.h"

#include "drm_fourcc.h"
#ifdef HWCVAL_TARGET_HAS_MULTIPLE_DISPLAY
#include "MultiDisplayType.h"
#endif

#undef LOG_TAG
#define LOG_TAG "DRM_SHIM"

using namespace Hwcval;

/// Statistics declarations
static Hwcval::Statistics::Counter sHwPlaneTransformUsedCounter(
    "hw_plane_transforms_used");
static Hwcval::Statistics::Counter sHwPlaneScaleUsedCounter(
    "hw_plane_scalers_used");

/// Constructor
DrmShimChecks::DrmShimChecks()
    : HwcTestKernel(),
      mShimDrmFd(0),
      mPropMgr(0),
      mUniversalPlanes(false),
      mDrmFrameNo(0),
      mDrmParser(this,&mLogParser) {
  for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
    mCurrentFrame[i] = -1;
    mLastFrameWasDropped[i] = false;
  }
  mCrtcs.emplace(0,(DrmShimCrtc*)0);

}

/// Destructor
DrmShimChecks::~DrmShimChecks() {
  HWCLOGI("Destroying DrmShimChecks");

  {
    HWCVAL_LOCK(_l, mMutex);
    mWorkQueue.Process();
  }
}

void DrmShimChecks::CheckGetResourcesExit(int fd, drmModeResPtr res) {
  HWCVAL_UNUSED(fd);

  if (res) {
    ALOG_ASSERT(res->count_crtcs <= HWCVAL_MAX_CRTCS);

    for (int i = 0; i < res->count_crtcs; ++i) {
      DrmShimCrtc* crtc = CreatePipe(i, res->crtcs[i]);
      mCrtcs[crtc->GetCrtcId()] = crtc;
    }
  }
}

void DrmShimChecks::OverrideDefaultMode(drmModeConnectorPtr pConn) {
  uint32_t maxScore = 0;
  int realPreferredMode = -1;
  int newPreferredMode = -1;

  for (int i = 0; i < pConn->count_modes; ++i) {
    drmModeModeInfoPtr pMode = pConn->modes + i;
    uint32_t score = 0;

    if (pMode->hdisplay == mPrefHdmiWidth) {
      ++score;
    }

    if (pMode->vdisplay == mPrefHdmiHeight) {
      ++score;
    }

    if (pMode->vrefresh == mPrefHdmiRefresh) {
      ++score;
    }

    if (score > maxScore) {
      newPreferredMode = i;
      maxScore = score;
    }

    if (pMode->type & DRM_MODE_TYPE_PREFERRED) {
      realPreferredMode = i;
    }
  }

  if (maxScore == 0) {
    HWCLOGI("No mode matching preferred mode override found.");
  } else if (realPreferredMode != newPreferredMode) {
    // unset preferred mode on mode actually preferred by monitor
    pConn->modes[realPreferredMode].type &= ~DRM_MODE_TYPE_PREFERRED;

    // set preferred mode on the one we want
    drmModeModeInfoPtr prefMode = pConn->modes + newPreferredMode;
    prefMode->type |= DRM_MODE_TYPE_PREFERRED;

    if (maxScore == 3) {
      HWCLOGI("Exact match with preferred mode override:");
    } else {
      HWCLOGI("Closest match with preferred mode override:");
    }

    HWCLOGI("Mode %d %dx%d refresh=%d", newPreferredMode, prefMode->hdisplay,
            prefMode->vdisplay, prefMode->vrefresh);
  }
}

void DrmShimChecks::RandomizeModes(int& count, drmModeModeInfoPtr& modes) {
  bool modeUsed[count];
  memset(modeUsed, 0, sizeof(bool) * count);

  // generate a mode count between 1 and the number of real modes
  int newCount = (rand() % count) + 1;
  drmModeModeInfoPtr newModes =
      (drmModeModeInfoPtr)malloc(sizeof(drmModeModeInfo) * newCount);
  memset(newModes, 0, sizeof(drmModeModeInfo) * newCount);

  // Shuffle modes from the old into the new
  for (int i = 0; i < newCount; ++i) {
    // Choose one of the old modes that we haven't already used
    int n;
    do {
      n = rand() % count;
    } while (modeUsed[n]);

    newModes[i] = modes[n];
    newModes[i].type &= ~DRM_MODE_TYPE_PREFERRED;
    modeUsed[n] = true;
  }

  // Choose a preferred mode
  int prefModeIx = rand() % newCount;
  newModes[prefModeIx].type |= DRM_MODE_TYPE_PREFERRED;

  // Free the old mode list
  free(modes);

  // Return the shuffled modes to be passed back to HWC
  count = newCount;
  modes = newModes;
}

const char* DrmShimChecks::AspectStr(uint32_t aspect) {
  switch (aspect) {
    case DRM_MODE_PICTURE_ASPECT_4_3:
      return "4:3";
    case DRM_MODE_PICTURE_ASPECT_16_9:
      return "16:9";
    default:
      break;
  }

  return "UNKNOWN_ASPECT";
}

void DrmShimChecks::LogModes(uint32_t connId, const char* str,
                             drmModeConnectorPtr pConn) {
  HWCLOGI("%s: connId %d encoder_id %d:", str, connId, pConn->encoder_id);
  for (int i = 0; i < pConn->count_modes; ++i) {
    drmModeModeInfoPtr pMode = pConn->modes + i;
    HWCLOGI("  Mode %d: %s", i, pMode->name);
    HWCLOGI("  Clock %d vrefresh %d flags 0x%x aspect %s type %d %s",
            pMode->clock, pMode->vrefresh, pMode->flags,
	    AspectStr(pMode->flags), pMode->type,
            ((pMode->type & DRM_MODE_TYPE_PREFERRED) ? "PREFERRED " : ""));
    HWCLOGI("  H Size %d sync start %d end %d total %d skew %d",
            pMode->hdisplay, pMode->hsync_start, pMode->hsync_end,
            pMode->htotal, pMode->hskew);
    HWCLOGI("  V Size %d sync start %d end %d total %d scan %d",
            pMode->vdisplay, pMode->vsync_start, pMode->vsync_end,
            pMode->vtotal, pMode->vscan);
  }

  if (pConn->count_modes != 1) {
    HWCLOGW("Number of modes=%d.", pConn->count_modes);
  }
}

static bool IsConnectorTypeHotPluggable(uint32_t connType) {
  switch (connType) {
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
    case DRM_MODE_CONNECTOR_DisplayPort: {
      return true;
    }

    default: { return false; }
  }
}

void DrmShimChecks::CheckGetConnectorExit(int fd, uint32_t connId,
                                          drmModeConnectorPtr& pConn) {
  HWCVAL_UNUSED(fd);
  HWCVAL_UNUSED(connId);
  HWCVAL_LOCK(_l, mMutex);

  HwcTestCrtc::ModeVec modes;

  LogModes(connId, "Real modes", pConn);

  // Optionally, pretend the panel is HDMI
  bool connectorPhysicallyHotPluggable =
      IsConnectorTypeHotPluggable(pConn->connector_type);

  if (mState->IsOptionEnabled(eOptSpoofNoPanel) &&
      !connectorPhysicallyHotPluggable) {
    pConn->connector_type = DRM_MODE_CONNECTOR_HDMIA;
  }

  // Establish if the connector is software hot-pluggable (after allowing for
  // display type spoof).
  bool hotPluggable = IsConnectorTypeHotPluggable(pConn->connector_type);

  if (hotPluggable) {
    // Note this will replace the existing item if is already there
    OverrideDefaultMode(pConn);
    mHotPluggableConnectors.emplace(pConn->connector_id);
  } else {
    mHotPluggableConnectors.erase(pConn->connector_id);
  }

  if ((pConn->count_modes > 1) && mState->IsOptionEnabled(eOptRandomizeModes)) {
    RandomizeModes(pConn->count_modes, pConn->modes);
    LogModes(connId, "Shuffled modes", pConn);
  }

  uint32_t realRefresh = 0;

  // If we are spoofing DRRS and we are the panel, add the second mode for the
  // minimum refresh
  if ((pConn->count_modes == 1) && (pConn->modes[0].vrefresh > 48) &&
      mState->IsOptionEnabled(eOptSpoofDRRS)) {
    drmModeModeInfoPtr pMode = pConn->modes;
    HwcTestCrtc::Mode mode;
    mode.width = pMode->hdisplay;
    mode.height = pMode->vdisplay;
    mode.refresh = pMode->vrefresh;

    modes.push_back(mode);

    char* pMem = (char*)malloc(2 * sizeof(drmModeModeInfo));
    memset(pMem, 0, 2 * sizeof(drmModeModeInfo));

    pConn->modes = (drmModeModeInfoPtr)pMem;
    pConn->modes[0] = *pMode;
    pConn->modes[1] = *pMode;
    pConn->modes[1].vrefresh = 48;
    pConn->count_modes = 2;
    free(pMode);

    realRefresh = mode.refresh;
    mode.refresh = 48;
    modes.push_back(mode);
  } else {
    for (int i = 0; i < pConn->count_modes; ++i) {
      drmModeModeInfoPtr pMode = pConn->modes + i;
      HwcTestCrtc::Mode mode;
      mode.width = pMode->hdisplay;
      mode.height = pMode->vdisplay;
      mode.refresh = pMode->vrefresh;

      modes.push_back(mode);
    }
  }

  for (int i = 0; i < pConn->count_encoders; ++i) {
    HWCLOGI("  Encoder %d", pConn->encoders[i]);
    mConnectorForEncoder[pConn->encoders[i]] = connId;
  }

  ALOG_ASSERT(mPropMgr);

  if (mConnectors.find(connId) != mConnectors.end()) {
    Connector& conn = mConnectors[connId];
    DrmShimCrtc* crtc = conn.mCrtc;
    conn.mModes = modes;
    conn.mAttributes = 0;
    conn.mRealRefresh = realRefresh;
    mPropMgr->CheckConnectorProperties(connId, conn.mAttributes);
    if (mState->IsOptionEnabled(eOptSpoofDRRS) &&
        !connectorPhysicallyHotPluggable) {
      conn.mAttributes |= eDRRS;
    }

    conn.mRealDisplayType = connectorPhysicallyHotPluggable
                                ? HwcTestState::eRemovable
                                : HwcTestState::eFixed;

    if (crtc) {
      if (hotPluggable && (crtc->GetWidth() == 0)) {
        // New CRTC
        // Use default connection state
        bool plug = mState->GetNewDisplayConnectionState();
        HWCLOGD_COND(eLogHotPlug,
                     "Connector %d crtc %d using default connection state: %s",
                     connId, crtc->GetCrtcId(), plug ? "plug" : "unplug");
        crtc->SimulateHotPlug(plug);
      }

      crtc->SetAvailableModes(conn.mModes);

      if (!crtc->IsBehavingAsConnected()) {
        HWCLOGD_COND(eLogHotPlug,
                     "Connector %d CRTC %d hotplug spoof disconnected", connId,
                     crtc->GetCrtcId());
        pConn->connection = DRM_MODE_DISCONNECTED;
        pConn->count_modes = 0;
      }
    } else {
      HWCLOGD_COND(eLogHotPlug, "Connector %d known, but no CRTC", connId);

      if (hotPluggable && !mState->GetNewDisplayConnectionState()) {
        HWCLOGD_COND(eLogHotPlug, "Connector %d initial spoof hotunplugged",
                     connId);
        pConn->connection = DRM_MODE_DISCONNECTED;
        pConn->count_modes = 0;
      }
    }
  } else {
    Connector conn;
    conn.mCrtc = 0;
    conn.mModes = modes;
    conn.mAttributes = 0;
    conn.mRealRefresh = realRefresh;
    conn.mDisplayIx = eNoDisplayIx;
    mPropMgr->CheckConnectorProperties(connId, conn.mAttributes);

    if (mState->IsOptionEnabled(eOptSpoofDRRS) &&
        !connectorPhysicallyHotPluggable) {
      conn.mAttributes |= eDRRS;
    }

    conn.mRealDisplayType = connectorPhysicallyHotPluggable
                                ? HwcTestState::eRemovable
                                : HwcTestState::eFixed;
    mConnectors.emplace(connId, conn);

    if (hotPluggable && !mState->GetNewDisplayConnectionState()) {
      HWCLOGD_COND(eLogHotPlug, "New connector %d initial spoof hotunplugged",
                   connId);
      pConn->connection = DRM_MODE_DISCONNECTED;
      pConn->count_modes = 0;
    } else {
      HWCLOGD_COND(eLogHotPlug, "New connector %d state %s", connId,
                   (pConn->connection == DRM_MODE_CONNECTED) ? "connected"
                                                             : "disconnected");
    }
  }

  // drmModeGetConnector can take ages which means hot plug is delayed
  // indicate that this is OK.
  //TODO: How do we take care this situation?
  //mProtChecker.RestartSelfTeardown();
}

void DrmShimChecks::CheckGetEncoder(uint32_t encoder_id,
                                    drmModeEncoderPtr pEncoder) {
  if (pEncoder) {
    HWCLOGI(
        "DrmShimChecks::CheckGetEncoder encoder_id %d crtc_id %d "
        "possible_crtcs %d",
        encoder_id, pEncoder->crtc_id, pEncoder->possible_crtcs);
    mPossibleCrtcsForEncoder[encoder_id] = pEncoder->possible_crtcs;
  }
}

void DrmShimChecks::MapDisplay(int32_t displayIx, uint32_t connId,
                               uint32_t crtcId) {
  if (displayIx >= 0) {

    if (mConnectors.find(connId) != mConnectors.end()) {
      Connector& conn = mConnectors.at(connId);

      HWCLOGI("MapDisplay: Connector %d -> displayIx %d (%d modes) crtc %d@%p",
              connId, displayIx, conn.mModes.size(),
              (conn.mCrtc ? conn.mCrtc->GetCrtcId() : 0), conn.mCrtc);
      conn.mDisplayIx = displayIx;

      if (conn.mCrtc) {
        if (conn.mCrtc->GetCrtcId() != crtcId) {
          HWCLOGW(
              "Inconsistent connector-CRTC mapping. HWC says connector %d is "
              "crtc %d, we think crtc %d",
              connId, crtcId, conn.mCrtc->GetCrtcId());
        }
      }
    } else {
      HWCLOGW("MapDisplay: Connector %d UNKNOWN displayIx %d", connId,
              displayIx);
    }
  }
}

void DrmShimChecks::CheckSetCrtcEnter(int fd, uint32_t crtcId,
                                      uint32_t bufferId, uint32_t x, uint32_t y,
                                      uint32_t* connectors, int count,
                                      drmModeModeInfoPtr mode) {
  HWCLOGI("DrmShimChecks::CheckSetCrtcEnter @ %p: Crtc %d:", this, crtcId);
  HWCVAL_UNUSED(fd);
  HWCVAL_UNUSED(x);
  HWCVAL_UNUSED(y);

  if (!mode) {
    HWCLOGA("  No mode");
    return;
  }

  HWCLOGA("  Crtc %d Mode %s clock %d vrefresh %d flags %x aspect %s type %d",
          crtcId, mode->name, mode->clock, mode->vrefresh, mode->flags,
	  AspectStr(mode->flags), mode->type);
  HWCLOGI("  H Size %d sync start %d end %d total %d skew %d", mode->hdisplay,
          mode->hsync_start, mode->hsync_end, mode->htotal, mode->hskew);
  HWCLOGI("  V Size %d sync start %d end %d total %d scan %d", mode->vdisplay,
          mode->vsync_start, mode->vsync_end, mode->vtotal, mode->vscan);

  HWCVAL_LOCK(_l, mMutex);
  mWorkQueue.Process();

  HwcTestState::DisplayType displayType = HwcTestState::eFixed;
  int pipe = 0;

  // Determine display type
  for (int i = 0; i < count; ++i) {
    if (mHotPluggableConnectors.find(connectors[i]) != mHotPluggableConnectors.end()) {
      displayType = HwcTestState::eRemovable;
      pipe = i;
    }
  }

  // Do we already know about the CRTC?
  DrmShimCrtc* crtc;

  if ((mCrtcByPipe[pipe] == 0) && (mCrtcs.find(crtcId) == mCrtcs.end())) {
    // No
    // So let's create it
    crtc = new DrmShimCrtc(crtcId, mode->hdisplay, mode->vdisplay, mode->clock,
                           mode->vrefresh);
    crtc->SetChecks(this);
    crtc->SetPipeIndex(pipe);
    HwcTestCrtc::SeqVector* seq = mOrders[0];
    crtc->SetZOrder(seq);

    mCrtcs.emplace(crtcId, crtc);
    mCrtcByPipe[pipe] = crtc;
    HWCLOGD("Pipe %d has new CRTC %d Dimensions %dx%d clock %d refresh %d",
            pipe, crtcId, mode->hdisplay, mode->vdisplay, mode->clock,
            mode->vrefresh);
  } else if (mCrtcs.find(crtcId) == mCrtcs.end()) {
    crtc = mCrtcByPipe[pipe];
    HWCLOGD("Pipe %d CRTC %d maps to existing CRTC %d", pipe, crtcId,
            crtc->GetCrtcId());
    crtc->SetCrtcId(crtcId);
    mCrtcs.emplace(crtcId, crtc);
  } else {
    crtc = mCrtcs[crtcId];
    HWCLOGD("Reset mode for CRTC %d to %dx%d@%d", crtcId, mode->hdisplay,
            mode->vdisplay, mode->vrefresh);
  }

  crtc->SetDisplayType(displayType);

  HwcTestCrtc::Mode actualMode;
  actualMode.width = mode->hdisplay;
  actualMode.height = mode->vdisplay;
  actualMode.refresh = mode->vrefresh;
  crtc->SetActualMode(actualMode);

  DrmShimPlane* mainPlane = 0;

  if (mPlanes.find(crtcId) == mPlanes.end()) {
    if (!mUniversalPlanes) {
      HWCLOGD("Universal planes DISABLED: Creating main plane %d for crtc %d",
              crtcId, crtcId);
      // also create the main plane
      mainPlane = new DrmShimPlane(crtcId, crtc);
      mainPlane->SetPlaneIndex(0);
      mPlanes.emplace(crtcId, mainPlane);
      crtc->AddPlane(mainPlane);
    }
  } else {
    mainPlane = mPlanes[crtcId];
  }

  // Remember the connector->CRTC associations
  for (int i = 0; i < count; ++i) {
    uint32_t dix;

    if (mConnectors.find(connectors[i]) != mConnectors.end()) {
      Connector& conn = mConnectors.at(connectors[i]);

      // If we are spoofing DRRS
      // make sure we don't send weird refresh rates to DRM that can't deal with
      // it.
      if (conn.mRealRefresh > 0) {
        mode->vrefresh = conn.mRealRefresh;
      }

      conn.mCrtc = crtc;
      crtc->SetDisplayIx(conn.mDisplayIx);
      crtc->SetRealDisplayType(conn.mRealDisplayType);

      if (conn.mDisplayIx != eNoDisplayIx) {
        mCrtcByDisplayIx[conn.mDisplayIx] = crtc;
        mPersistentCrtcByDisplayIx[conn.mDisplayIx] = crtc;
      }

      HWCLOGI("  Connector %d -> CRTC %d D%d (%d modes)", connectors[i],
              crtc->GetCrtcId(), crtc->GetDisplayIx(), conn.mModes.size());
      crtc->SetAvailableModes(conn.mModes);
      dix = conn.mDisplayIx;

      if ((dix == 0) && (crtc->GetWidth() != 0)) {
        // D0 is being resized.
        // HWC will use a proxy, so the input dimensions will not change.
        HWCLOGD("D%d Crtc %d Setting OutDimensions %dx%d", crtc->GetDisplayIx(),
                crtc->GetCrtcId(), mode->hdisplay, mode->vdisplay);
        crtc->SetOutDimensions(mode->hdisplay, mode->vdisplay);
      } else {
        HWCLOGD("D%d Crtc %d Setting Dimensions %dx%d clock %d refresh %d",
                crtc->GetDisplayIx(), crtc->GetCrtcId(), mode->hdisplay,
                mode->vdisplay, mode->clock, mode->vrefresh);
        crtc->SetDimensions(mode->hdisplay, mode->vdisplay, mode->clock,
                            mode->vrefresh);
      }

      // Is there a logical display mapping set up? If so use it
      const char* ldmStr = mState->GetHwcOptionStr("dmconfig");

      if (ldmStr) {
        HWCLOGD("Logical display config will override: %s", ldmStr);
        ParseDmConfig(ldmStr);
      } else {
        HWCLOGD_COND(eLogMosaic, "No logical display config (dmconfig)");
      }
    } else {
      Connector conn;
      conn.mCrtc = crtc;
      conn.mDisplayIx = crtc->GetDisplayIx();
      dix = conn.mDisplayIx;
      mConnectors.emplace(connectors[i], conn);
      HWCLOGI("  Connector %d UNKNOWN -> CRTC %d D%d", connectors[i],
              crtc->GetCrtcId(), crtc->GetDisplayIx());
      ALOG_ASSERT(0);
    }

    crtc->SetConnector(connectors[i]);

    // Make sure no other CRTCs point to the same connector
    std::map<uint32_t, DrmShimCrtc*>::iterator crtcItr = mCrtcs.begin();
    for (; crtcItr != mCrtcs.end(); ++crtcItr) {
      DrmShimCrtc* otherCrtc = crtcItr->second;

      if (otherCrtc != crtc) {
        if (otherCrtc->GetDisplayIx() == dix) {
          otherCrtc->SetDisplayIx(-1);
        }
      }

      HWCLOGV_COND(eLogDrm, "Crtc %d -> D%d", otherCrtc->GetCrtcId(),
                   otherCrtc->GetDisplayIx());
    }

    for (uint32_t d = 0; d < HWCVAL_MAX_CRTCS; ++d) {
      if (mCrtcByDisplayIx[d]) {
        HWCLOGV_COND(eLogDrm, "D%d -> Crtc %d", d,
                     mCrtcByDisplayIx[d]->GetCrtcId());
      }
    }
  }

  // We were also supplied a buffer id, so behave as in SetPlane
  if ((bufferId != 0) && (mainPlane != 0)) {
    if (mBuffersByFbId.find(bufferId) == mBuffersByFbId.end()) {
      // This buffer not known to us
      // probably blanking
      mainPlane->ClearBuf();
    } else {
      std::shared_ptr<DrmShimBuffer> buf =
          UpdateBufferPlane(bufferId, crtc, mainPlane);
    }
  }

  // Clear ESD recovery
  crtc->EsdStateTransition(HwcTestCrtc::eEsdDpmsOff, HwcTestCrtc::eEsdModeSet);

}

void DrmShimChecks::CheckSetCrtcExit(int fd, uint32_t crtcId, uint32_t ret) {
  HWCLOGD("DrmShimChecks::CheckSetCrtcExit @ %p: Crtc %d:", this, crtcId);
  HWCVAL_UNUSED(fd);

  HWCVAL_LOCK(_l, mMutex);
  mWorkQueue.Process();

  HwcTestCrtc* crtc = GetCrtc(crtcId);

  if (ret == 0) {
    crtc->SetModeSet(true);
  } else {
    HWCERROR(eCheckDrmCallSuccess, "drmModeSetCrtcExit failed to CRTC %d",
             crtcId);
  }
}

void DrmShimChecks::CheckGetCrtcExit(uint32_t crtcId, drmModeCrtcPtr pCrtc) {
  HWCLOGI("GetCrtc: Crtc %d:", crtcId);
  HWCLOGI("  Mode %s", pCrtc->mode.name);
  HWCLOGI("  Clock %d vrefresh %d flags %d type %d", pCrtc->mode.clock,
          pCrtc->mode.vrefresh, pCrtc->mode.flags, pCrtc->mode.type);
  HWCLOGI("  H Size %d sync start %d end %d total %d skew %d",
          pCrtc->mode.hdisplay, pCrtc->mode.hsync_start, pCrtc->mode.hsync_end,
          pCrtc->mode.htotal, pCrtc->mode.hskew);
  HWCLOGI("  V Size %d sync start %d end %d total %d scan %d",
          pCrtc->mode.vdisplay, pCrtc->mode.vsync_start, pCrtc->mode.vsync_end,
          pCrtc->mode.vtotal, pCrtc->mode.vscan);

  // Create CRTC record on drmModeSetCrtc, not here
  // since we have no idea what display index is at this point.
}

/// Check for drmModeGetPlaneResources
void DrmShimChecks::CheckGetPlaneResourcesExit(drmModePlaneResPtr pRes) {
  HWCVAL_LOCK(_l, mMutex);
  // Record the existence of the "new" planes
  for (uint32_t i = 0; i < pRes->count_planes; ++i) {
    if (mPlanes.find(pRes->planes[i]) == mPlanes.end()) {
      DrmShimPlane* plane = new DrmShimPlane(pRes->planes[i]);

      HWCLOGI("GetPlaneResources: new plane %d", pRes->planes[i]);
      mPlanes.emplace(pRes->planes[i], plane);
    }
  }
}

DrmShimCrtc* DrmShimChecks::CreatePipe(uint32_t pipe, uint32_t crtcId) {
  DrmShimCrtc* crtc = mCrtcByPipe[pipe];

  if (crtc == 0) {
    // Create the crtc, drmModeGetCrtc has not been called yet.
    // So, we don't yet know the CRTC id
    HWCLOGD("Creating new CRTC %d for pipe %d with unknown CRTC id", crtcId,
            pipe);
    crtc = new DrmShimCrtc(crtcId, 0, 0, 0, 0);
    crtc->SetChecks(this);
    crtc->SetPipeIndex(pipe);
    HwcTestCrtc::SeqVector* seq = mOrders[0];
    crtc->SetZOrder(seq);
    mCrtcByPipe[pipe] = crtc;
  } else if ((crtcId > 0) && (crtcId != crtc->GetCrtcId())) {
    HWCLOGW("Pipe %d existing CRTC has id %d, should be %d", pipe,
            crtc->GetCrtcId(), crtcId);
    ALOG_ASSERT(crtcId == crtc->GetCrtcId());
  }

  return crtc;
}

/// Check for drmModeGetPlane
void DrmShimChecks::CheckGetPlaneExit(uint32_t plane_id,
                                      drmModePlanePtr pPlane) {
  HWCVAL_LOCK(_l, mMutex);

  // Record the association between the plane and Crtc
  // Note: pPlane->crtc_id is actually null, so we have to use the last CRTC Id
  // set in GetCrtc
  DrmShimPlane* planeIx = mPlanes[plane_id];

  uint32_t pipe = 0;
  while ((((1 << pipe) & (pPlane->possible_crtcs)) == 0) &&
         (pipe < HWCVAL_MAX_CRTCS)) {
    ++pipe;
  }
  HWCLOGD(
      "CheckGetPlaneExit: plane %d possible_crtcs 0x%x crtc_id %d planeIx %d "
      "pipe %d",
      plane_id, pPlane->possible_crtcs, pPlane->crtc_id, planeIx, pipe);

  if (((uint32_t)(1 << pipe)) != pPlane->possible_crtcs) {
    HWCERROR(eCheckDrmShimFail,
             "Plane %d mapped to multiple/unknown CRTCs. possible_crtcs=0x%x",
             plane_id, pPlane->possible_crtcs);
    return;
  }

  if (mPlanes.find(plane_id) != mPlanes.end()) {
    DrmShimPlane* plane = mPlanes[plane_id];
    if (pipe < HWCVAL_MAX_CRTCS) {
      DrmShimCrtc* crtc = CreatePipe(pipe);

      plane->SetCrtc(crtc);

#ifdef DRM_PLANE_TYPE_CURSOR
      int32_t plane_type = mPropMgr->GetPlaneType(plane_id);
      if (plane_type == DRM_PLANE_TYPE_CURSOR) {
        HWCLOGD("CheckGetPlaneExit: NOT adding cursor plane %d to crtc %d",
                plane_id, (crtc ? crtc->GetCrtcId() : 0));
      } else {
#endif
        HWCLOGD("CheckGetPlaneExit: adding plane %p to crtc %d", plane,
                (crtc ? crtc->GetCrtcId() : 0));
        crtc->AddPlane(plane);
#ifdef DRM_PLANE_TYPE_CURSOR
      }
#endif

      HWCLOGI(
          "CheckGetPlaneExit: plane %d possible_crtcs 0x%x associated with "
          "crtc %d",
          plane_id, pPlane->possible_crtcs, (crtc ? crtc->GetCrtcId() : 0));
    } else {
      HWCLOGW("CheckGetPlaneExit: Crtc for pipe %d not valid", pipe);
    }
  } else {
    HWCLOGI(
        "CheckGetPlaneExit: plane %d not previously found by GetPlaneResources",
        plane_id);
  }
}

/// Check for drmModeAddFB and drmModeAddFB2
void DrmShimChecks::CheckAddFB(int fd, uint32_t width, uint32_t height,
                               uint32_t pixel_format, uint32_t depth,
                               uint32_t bpp, uint32_t bo_handles[4],
                               uint32_t pitches[4], uint32_t offsets[4],
                               uint32_t buf_id, uint32_t flags,
                               __u64 modifier[4], int ret) {
  HWCVAL_UNUSED(width);
  HWCVAL_UNUSED(height);

  // AddFB can take up to four handles, for cases where the channels are in
  // separate buffers.
  // At time of writing this is not required for any buffer HWC would give an FB
  // to.
  // This will change in future when NV12 buffers are supported by hardware.
  uint32_t boHandle = bo_handles[0];

  if ((ret == 0) && (buf_id > 0)) {
    HWCLOGV_COND(eLogDrm,
                 "drmModeAddFB: buf_id %d pixel_format 0x%x depth %d bpp %d "
                 "boHandles/pitches/offsets/modifier "
                 "(0x%x/%d/%d/%llu,0x%x/%d/%d/%llu,0x%x/%d/%d/%llu,0x%x/%d/%d/"
                 "%llu) flags %d",
                 buf_id, pixel_format, depth, bpp, bo_handles[0], pitches[0],
                 offsets[0], modifier[0], bo_handles[1], pitches[1], offsets[1],
                 modifier[1], bo_handles[2], pitches[2], offsets[2],
                 modifier[2], bo_handles[3], pitches[3], offsets[3],
                 modifier[3], flags);

    if (flags & DRM_MODE_FB_AUX_PLANE) {
      // Aux buffer detected - save the pitch, offset and modifier
      HWCLOGV_COND(eLogDrm,
                   "drmModeAddFB: Aux buffer detected for buf_id %d - pitch is "
                   "%d - offset is %d - modifier is %llu",
                   buf_id, pitches[1], offsets[1], modifier[1]);
      mWorkQueue.Push(std::shared_ptr<Hwcval::Work::Item>(new Hwcval::Work::AddFbItem(
          fd, boHandle, buf_id, width, height, pixel_format, pitches[1],
          offsets[1], modifier[1])));
    } else {
      mWorkQueue.Push(std::shared_ptr<Hwcval::Work::Item>(new Hwcval::Work::AddFbItem(fd, boHandle, buf_id, width,
                                                  height, pixel_format)));
    }
  } else {
    HWCLOGW("drmModeAddFB handle 0x%x failed to allocate FB ID %d status %d",
            boHandle, buf_id, ret);
    HWCLOGD(
        "buf_id %d pixel_format 0x%x depth %d bpp %d boHandles/pitches/offsets "
        "(0x%x/%d/%d,0x%x/%d/%d,0x%x/%d/%d,0x%x/%d/%d) flags %d",
        buf_id, pixel_format, depth, bpp, bo_handles[0], pitches[0], offsets[0],
        bo_handles[1], pitches[1], offsets[1], bo_handles[2], pitches[2],
        offsets[2], bo_handles[3], pitches[3], offsets[3], flags);
  }
}

void DrmShimChecks::CheckRmFB(int fd, uint32_t bufferId) {
  mWorkQueue.Push(std::shared_ptr<Hwcval::Work::Item>(new Hwcval::Work::RmFbItem(fd, bufferId)));
}

/// Work queue processing for drmModeAddFB and drmModeAddFB2
///
/// These associate a framebuffer id (FB ID) with a buffer object (bo).
void DrmShimChecks::DoWork(const Hwcval::Work::AddFbItem& item) {
  char str[HWCVAL_DEFAULT_STRLEN];
  uint32_t pixelFormat = item.mPixelFormat;
  uint32_t auxPitch = item.mAuxPitch;
  uint32_t auxOffset = item.mAuxOffset;
  __u64 modifier = item.mModifier;

  if (item.mHasAuxBuffer) {
    HWCLOGD(
        "DoWork AddFbItem FB %d fd %d boHandle 0x%x (Aux buffer detected - "
        "pitch: %d offset: %d modifier: %llu)",
        item.mFbId, item.mFd, item.mBoHandle, auxPitch, auxOffset, modifier);
  } else {
    HWCLOGD("DoWork AddFbItem FB %d fd %d boHandle 0x%x", item.mFbId, item.mFd,
            item.mBoHandle);
  }

  BoKey k = {item.mFd, item.mBoHandle};

  if (mBosByBoHandle.find(k) != mBosByBoHandle.end()) {
    // We found the bo record for this bo handle, add the FB to the
    // DrmShimBuffer
    // and make sure it is indexed.
    std::shared_ptr<HwcTestBufferObject> bo = mBosByBoHandle[k];
    std::shared_ptr<DrmShimBuffer> buf = bo->mBuf;
    HWCLOGD_COND(eLogBuffer, "AddFb found bo %s, buf@%p", bo->IdStr(str),
                 buf.get());

    if (buf.get()) {
      DrmShimBuffer::FbIdData data;
      data.pixelFormat = pixelFormat;
      data.hasAuxBuffer = item.mHasAuxBuffer;
      data.auxPitch = auxPitch;
      data.auxOffset = auxOffset;
      data.modifier = modifier;

      buf->GetFbIds().emplace(item.mFbId, data);
      mBuffersByFbId[item.mFbId] = buf;
      // TODO: what if this FB ID previously belonged to a different buffer?

      HWCLOGD_COND(eLogBuffer,
                   "drmModeAddFB[2]: Add FB %d to %s pixelFormat 0x%x",
                   item.mFbId, buf->IdStr(str), pixelFormat);
    } else {
      // Sometimes the addFB comes before the create. Why??
      //
      // Create a dummy DrmShimBuffer, this will get more information later
      // when RecordBufferState is called.
      buf = std::shared_ptr<DrmShimBuffer>(new DrmShimBuffer(0));

      // Assume this is a blanking or empty buffer until it is associated with a
      // handle.
      // We decide which based on the size of the buffer - this is based on our
      // knowledge of how HWC works.
      if (BelievedEmpty(item.mWidth, item.mHeight)) {
        buf->SetBlack(true);
      } else {
        buf->SetBlanking(true);
      }

      // Associate the bo with the DrmShimBuffer.
      bo->mBuf = buf;
      buf->AddBo(bo);

      // Add the FB ID to the new buffer
      DrmShimBuffer::FbIdData data;
      data.pixelFormat = pixelFormat;
      data.hasAuxBuffer = item.mHasAuxBuffer;
      data.auxPitch = auxPitch;
      data.auxOffset = auxOffset;
      data.modifier = modifier;

      buf->GetFbIds().emplace(item.mFbId, data);
      mBuffersByFbId[item.mFbId] = buf;
      // TODO: what if this FB ID previously belonged to a different buffer?

      char str[HWCVAL_DEFAULT_STRLEN];
      HWCLOGD_COND(eLogBuffer,
                   "drmModeAddFB[2]: Add FB %d to new %s pixelFormat 0x%x",
                   item.mFbId, buf->IdStr(str), pixelFormat);
    }
  } else {
    // We don't know about this bo handle.
    // This can happen sometimes - the AddFB happens before the bo is apparently
    // created. Or it could be that we
    // previously dispensed with a bo record because it didn't seem to be used
    // any more, and now we need it again.
    // Either way, we create a new buffer object and associate it with the FB
    // ID.
    std::shared_ptr<HwcTestBufferObject> bo = std::shared_ptr<HwcTestBufferObject>(
        new HwcTestBufferObject(item.mFd, item.mBoHandle));
    DrmShimBuffer::FbIdData data;
    data.pixelFormat = pixelFormat;
    data.hasAuxBuffer = item.mHasAuxBuffer;
    data.auxPitch = auxPitch;
    data.auxOffset = auxOffset;
    data.modifier = modifier;
    std::shared_ptr<DrmShimBuffer> buf = std::shared_ptr<DrmShimBuffer>(new DrmShimBuffer(0));
    buf->GetFbIds().emplace(item.mFbId, data);
    mBuffersByFbId[item.mFbId] = buf;
    mBosByBoHandle.emplace(k, bo);

    buf->AddBo(bo);
    // TODO: what if this FB ID previously belonged to a different buffer?

    char str[HWCVAL_DEFAULT_STRLEN];
    HWCLOGD_COND(eLogBuffer, "drmModeAddFB[2]: NEW FB %d %s pixelFormat 0x%x",
                 item.mFbId, bo->IdStr(str), pixelFormat);
  }
}

/// Work queue processing for drmModeRmFB.
/// This remove a framebuffer ID (FB ID) from a buffer object.
void DrmShimChecks::DoWork(const Hwcval::Work::RmFbItem& item) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  if (mBuffersByFbId.find(item.mFbId) != mBuffersByFbId.end()) {
    // We found the bo from the FB ID index, so remove the FB ID from the BO and
    // from the index.
    std::shared_ptr<DrmShimBuffer> buf = mBuffersByFbId[item.mFbId];
    buf->GetFbIds().erase(item.mFbId);
    mBuffersByFbId.erase(item.mFbId);

    HWCLOGD_COND(eLogBuffer,
                 "drmModeRmFB: Removed association of FB %d with %s",
                 item.mFbId, buf->IdStr(strbuf));
  } else {
    // RmFB may happen after the buffer object has already been closed, so this
    // is not an error.
    HWCLOGW_COND(eLogBuffer, "drmModeRmFB: Unknown FB ID %d", item.mFbId);
  }
}

void DrmShimChecks::checkPageFlipEnter(int fd, uint32_t crtc_id, uint32_t fb_id,
                                       uint32_t flags, void*& user_data) {
  if (mState->IsCheckEnabled(eLogDrm)) {
    HWCLOGD(
        "Enter DrmShimChecks::checkPageFlipEnter fd %x crtc_id %d FB %d flags "
        "%x user_data %p",
        fd, crtc_id, fb_id, flags, user_data);
  }

  if (mState->IsBufferMonitorEnabled()) {
    std::shared_ptr<DrmShimBuffer> buf;
    {
      // Note, this lock must be released BEFORE mCompVal->Compare() is called.
      HWCVAL_LOCK(_l, mMutex);
      mWorkQueue.Process();

      DrmShimPlane* mainPlane = mPlanes[crtc_id];
      HWCCHECK(eCheckInvalidCrtc);
      if (mainPlane == 0) {
        HWCERROR(eCheckInvalidCrtc, "Unknown CRTC %d", crtc_id);
        return;
      }

      uint32_t fbForCrtc = mainPlane->GetCurrentDsId();

      // Get pointer to internal CRTC object
      DrmShimCrtc* crtc = static_cast<DrmShimCrtc*>(mainPlane->GetCrtc());
      HWCCHECK(eCheckInvalidCrtc);
      if (crtc == 0) {
        HWCERROR(eCheckInvalidCrtc, "Could not find a crtc entry for id %d",
                 crtc_id);
        return;
      }

      // Not a dropped frame
      crtc->IncDrawCount();

      // Not disabled (anymore)
      crtc->SetMainPlaneDisabled(false);

      // Record frame number to check for flicker
      crtc->SetDrmFrame();
      mainPlane->DrmCallStart();

      if (mState->IsOptionEnabled(eOptPageFlipInterception)) {
        if (user_data) {
          HWCLOGD_COND(eLogEventHandler, "Crtc %d saving user data %p",
                       crtc->GetCrtcId(), user_data);
          crtc->SavePageFlipUserData(uint64_t(user_data));
          user_data = (void*)(uintptr_t) crtc->GetCrtcId();
          HWCLOGD_COND(eLogEventHandler,
                       "Crtc %d Page flip user data shimmed with crtc %p",
                       crtc->GetCrtcId(), user_data);
        }
      }

      if (fbForCrtc == fb_id) {
      } else {
        if (fb_id != 0) {
          if (mBuffersByFbId.find(fb_id) == mBuffersByFbId.end()) {
            // This buffer not known to us
            // probably blanking
            mainPlane->ClearBuf();
            return;
          }

          buf = UpdateBufferPlane(fb_id, crtc, mainPlane);

          if (buf.get() == 0) {
            // Blanking buffer
            return;
          }

          // Set expected dimensions
          mainPlane->SetDisplayFrame(0, 0, buf->GetWidth(), buf->GetHeight());
          mainPlane->SetSourceCrop(0, 0, buf->GetWidth(), buf->GetHeight());

          if (buf->GetHandle() != 0) {
            // Allocated size of buffer must be at least full screen
            HWCCHECK(eCheckMainPlaneFullScreen);
            if ((buf->GetAllocWidth() < crtc->GetWidth()) ||
                buf->GetAllocHeight() < crtc->GetHeight()) {
              HWCERROR(eCheckMainPlaneFullScreen, "Size is %dx%d",
                       buf->GetAllocWidth(), buf->GetAllocHeight());
            }
          }
        } else {
          mainPlane->ClearBuf();
        }
      }
    }

    // Check the composition that created this buffer, if there was one
    mCompVal->Compare(buf);
  }
}

HwcTestBufferObject* DrmShimChecks::CreateBufferObject(int fd,
                                                       uint32_t boHandle) {
  return new HwcTestBufferObject(fd, boHandle);
}

std::shared_ptr<HwcTestBufferObject> DrmShimChecks::GetBufferObject(
    uint32_t boHandle) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  BoKey k = {mShimDrmFd, boHandle};
  std::shared_ptr<HwcTestBufferObject> bo = mBosByBoHandle[k];

  if (bo.get() == 0) {
    bo = std::shared_ptr<HwcTestBufferObject>(CreateBufferObject(mShimDrmFd, boHandle));
    HWCLOGV_COND(eLogBuffer, "GetBufferObject: fd %d boHandle 0x%x created %s",
                 mShimDrmFd, boHandle, bo->IdStr(strbuf));
    mBosByBoHandle.emplace(k, bo);
  } else {
    HWCLOGV_COND(eLogBuffer, "GetBufferObject: fd %d boHandle 0x%x found %s",
                 mShimDrmFd, boHandle, bo->IdStr(strbuf));
  }

  return bo;
}

void DrmShimChecks::checkPageFlipExit(int fd, uint32_t crtc_id, uint32_t fb_id,
                                      uint32_t flags, void* user_data,
                                      int ret) {
  HWCLOGV_COND(eLogDrm,
               "Enter DrmShimChecks::checkPageFlipExit fd %x crtc_id %d FB %d "
               "flags %x user_data %p",
               fd, crtc_id, fb_id, flags, user_data);

  HWCCHECK(eCheckDrmCallSuccess);
  if (ret) {
    HWCERROR(eCheckDrmCallSuccess, "Page flip failed to crtc %d (status %d)",
             crtc_id, ret);
  }

  if (mState->IsBufferMonitorEnabled()) {
    HWCVAL_LOCK(_l, mMutex);
    mWorkQueue.Process();

    if (mPlanes.find(crtc_id) == mPlanes.end()) {
      return;
    }

    DrmShimPlane* mainPlane = mPlanes[crtc_id];
    int32_t callDuration = mainPlane->GetDrmCallDuration();

    if (callDuration > HWCVAL_DRM_CALL_DURATION_WARNING_LEVEL_NS) {
      HWCLOGW("PageFlip to crtc %d took %fms", crtc_id,
              ((double)callDuration) / 1000000.0);
    }

    DrmShimCrtc* crtc = static_cast<DrmShimCrtc*>(mainPlane->GetCrtc());

    // Record frame number to check for flicker
    crtc->SetDrmFrame();
  }
}

void DrmShimChecks::checkSetPlaneEnter(int fd, uint32_t plane_id,
                                       uint32_t crtc_id, uint32_t fb_id,
                                       uint32_t flags, uint32_t crtc_x,
                                       uint32_t crtc_y, uint32_t crtc_w,
                                       uint32_t crtc_h, uint32_t src_x,
                                       uint32_t src_y, uint32_t src_w,
                                       uint32_t src_h, void*& user_data) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  if (mState->IsCheckEnabled(eLogDrm)) {
    HWCLOGD("Enter DrmShimChecks::checkSetPlaneEnter");
    HWCLOGD("  -- fd %x plane id %d crtc_id %d FB %u flags %d ud %p", fd,
            plane_id, crtc_id, fb_id, flags, user_data);
    HWCLOGD(
        "  -- src x,y,w,h (%4.2f, %4.2f, %4.2f, %4.2f) crtc (%d, %d, %d, %d)",
        (double)src_x / 65536.0, (double)src_y / 65536.0,
        (double)src_w / 65536.0, (double)src_h / 65536.0, crtc_x, crtc_y,
        crtc_w, crtc_h);
  }

  if (mState->IsBufferMonitorEnabled()) {
    std::shared_ptr<DrmShimBuffer> buf;

    {
      // Note, this lock must be released BEFORE mCompVal->Compare() is called.
      HWCVAL_LOCK(_l, mMutex);
      mWorkQueue.Process();

      DrmShimPlane* plane = mPlanes[plane_id];

      HWCCHECK(eCheckPlaneIdInvalidForCrtc);
      if (plane == 0) {
        HWCERROR(eCheckPlaneIdInvalidForCrtc, "Unknown plane %d", plane_id);
        return;
      }

      // Get pointer to internal CRTC object
      DrmShimCrtc* crtc = static_cast<DrmShimCrtc*>(plane->GetCrtc());
      // HWCCHECK already done
      if (crtc == 0) {
        HWCERROR(eCheckPlaneIdInvalidForCrtc,
                 "No entry for crtc %d on plane %d", crtc_id, plane_id);
        return;
      }

      // HWCCHECK already done
      if (crtc->GetCrtcId() != crtc_id) {
        HWCERROR(eCheckPlaneIdInvalidForCrtc, "Plane %d sent to wrong CRTC %d",
                 plane_id, crtc_id);
        return;
      }

      // Not a dropped frame
      crtc->IncDrawCount();

      // Record frame number to check for flicker
      crtc->SetDrmFrame();
      plane->DrmCallStart();

      if (mState->IsOptionEnabled(eOptPageFlipInterception)) {
        if (user_data) {
          // Setup page flip event
          HWCLOGD_COND(eLogEventHandler, "Crtc %d saving user data %p",
                       crtc->GetCrtcId(), user_data);
          crtc->SavePageFlipUserData(uint64_t(user_data));
          user_data = (void*)(uintptr_t) crtc_id;
          HWCLOGD_COND(eLogEventHandler,
                       "Crtc %d Page flip user data shimmed with crtc %p",
                       crtc->GetCrtcId(), user_data);
        }
      }

      if (plane->GetCurrentDsId() == fb_id) {
        // Set expected dimensions
        plane->SetDisplayFrame(crtc_x, crtc_y, crtc_w, crtc_h);
        plane->SetSourceCrop(float(src_x) / 65536.0, float(src_y) / 65536.0,
                             float(src_w) / 65536.0, float(src_h) / 65536.0);

        // check for disabling main plane
        // TODO: check proper usage of this flag
        if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
          HWCLOGD(
              "Detected callback to force main plane disabled on FB %d plane "
              "%d",
              fb_id, plane_id);
          plane->GetCrtc()->SetMainPlaneDisabled(true);
        }
      } else {
        // fb_id=0 means we are turning off the plane.
        if (fb_id != 0) {
          // check for disabling main plane
          // TODO: check proper usage of this flag
          if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
            HWCLOGD(
                "Detected callback to force main plane disabled on FB %d plane "
                "%d",
                fb_id, plane_id);
            plane->GetCrtc()->SetMainPlaneDisabled(true);
          }

          buf = UpdateBufferPlane(fb_id, crtc, plane);
          double w = ((double)src_w) / 65536.0;
          double h = ((double)src_h) / 65536.0;

          // Null DrmShimBuffer implies blanking.
          if (buf.get()) {
            // TODO: check if fence indicates that buffer is ready for display

            // Don't perform checks against alloc width & height for blanking
            // buffers
            // as gralloc often only gives zeroes for these.
            if (!buf->IsBlanking() && !buf->IsBlack()) {
              HWCCHECK(eCheckBufferTooSmall);
              if ((w > buf->GetAllocWidth()) || (h > buf->GetAllocHeight())) {
                HWCERROR(
                    eCheckBufferTooSmall,
                    "Plane %d %s %dx%d (alloc %dx%d) Crop %fx%f Display %dx%d",
                    plane_id, buf->IdStr(strbuf), buf->GetWidth(),
                    buf->GetHeight(), buf->GetAllocWidth(),
                    buf->GetAllocHeight(), w, h, crtc_w, crtc_h);
              }
            }

            HWCCHECK(eCheckDisplayCropEqualDisplayFrame);
            if ((fabs(w - crtc_w) > 1.0) || (fabs(h - crtc_h) > 1.0)) {
              HWCERROR(
                  eCheckDisplayCropEqualDisplayFrame,
                  "Plane %d %s %dx%d (alloc %dx%d) Crop %fx%f Display %dx%d",
                  plane_id, buf->IdStr(strbuf), buf->GetWidth(),
                  buf->GetHeight(), buf->GetAllocWidth(), buf->GetAllocHeight(),
                  w, h, crtc_w, crtc_h);
            }
          }

          // Set expected dimensions
          plane->SetDisplayFrame((int32_t)crtc_x, (int32_t)crtc_y, crtc_w,
                                 crtc_h);
          plane->SetSourceCrop(float(src_x) / 65536.0, float(src_y) / 65536.0,
                               w, h);
        } else {
          plane->ClearBuf();
        }
      }
    }

    // Check the composition that created this buffer, if there was one
    mCompVal->Compare(buf);
  }
}

void DrmShimChecks::checkSetPlaneExit(
    int fd, uint32_t plane_id, uint32_t crtc_id, uint32_t fb_id, uint32_t flags,
    uint32_t crtc_x, uint32_t crtc_y, uint32_t crtc_w, uint32_t crtc_h,
    uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, int ret) {
  HWCVAL_UNUSED(fd);
  HWCVAL_UNUSED(crtc_id);
  HWCVAL_UNUSED(fb_id);
  HWCVAL_UNUSED(flags);
  HWCVAL_UNUSED(crtc_x);
  HWCVAL_UNUSED(crtc_y);
  HWCVAL_UNUSED(crtc_w);
  HWCVAL_UNUSED(crtc_h);
  HWCVAL_UNUSED(src_x);
  HWCVAL_UNUSED(src_y);
  HWCVAL_UNUSED(src_w);
  HWCVAL_UNUSED(src_h);

  HWCLOGV_COND(eLogDrm, "Enter DrmShimChecks::checkSetPlaneExit plane_id %d",
               plane_id);

  HWCCHECK(eCheckDrmCallSuccess);
  if (ret) {
    HWCERROR(eCheckDrmCallSuccess, "SetPlane failed to plane %d (status %d)",
             plane_id, ret);
  }

  if (mState->IsBufferMonitorEnabled()) {
    HWCVAL_LOCK(_l, mMutex);
    mWorkQueue.Process();

    DrmShimPlane* plane = mPlanes[plane_id];

    if (plane == 0) {
      return;
    }

    // Get pointer to internal CRTC object
    DrmShimCrtc* crtc = static_cast<DrmShimCrtc*>(plane->GetCrtc());

    if (crtc == 0) {
      return;
    }

    int32_t callDuration = plane->GetDrmCallDuration();

    if (callDuration > HWCVAL_DRM_CALL_DURATION_WARNING_LEVEL_NS) {
      HWCLOGW("SetPlane to plane %d took %fms", plane_id,
              ((double)callDuration) / 1000000.0);
    }

    // Record frame number to check for flicker
    crtc->SetDrmFrame();
  }
}

uint32_t DrmShimChecks::DrmTransformToHalTransform(
    HwcTestState::DeviceType deviceType, uint32_t drmTransform) {
  switch (drmTransform) {
    case DRM_MODE_ROTATE_0:
      return hwcomposer::HWCTransform::kIdentity;
    case DRM_MODE_ROTATE_270:
      return hwcomposer::HWCTransform::kTransform270;
    case DRM_MODE_ROTATE_180:
      return hwcomposer::HWCTransform::kTransform180;
    case DRM_MODE_ROTATE_90:
      return hwcomposer::HWCTransform::kTransform90;
    case DRM_MODE_REFLECT_X:
      return hwcomposer::HWCTransform::kReflectX;
    case DRM_MODE_REFLECT_Y:
      return hwcomposer::HWCTransform::kReflectY;
    default:
      HWCERROR(eCheckNuclearParams, "Invalid BXT transform value %d",
               drmTransform);
      return 0;
  }
}

static const double cdClkBxt = 288000;

// Validate any possible plane scaling against restrictions on Broxton.
// Return the number of scalers used by this plane (0 or 1).
uint32_t DrmShimChecks::BroxtonPlaneValidation(HwcTestCrtc* crtc,
                                               std::shared_ptr<DrmShimBuffer> buf,
                                               const char* str, uint32_t id,
                                               double srcW, double srcH,
                                               uint32_t dstW, uint32_t dstH,
                                               uint32_t transform) {
  // If 90 degree rotation is in use then we must swap width & height of one of
  // the co-ordinate pairs
  // At time of writing, it is TBC that this works, but it won't be great if it
  // doesn't!
  uint32_t logDstW;
  uint32_t logDstH;

  if (transform != hwcomposer::HWCTransform::kIdentity) {
    ++sHwPlaneTransformUsedCounter;
  }

  if (transform & hwcomposer::HWCTransform::kTransform90) {
    // Render compression should not be combined with 90/270 degree rotation
    HWCCHECK(eCheckRCWithInvalidRotation);
    if (buf.get() && buf->IsRenderCompressed()) {
      HWCERROR(eCheckRCWithInvalidRotation,
               "Can not rotate 90/270 degrees with Render Compression");
    }

    logDstW = dstH;
    logDstH = dstW;
  } else {
    logDstW = dstW;
    logDstH = dstH;
  }

  char strbuf[HWCVAL_DEFAULT_STRLEN];

  if ((fabs(srcW - logDstW) > 1.0) || (fabs(srcH - logDstH) > 1.0)) {
    ++sHwPlaneScaleUsedCounter;
    HWCCHECK(eCheckBadScalerSourceSize);

    if ((srcW < 8) || (srcH < 8) || (srcW > 4096)) {
      HWCERROR(eCheckBadScalerSourceSize,
               "%s %d %s Crop %fx%f, for BXT should be 8-4096 pixels.", str, id,
               buf.get() ? buf->IdStr(strbuf) : "", srcW, srcH);
    } else if (buf->IsVideoFormat()) {
      if (srcH < 16) {
        HWCERROR(eCheckBadScalerSourceSize,
                 "%s %d %s Crop %fx%f, for BXT min height for YUV 420 "
                 "planar/NV12 formats is 16 pixels",
                 str, id, buf.get() ? buf->IdStr(strbuf) : "", srcW, srcH);
      }

      // Check for minimum value for NV12.
      if (buf->IsNV12Format()) {
        if (srcW < 16) {
          HWCERROR(eCheckBadScalerSourceSize,
                   "%s %d %s Crop %fx%f, for BXT min width for NV12 formats is "
                   "16 pixels",
                   str, id, buf.get() ? buf->IdStr(strbuf) : "", srcW, srcH);
        }
      }
    }

    // "Plane/Pipe scaling is not compatible with interlaced fetch mode."
    // HWC does not use this.

    // "Plane up and down scaling is not compatible with keying."
    // HWC does not use keying (i.e. transparent colour).

    // "Plane scaling is not compatible with the indexed 8-bit, XR_BIAS and
    // floating point source pixel formats"
    // Not currently used in HWC.

    // Scale factor must be >1/2 for NV12, >1/3 for other formats
    double minScale = 1.0 / 3.0;

    if (buf->IsNV12Format()) {
      minScale = 0.5;
    }

    double minScaleFromBandwidth = 0.0;

    if (crtc) {
      double crtClk = crtc->GetClock();

      if (crtClk) {
        minScaleFromBandwidth = crtClk / cdClkBxt;
        minScale = max(minScale, minScaleFromBandwidth);
        HWCLOGV_COND(eLogDrm, "CrtClk %f cdClkBxt %f minScaleFromBandwidth %f",
                     crtClk, cdClkBxt, minScaleFromBandwidth);
      } else {
        HWCLOGV_COND(eLogDrm, "BroxtonPlaneValidation: no crtclk for CRTC %d",
                     crtc->GetCrtcId());
      }
    }

    double xScale = double(logDstW) / srcW;
    double yScale = double(logDstH) / srcH;
    HWCLOGV_COND(eLogDrm,
                 "BroxtonPlaneValidation: %s %d scale %fx%f minScale %f", str,
                 id, xScale, yScale, minScale);

    HWCCHECK(eCheckScalingFactor);
    if ((xScale <= minScale) || (yScale <= minScale)) {
      HWCERROR(eCheckScalingFactor,
               "%s %d %s %dx%d (alloc %dx%d) Crop %fx%f Display (in source "
               "frame) %dx%d Scale %fx%f",
               str, id, buf.get() ? buf->IdStr(strbuf) : "", buf->GetWidth(),
               buf->GetHeight(), buf->GetAllocWidth(), buf->GetAllocHeight(),
               srcW, srcH, logDstW, logDstH, xScale, yScale);
      HWCLOGE("  -- Minimum supported scale factor for %s is %f",
              buf->StrBufFormat(), minScale);
    } else if ((xScale * yScale) <= minScaleFromBandwidth) {
      HWCERROR(eCheckScalingFactor,
               "%s %d %s %dx%d (alloc %dx%d) Crop %fx%f Display (in source "
               "frame) %dx%d Scale %fx%f=%f",
               str, id, buf.get() ? buf->IdStr(strbuf) : "", buf->GetWidth(),
               buf->GetHeight(), buf->GetAllocWidth(), buf->GetAllocHeight(),
               srcW, srcH, logDstW, logDstH, xScale, yScale, xScale * yScale);
      HWCLOGE("  -- Minimum supported scale factor for product %s is %f",
              buf->StrBufFormat(), minScale);
    }

    // Scaling has been requested for this plane
    return 1;
  } else if (buf->IsNV12Format()) {
    // NV12 formats use a plane scaler even when 1:1
    return 1;
  } else {
    return 0;
  }
}

#if 0
void DrmShimChecks::CheckIoctlI915SetPlane180Rotation(struct drm_i915_plane_180_rotation* rot)
{
    if (rot->obj_type == DRM_MODE_OBJECT_PLANE || rot->obj_type == DRM_MODE_OBJECT_CRTC)
    {
        HWCVAL_LOCK(_l,mMutex);
        mWorkQueue.Process();

        DrmShimPlane* plane = mPlanes.valueFor(rot->obj_id);

        if (plane)
        {
	    uint32_t hwTransform = rot->rotate ? hwcomposer::HWCTransform::kTransform180 : Hhwcomposer::HWCTransform::kIdentity;
            plane->SetHwTransform(hwTransform);
            HWCLOGD("Performing transform %s on plane %d", DrmShimTransform::GetTransformName(hwTransform), rot->obj_id);

            // Redraw is necessary after hardware transform
            plane->SetRedrawExpected(true);

            // Flicker detection
            DrmShimCrtc* crtc = static_cast<DrmShimCrtc*>(plane->GetCrtc());

            if (crtc)
            {
                crtc->SetDrmFrame();
            }
        }
        else
        {
            HWCERROR(eCheckIoctlParameters, "SetPlane180Rotation: plane %d unknown", rot->obj_id);
        }
        HWCCHECK(eCheckIoctlParameters);
    }
}
#endif

uint32_t DrmShimChecks::GetCrtcIdForConnector(uint32_t conn_id) {
  DrmShimCrtc* crtc = mConnectors.at(conn_id).mCrtc;

  if (crtc) {
    return crtc->GetCrtcId();
  } else {
    return 0;
  }
}

void DrmShimChecks::CheckSetDPMS(uint32_t conn_id, uint64_t value,
                                 HwcTestEventHandler* eventHandler,
                                 HwcTestCrtc*& theCrtc, bool& reenable) {
  PushThreadState ts("CheckSetDPMS (locking)");
  HWCVAL_LOCK(_l, mMutex);
  SetThreadState("CheckSetDPMS (locked)");
  mWorkQueue.Process();

  DrmShimCrtc* crtc = mConnectors.at(conn_id).mCrtc;
  theCrtc = crtc;
  uint32_t crtcId = (crtc ? crtc->GetCrtcId() : 0);
  HWCLOGD("CheckSetDPMS conn_id=%d crtc %d value=%d", conn_id, crtcId, value);

  if (crtc) {
    // Panel was turned on or off
    if (value == DRM_MODE_DPMS_OFF) {
      crtc->SetDPMSEnabled(false);
      mCrcReader.SuspendCRCs(crtc->GetCrtcId(),
                             HwcCrcReader::CRC_SUSPEND_BLANKING, true);
    }
#ifdef DRM_MODE_DPMS_ASYNC_OFF
    else if (value == DRM_MODE_DPMS_ASYNC_OFF) {
      crtc->SetDPMSEnabled(false);  // TODO: how is async off different?
      // though hopefully it doesn't matter given the 4 frame window
      mCrcReader.SuspendCRCs(crtc->GetCrtcId(),
                             HwcCrcReader::CRC_SUSPEND_BLANKING, true);
    }
#endif
    else if (value == DRM_MODE_DPMS_ON) {
      crtc->SetDPMSEnabled(true);
      mCrcReader.SuspendCRCs(crtc->GetCrtcId(),
                             HwcCrcReader::CRC_SUSPEND_BLANKING, false);
    }
#ifdef DRM_MODE_DPMS_ASYNC_ON
    else if (value == DRM_MODE_DPMS_ASYNC_ON) {
      crtc->SetDPMSEnabled(true);
      mCrcReader.SuspendCRCs(crtc->GetCrtcId(),
                             HwcCrcReader::CRC_SUSPEND_BLANKING, false);
    }
#endif

    if ((crtcId > 0) && (eventHandler != 0)) {
      eventHandler->CancelEvent(crtcId);

      if (value == DRM_MODE_DPMS_ON
#ifdef DRM_MODE_DPMS_ASYNC_ON
          || (value == DRM_MODE_DPMS_ASYNC_ON)
#endif
          ) {
        if (crtc) {
          bool reenableVBlank = crtc->IsModeSet();
          reenable = reenableVBlank;
        }
      } else {
        if (crtc) {
          crtc->SetModeSet(false);
        }
      }
    }

    DoStall(Hwcval::eStallDPMS, &mMutex);
    crtc->SetDPMSInProgress(true);
  } else {
    HWCLOGW("DPMS Enable/disable for unknown connector %d", conn_id);
  }
}

void DrmShimChecks::CheckSetDPMSExit(uint32_t fd, HwcTestCrtc* crtc,
                                     bool reenable,
                                     HwcTestEventHandler* eventHandler,
                                     uint32_t status) {
  HWCVAL_UNUSED(status);
  HWCLOGD("CheckSetDPMSExit crtc %d status=%d", crtc->GetCrtcId(), status);

  PushThreadState ts("CheckSetDPMSExit (locking)");
  HWCVAL_LOCK(_l, mMutex);
  SetThreadState("CheckSetDPMSExit (locked)");
  mWorkQueue.Process();

  if (crtc) {
    crtc->SetDPMSInProgress(false);

    if (reenable && eventHandler) {
      eventHandler->CaptureVBlank(fd, crtc->GetCrtcId());
    }
  }
}

void DrmShimChecks::CheckSetPanelFitter(uint32_t conn_id, uint64_t value) {
  HWCLOGD("CheckSetPanelFitter conn_id=%d value=%d", conn_id, value);

  PushThreadState ts("CheckSetPanelFitter (locking)");
  HWCVAL_LOCK(_l, mMutex);
  SetThreadState("CheckSetPanelFitter (locked)");
  mWorkQueue.Process();

  DrmShimCrtc* crtc = mConnectors.at(conn_id).mCrtc;

  if (crtc) {
    crtc->SetPanelFitter(value);
  } else {
    HWCLOGW("SetPanelFitter for unknown connector %d", conn_id);
  }
}

void DrmShimChecks::CheckSetPanelFitterSourceSize(uint32_t conn_id, uint32_t sw,
                                                  uint32_t sh) {
  HWCLOGD("CheckSetPanelFitterSourceSize conn_id=%d sw=%d, sh=%d", conn_id, sw,
          sh);

  PushThreadState ts("CheckPanelFitterSourceSize (locking)");
  HWCVAL_LOCK(_l, mMutex);
  SetThreadState("CheckPanelFitterSourceSize (locked)");
  mWorkQueue.Process();

  DrmShimCrtc* crtc = mConnectors.at(conn_id).mCrtc;

  if (crtc) {
    crtc->SetPanelFitterSourceSize(sw, sh);
  } else {
    // TODO: understand why this happens sometimes.
    // Otherwise this really should be HWCERROR(eCheckIoctlParameters,...
    HWCLOGW("SetPanelFitterSourceSize for unknown connector %d", conn_id);
  }
}

std::shared_ptr<DrmShimBuffer> DrmShimChecks::UpdateBufferPlane(
    uint32_t fbId, DrmShimCrtc* crtc, DrmShimPlane* plane) {
  mWorkQueue.Process();

  std::shared_ptr<DrmShimBuffer> buf;
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  plane->SetCurrentDsId((int64_t)fbId);

  if (mBuffersByFbId.find(fbId) != mBuffersByFbId.end()) {
    // Buffer already known to us
    // Just record that we want it drawn, but it isn't new
    buf = mBuffersByFbId[fbId];
    ALOG_ASSERT(buf.get());

    DrmShimBuffer::FbIdData* pFbIdData = buf->GetFbIdData(fbId);
    uint32_t pixelFormat = pFbIdData ? pFbIdData->pixelFormat : 0;
    bool hasAuxBuffer = pFbIdData ? pFbIdData->hasAuxBuffer : false;
    uint32_t auxPitch = pFbIdData ? pFbIdData->auxPitch : 0;
    uint32_t auxOffset = pFbIdData ? pFbIdData->auxOffset : 0;
    __u64 modifier = pFbIdData ? pFbIdData->modifier : 0;

    plane->SetPixelFormat(pixelFormat);
    plane->SetHasAuxBuffer(hasAuxBuffer);
    plane->SetAuxPitch(auxPitch);
    plane->SetAuxOffset(auxOffset);
    plane->SetTilingFromModifier(modifier);

    if (hasAuxBuffer) {
      HWCLOGD_COND(eLogBuffer,
                   "UpdateBufferPlane %s CRTC %d plane %d pixelFormat %d (Aux "
                   "buffer - pitch %d offset %d modifier %llu)",
                   buf->IdStr(strbuf), crtc ? crtc->GetCrtcId() : 0,
                   plane ? plane->GetPlaneId() : 0, pixelFormat, auxPitch,
                   auxOffset, modifier);
    } else {
      HWCLOGD_COND(eLogBuffer,
                   "UpdateBufferPlane %s CRTC %d plane %d pixelFormat %d",
                   buf->IdStr(strbuf), crtc ? crtc->GetCrtcId() : 0,
                   plane ? plane->GetPlaneId() : 0, pixelFormat);
    }
  } else {
    HWCERROR(eCheckDrmFbId, "FB %d does not map to any open buffer", fbId);
  }
  HWCCHECK(eCheckDrmFbId);

  plane->SetBuf(buf);

  return buf;
}

void DrmShimChecks::ValidateFrame(uint32_t crtcId, uint32_t nextFrame) {
  HWCVAL_LOCK(_l, mMutex);
  mWorkQueue.Process();

  if (mCrtcs.find(crtcId) != mCrtcs.end()) {
    DrmShimCrtc* crtc = mCrtcs[crtcId];
    ValidateFrame(crtc, nextFrame, false);
  } else {
    HWCERROR(eCheckInvalidCrtc, "Unknown CRTC %d", crtcId);
  }
}

void DrmShimChecks::ValidateDrmReleaseTo(uint32_t connectorId) {
  HWCVAL_LOCK(_l, mMutex);
  mWorkQueue.Process();

  if (mConnectors.find(connectorId) != mConnectors.end()) {
    const Connector& conn = mConnectors.at(connectorId);
    DrmShimCrtc* crtc = conn.mCrtc;

    if (crtc) {
      HWCLOGD_COND(eLogParse, "ValidateDrmReleaseTo: connector %d crtc %p",
                   connectorId, crtc);

      if (crtc->IsConnectedDisplay()) {
        ValidateFrame(crtc, -1, true);
      }
    } else {
      HWCLOGD_COND(eLogParse, "ValidateDrmReleaseTo: NO crtc for connector %d",
                   connectorId);
    }
  } else {
    HWCLOGD_COND(eLogParse, "ValidateDrmReleaseTo: Connector %d does not exist",
                 connectorId);
  }
}

void DrmShimChecks::ValidateFrame(DrmShimCrtc* crtc, uint32_t nextFrame,
                                  bool drop) {
  uint32_t disp = crtc->GetDisplayIx();
  if (disp == eNoDisplayIx) {
    // TODO use appropriate display index. Cached from before disconnect?
    HWCLOGD("CRTC %d disconnected from SF, skipping validation",
            crtc->GetCrtcId());
    return;
  }

  HWCLOGD_COND(eLogParse,
               "DrmShimChecks::ValidateFrame Validate crtc %d@%p displayIx %d "
               "nextFrame %d drop %d",
               crtc->GetCrtcId(), crtc, disp, nextFrame, drop);

  int currentFrame = mCurrentFrame[disp];
  mCurrentFrame[disp] = nextFrame;

  if (currentFrame > 0) {
    mLLQ[disp].LogQueue();
    HWCLOGD_COND(
        eLogParse,
        "DrmShimChecks::ValidateFrame Getting disp %d frame:%d from LLQ", disp,
        currentFrame);

    uint32_t srcDisp = crtc->GetSfSrcDisplayIx();

    // Don't generate "previous frame's fence was not signalled" errors if:
    // 1. We have some mosaic display stuff going on as the fence can't be
    // signalled twice! OR
    // 2. we are suspending and hence are putting a blanking frame on the
    // display.
    bool expectPrevFrameSignalled =
        (!crtc->IsMappedFromOtherDisplay()) && (nextFrame != 0);
    Hwcval::LayerList* ll =
        mLLQ[srcDisp].GetFrame(currentFrame, expectPrevFrameSignalled);

    if (crtc->DidSetDisplayFail()) {
      // Don't validate if the SetDisplay failed as (a) we have already logged
      // that error, (b) the Retire Fence will already have been signalled
      // and we don't want to generate an error to that effect.
      HWCLOGI(
          "DrmShimChecks::ValidateFrame DidSetDisplayFail on CRTC %d failed, "
          "skip validation",
          crtc->GetCrtcId());
      return;
    }

    if (ll) {
      if (crtc->IsExternalDisplay()) {
        SetExtendedModeExpectation(ll->GetVideoFlags().mSingleFullScreenVideo,
                                   true, currentFrame);
      }

      if (nextFrame > 0) {
        HWCLOGD_COND(eLogParse, "DrmShimChecks::ValidateFrame CRTC %d frame:%d",
                     crtc->GetCrtcId(), currentFrame);
        crtc->Checks(ll, this, currentFrame);
      } else {
        // Display was turned off, so buffers may have been overwritten, so we
        // can't validate.
      }
    } else {
      HWCLOGW("ValidateFrame CRTC %d NO FRAME %d", crtc->GetCrtcId(),
              currentFrame);
    }
  }

  crtc->PageFlipsSinceDPMS();
}

void DrmShimChecks::ValidateEsdRecovery(uint32_t d) {
  HWCVAL_LOCK(_l, mMutex);
  mWorkQueue.Process();

  DrmShimCrtc* crtc = GetCrtcByDisplayIx(d);

  HWCLOGD_COND(eLogParse, "PARSED MATCHED {ESD%d}", d);
  if (crtc) {
    crtc->EsdStateTransition(HwcTestCrtc::eEsdAny, HwcTestCrtc::eEsdStarted);
  }
}

void DrmShimChecks::ValidateDisplayMapping(uint32_t connId, uint32_t crtcId) {
  // Now we need to work out the display index based on (a) what is already
  // connected and
  // (b) whether the connector is fixed or removable.

  // Is display index 0 already in use?
  // We can't do this from using mCrtcByDisplayIx because this may not be
  // assigned yet.
  uint32_t crtcIdByDisplayIx[HWCVAL_MAX_CRTCS];
  memset(crtcIdByDisplayIx, 0, sizeof(crtcIdByDisplayIx));

  for (std::map<uint32_t, Connector>::iterator connItr = mConnectors.begin(); connItr != mConnectors.end() ; ++connItr) {
    Connector& conn = connItr->second;
    uint32_t id = connItr->first;

    if (id == connId) {
      // We don't have to block this from reuse, it's the one we are
      // reallocating
    } else if (conn.mDisplayIx != eNoDisplayIx) {
      if (conn.mCrtc) {
        crtcIdByDisplayIx[conn.mDisplayIx] = conn.mCrtc->GetCrtcId();
      } else {
        // Just say that the connector is in use
        crtcIdByDisplayIx[conn.mDisplayIx] = 0xffffffff;
      }
    }
  }

  uint32_t displayIx = 0;
  if (crtcIdByDisplayIx[0]) {
    if (crtcIdByDisplayIx[0] == crtcId) {
      HWCLOGD_COND(
          eLogHotPlug,
          "New Connection: Connector %d CRTC %d already associated with D0",
          connId, crtcId);
      return;
    }

    if (crtcIdByDisplayIx[1]) {
      if (crtcIdByDisplayIx[1] == crtcId) {
        HWCLOGD_COND(
            eLogHotPlug,
            "New Connection: Connector %d CRTC %d already associated with D1",
            connId, crtcId);
      } else {
        HWCLOGW(
            "New Connection: Connector %d CRTC %d can't be used because D0 and "
            "D1 already associated",
            connId, crtcId);
      }

      return;
    }

    // We can associate with display index 1
    displayIx = 1;
  }

  MapDisplay(displayIx, connId, crtcId);
}

void DrmShimChecks::ValidateDisplayUnmapping(uint32_t crtcId) {
  if (mCrtcs.find(crtcId) == mCrtcs.end()) {
    HWCLOGW("Reset Connection: CRTC %d not found", crtcId);
    return;
  }

  DrmShimCrtc* crtc = mCrtcs[crtcId];
  ALOG_ASSERT(crtc->GetCrtcId() == crtcId);

  uint32_t dix = crtc->GetDisplayIx();
  if (dix != eNoDisplayIx) {
    mCrtcByDisplayIx[crtc->GetDisplayIx()] = 0;
    crtc->SetDisplayIx(eNoDisplayIx);
  }

  for (std::map<uint32_t, Connector>::iterator connItr = mConnectors.begin(); connItr != mConnectors.end() ; ++connItr) {
    Connector& conn = connItr->second;
    if (conn.mCrtc == crtc) {
      conn.mDisplayIx = eNoDisplayIx;
      conn.mCrtc = 0;
    }
  }
}

// Display property query
// DO NOT CALL from locked code
uint32_t DrmShimChecks::GetDisplayProperty(
    uint32_t displayIx, HwcTestState::DisplayPropertyType prop) {
  HWCVAL_LOCK(_l, mMutex);
  mWorkQueue.Process();

  DrmShimCrtc* crtc = GetCrtcByDisplayIx(displayIx);
  if (crtc == 0) {
    return 0;
  }

  switch (prop) {
    case HwcTestState::ePropConnectorId: {
      return crtc->GetConnector();
    }
    default: {
      // Test has requested an invalid property
      ALOG_ASSERT(0);
      return 0;
    }
  }
}

/// Move device-specific ids from old to new buffer
void DrmShimChecks::MoveDsIds(std::shared_ptr<DrmShimBuffer> existingBuf,
                              std::shared_ptr<DrmShimBuffer> buf) {
  DrmShimBuffer::FbIdVector& fbIds = existingBuf->GetFbIds();
  buf->GetFbIds() = fbIds;

  for (std::map<uint32_t, DrmShimBuffer::FbIdData>::iterator itr = fbIds.begin(); itr !=  fbIds.end(); ++itr) {
    mBuffersByFbId[itr->first] = buf;
  }
}

DrmShimCrtc* DrmShimChecks::GetCrtcByDisplayIx(uint32_t displayIx) {
  return static_cast<DrmShimCrtc*>(GetHwcTestCrtcByDisplayIx(displayIx));
}

DrmShimCrtc* DrmShimChecks::GetCrtcByPipe(uint32_t pipe) {
  return mCrtcByPipe[pipe];
}

void DrmShimChecks::MarkEsdRecoveryStart(uint32_t connectorId) {
  
  if (mConnectors.find(connectorId) != mConnectors.end()) {
    const Connector& conn = mConnectors.at(connectorId);
    DrmShimCrtc* crtc = conn.mCrtc;
    if (crtc) {
      crtc->MarkEsdRecoveryStart();
    }
  }
}

/// Set reference to the DRM property manager
void DrmShimChecks::SetPropertyManager(Hwcval::PropertyManager& propMgr) {
  mPropMgr = &propMgr;
  propMgr.SetTestKernel(this);
}

HwcTestKernel::ObjectClass DrmShimChecks::GetObjectClass(uint32_t objId) {

  if (mPlanes.find(objId) == mPlanes.end()) {

    if (mCrtcs.find(objId) == mCrtcs.end()) {
      HWCLOGV_COND(eLogNuclear,
                   "Object %d not found out of %d planes and %d crtcs", objId,
                   mPlanes.size(), mCrtcs.size());
      return HwcTestKernel::eOther;
    } else {
      return HwcTestKernel::eCrtc;
    }
  } else {
    return HwcTestKernel::ePlane;
  }
}

DrmShimPlane* DrmShimChecks::GetDrmPlane(uint32_t drmPlaneId) {
  if (mPlanes.find(drmPlaneId) != mPlanes.end()) {
    DrmShimPlane* plane = mPlanes[drmPlaneId];
    return plane;
  }

  return 0;
}

bool DrmShimChecks::SimulateHotPlug(uint32_t displayTypes, bool connected) {
  PushThreadState ts("DrmShimChecks::SimulateHotPlug");
  bool done = false;

  for (uint32_t i = 0; i < HWCVAL_MAX_PIPES; ++i) {
    HwcTestCrtc* crtc = mCrtcByPipe[i];

    if (crtc) {
      if (crtc->IsHotPluggable()) {
        // Allow filtering on the real display type
        // This makes sense only if we have eOptSpoofNoPanel set.
        // In this instance, the caller can for instance pass eFixed
        // for the displayTypes parameter and then only the panel would
        // be hot plugged/unplugged.
        if ((crtc->GetRealDisplayType() & displayTypes) != 0) {
          done |= crtc->SimulateHotPlug(connected);
        }
      }
    }
  }

  return done;
}

bool DrmShimChecks::IsHotPluggableDisplayAvailable() {
  if (!mState->GetNewDisplayConnectionState()) {
    return false;
  }

  for (uint32_t i = 0; i < HWCVAL_MAX_PIPES; ++i) {
    HwcTestCrtc* crtc = mCrtcByPipe[i];

    if (crtc) {
      if (crtc->IsHotPluggable()) {
        return true;
      }
    }
  }

  return false;
}

void DrmShimChecks::CheckSetDDRFreq(uint64_t value) {
  // 0 - normal
  // 1 - low freq
  HWCLOGD("DDR Frequency set to %s", value ? "LOW" : "NORMAL");
  mDDRMode = value;
}

bool DrmShimChecks::IsDDRFreqSupported() {

  for (std::map<uint32_t, Connector>::iterator connItr = mConnectors.begin(); connItr != mConnectors.end() ; ++connItr) {
    Connector& conn = connItr->second;

    if (conn.mAttributes & eDDRFreq) {
      return true;
    }
  }

  return false;
}

bool DrmShimChecks::IsDRRSEnabled(uint32_t connId) {

  if (mConnectors.find(connId) != mConnectors.end()) {
    const Connector& conn = mConnectors.at(connId);

    return ((conn.mAttributes & eDRRS) != 0);
  } else {
    HWCLOGD("IsDRRSEnabled: connector %d not found", connId);
    return false;
  }
}

Hwcval::LogChecker* DrmShimChecks::GetParser() {
  return &mDrmParser;
}
