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

#include "HwcService.h"
#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include "drmhwctwo.h"

#define HWC_VERSION_STRING                                             \
  "VERSION:HWC 2.0 GIT Branch & Latest Commit:" HWC_VERSION_GIT_BRANCH \
  " " HWC_VERSION_GIT_SHA " " __DATE__ " " __TIME__

namespace android {
using namespace hwcomposer;

HwcService::HwcService() : mpHwc(NULL) {
}

HwcService::~HwcService() {
}

bool HwcService::start(DrmHwcTwo &hwc) {
  mpHwc = &hwc;
  sp<IServiceManager> sm(defaultServiceManager());
  if (sm->addService(String16(IA_HWC_SERVICE_NAME), this, false)) {
    ALOGE("Failed to start %s service", IA_HWC_SERVICE_NAME);
    return false;
  }
  return true;
}

String8 HwcService::getHwcVersion() {
  return String8((HWC_VERSION_STRING));
}

status_t HwcService::setOption(String8 option, String8 value) {
  return OK;
}

void HwcService::dumpOptions(void) {
  // ALOGD("%s", OptionManager::getInstance().dump().string());
}

status_t HwcService::enableLogviewToLogcat(bool enable) {
  // TO DO
  return OK;
}

sp<IDiagnostic> HwcService::getDiagnostic() {
  // if (sbInternalBuild || sbLogViewerBuild)
  {
    Mutex::Autolock _l(mLock);
    ALOG_ASSERT(mpHwc);
    if (mpDiagnostic == NULL)
      mpDiagnostic = new Diagnostic(*mpHwc);
  }
  return mpDiagnostic;
}

sp<IControls> HwcService::getControls() {
  Mutex::Autolock _l(mLock);
  ALOG_ASSERT(mpHwc);
  return new Controls(*mpHwc, *this);
}

status_t HwcService::Diagnostic::readLogParcel(Parcel *parcel) {
  // TO DO
  return OK;
}

#if INTEL_HWC_INTERNAL_BUILD
void HwcService::Diagnostic::enableDisplay(uint32_t d)
{
    if (sbInternalBuild)
        DebugFilter::get().enableDisplay(d);
}

void HwcService::Diagnostic::disableDisplay(uint32_t d, bool bBlank)
{
    if (sbInternalBuild)
        DebugFilter::get().disableDisplay(d, bBlank);
}

void HwcService::Diagnostic::maskLayer(uint32_t d, uint32_t layer, bool bHide)
{
    if (sbInternalBuild)
        DebugFilter::get().maskLayer(d, layer, bHide);
}

void HwcService::Diagnostic::dumpFrames(uint32_t d, int32_t frames, bool bSync)
{
    if (sbInternalBuild)
    {
        DebugFilter::get().dumpFrames(d, frames);
        if ( bSync )
        {
            mHwc.synchronize();
        }
    }
}

#else
void HwcService::Diagnostic::enableDisplay(uint32_t)                { /* nothing */ }
void HwcService::Diagnostic::disableDisplay(uint32_t, bool)         { /* nothing */ }
void HwcService::Diagnostic::maskLayer(uint32_t, uint32_t, bool)    { /* nothing */ }
void HwcService::Diagnostic::dumpFrames(uint32_t, int32_t, bool)    { /* nothing */ }
#endif

HwcService::Controls::Controls(DrmHwcTwo &hwc, HwcService& hwcService) :
    mHwc(hwc),
    mHwcService(hwcService),
    mbHaveSessionsEnabled(false),
    mCurrentOptimizationMode(HWCS_OPTIMIZE_NORMAL)
{
}

HwcService::Controls::~Controls()
{
    if (mbHaveSessionsEnabled)
    {
        //TO DO 
        //videoDisableAllEncryptedSessions();
    }

    if (mCurrentOptimizationMode != HWCS_OPTIMIZE_NORMAL)
    {
        //TO DO 
        // Reset mode back to normal if needed
        //videoSetOptimizationMode(HWCS_OPTIMIZE_NORMAL);
    }
}

#define HWCS_ENTRY_FMT(fname, fmt, ...) \
    const char* ___HWCS_FUNCTION = fname; \
    Log::add(fname " " fmt " -->", __VA_ARGS__)

#define HWCS_ENTRY(fname)  \
    const char* ___HWCS_FUNCTION = fname; \
    Log::add(fname " -->")

#define HWCS_ERROR(code) \
    Log::add("%s ERROR %d <--", ___HWCS_FUNCTION, code)

#define HWCS_EXIT_ERROR(code) do { \
    int ___code = code; \
    HWCS_ERROR(___code); \
    return ___code; } while(0)

#define HWCS_OK_FMT(fmt, ...) \
    Log::add("%s OK " fmt " <--", ___HWCS_FUNCTION, __VA_ARGS__);

#define HWCS_EXIT_OK_FMT(fmt, ...) do { \
    HWCS_OK_FMT(fmt, __VA_ARGS__); \
    return OK; } while(0)

#define HWCS_EXIT_OK() do { \
    Log::add("%s OK <--", ___HWCS_FUNCTION); \
    return OK; } while(0)

#define HWCS_EXIT_VAR(code) do { \
    int ____code = code; \
    if (____code == OK) HWCS_EXIT_OK(); \
    HWCS_EXIT_ERROR(____code); \
    } while(0)

#define HWCS_EXIT_VAR_FMT(code, fmt, ...) do { \
    int ____code = code; \
    if (____code == OK) HWCS_EXIT_OK_FMT(fmt, __VA_ARGS__); \
    HWCS_EXIT_ERROR(____code); \
    } while(0)
status_t HwcService::Controls::displaySetOverscan(uint32_t display, int32_t xoverscan, int32_t yoverscan)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displayGetOverscan(uint32_t display, int32_t *xoverscan, int32_t *yoverscan)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displaySetScaling(uint32_t display, EHwcsScalingMode eScalingMode)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displayGetScaling(uint32_t display, EHwcsScalingMode *peScalingMode)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displayEnableBlank(uint32_t display, bool blank)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displayRestoreDefaultColorParam(uint32_t display, EHwcsColorControl color)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displayGetColorParam(uint32_t display, EHwcsColorControl color, float *, float *, float *)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displaySetColorParam(uint32_t display, EHwcsColorControl color, float value)
{
  //TO DO
  return OK;
}
Vector<HwcsDisplayModeInfo> HwcService::Controls::displayModeGetAvailableModes(uint32_t display)
{
  //TO DO
    Vector<HwcsDisplayModeInfo> modes;

    return modes;
}

status_t HwcService::Controls::displayModeGetMode(uint32_t display, HwcsDisplayModeInfo *pMode)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::displayModeSetMode(uint32_t display, const HwcsDisplayModeInfo *pMode)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::videoEnableEncryptedSession( uint32_t sessionID, uint32_t instanceID )
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::videoDisableEncryptedSession( uint32_t sessionID )
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::videoDisableAllEncryptedSessions( )
{
  //TO DO
  return OK;
}

bool HwcService::Controls::videoIsEncryptedSessionEnabled( uint32_t sessionID, uint32_t instanceID )
{
  //TO DO
  return OK;
}

bool HwcService::Controls::needSetKeyFrameHint()
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::videoSetOptimizationMode( EHwcsOptimizationMode mode )
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::mdsUpdateVideoState(int64_t videoSessionID, bool isPrepared)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::mdsUpdateVideoFPS(int64_t videoSessionID, int32_t fps)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::mdsUpdateInputState(bool state)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::widiGetSingleDisplay(bool *pEnabled)
{
  //TO DO
  return OK;
}

status_t HwcService::Controls::widiSetSingleDisplay(bool enable) {
  // TO DO
  return OK;
}
void HwcService::registerListener(ENotification notify,
                                  NotifyCallback *pCallback) {
  // TO DO
}

void HwcService::unregisterListener(ENotification notify,
                                    NotifyCallback *pCallback) {
  // TO DO
}

void HwcService::notify(ENotification notify, int32_t paraCnt, int64_t para[]) {
  // TO DO
}

}  //  android
