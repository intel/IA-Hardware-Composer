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

#include "hwcservice.h"
#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include "iahwc2.h"

#define HWC_VERSION_STRING                                             \
  "VERSION:HWC 2.0 GIT Branch & Latest Commit:" HWC_VERSION_GIT_BRANCH \
  " " HWC_VERSION_GIT_SHA " " __DATE__ " " __TIME__

namespace android {
using namespace hwcomposer;

HwcService::HwcService() : mpHwc(NULL) {
}

HwcService::~HwcService() {
}

bool HwcService::Start(IAHWC2 &hwc) {
  mpHwc = &hwc;
  sp<IServiceManager> sm(defaultServiceManager());
  if (sm->addService(String16(IA_HWC_SERVICE_NAME), this, false)) {
    ALOGE("Failed to start %s service", IA_HWC_SERVICE_NAME);
    return false;
  }
    ALOGE("started %s service", IA_HWC_SERVICE_NAME);
  return true;
}

String8 HwcService::GetHwcVersion() {
  return String8((HWC_VERSION_STRING));
}

status_t HwcService::SetOption(String8 option, String8 value) {
  return OK;
}

void HwcService::DumpOptions(void) {
  // ALOGD("%s", OptionManager::getInstance().dump().string());
}

status_t HwcService::EnableLogviewToLogcat(bool enable) {
  // TO DO
  return OK;
}

sp<IDiagnostic> HwcService::GetDiagnostic() {
  // if (sbInternalBuild || sbLogViewerBuild)
  {
    lock_.lock();
    ALOG_ASSERT(mpHwc);
    if (mpDiagnostic == NULL)
      mpDiagnostic = new Diagnostic(*mpHwc);

    lock_.unlock();
  }

  return mpDiagnostic;
}

sp<IControls> HwcService::GetControls() {
  // TODO: Check the need for lock
  ALOG_ASSERT(mpHwc);
  return new Controls(*mpHwc, *this);
}

status_t HwcService::Diagnostic::ReadLogParcel(Parcel *parcel) {
  // TO DO
  return OK;
}

void HwcService::Diagnostic::EnableDisplay(uint32_t) { /* nothing */
}
void HwcService::Diagnostic::DisableDisplay(uint32_t, bool) { /* nothing */
}
void HwcService::Diagnostic::MaskLayer(uint32_t, uint32_t, bool) { /* nothing */
}
void HwcService::Diagnostic::DumpFrames(uint32_t, int32_t, bool) { /* nothing */
}

HwcService::Controls::Controls(IAHWC2 &hwc, HwcService &hwcService)
    : mHwc(hwc),
      mHwcService(hwcService),
      mbHaveSessionsEnabled(false),
      mCurrentOptimizationMode(HWCS_OPTIMIZE_NORMAL) {
}

HwcService::Controls::~Controls() {
}

#define HWCS_ENTRY_FMT(fname, fmt, ...) \
  const char *___HWCS_FUNCTION = fname; \
  Log::add(fname " " fmt " -->", __VA_ARGS__)

#define HWCS_ENTRY(fname)               \
  const char *___HWCS_FUNCTION = fname; \
  Log::add(fname " -->")

#define HWCS_ERROR(code) Log::add("%s ERROR %d <--", ___HWCS_FUNCTION, code)

#define HWCS_EXIT_ERROR(code) \
  do {                        \
    int ___code = code;       \
    HWCS_ERROR(___code);      \
    return ___code;           \
  } while (0)

#define HWCS_OK_FMT(fmt, ...) \
  Log::add("%s OK " fmt " <--", ___HWCS_FUNCTION, __VA_ARGS__);

#define HWCS_EXIT_OK_FMT(fmt, ...) \
  do {                             \
    HWCS_OK_FMT(fmt, __VA_ARGS__); \
    return OK;                     \
  } while (0)

#define HWCS_EXIT_OK()                       \
  do {                                       \
    Log::add("%s OK <--", ___HWCS_FUNCTION); \
    return OK;                               \
  } while (0)

#define HWCS_EXIT_VAR(code)    \
  do {                         \
    int ____code = code;       \
    if (____code == OK)        \
      HWCS_EXIT_OK();          \
    HWCS_EXIT_ERROR(____code); \
  } while (0)

#define HWCS_EXIT_VAR_FMT(code, fmt, ...) \
  do {                                    \
    int ____code = code;                  \
    if (____code == OK)                   \
      HWCS_EXIT_OK_FMT(fmt, __VA_ARGS__); \
    HWCS_EXIT_ERROR(____code);            \
  } while (0)
status_t HwcService::Controls::DisplaySetOverscan(uint32_t display,
                                                  int32_t xoverscan,
                                                  int32_t yoverscan) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplayGetOverscan(uint32_t display,
                                                  int32_t *xoverscan,
                                                  int32_t *yoverscan) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplaySetScaling(
    uint32_t display, EHwcsScalingMode eScalingMode) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplayGetScaling(
    uint32_t display, EHwcsScalingMode *peScalingMode) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplayEnableBlank(uint32_t display,
                                                  bool blank) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplayRestoreDefaultColorParam(
    uint32_t display, EHwcsColorControl color) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplayGetColorParam(uint32_t display,
                                                    EHwcsColorControl color,
                                                    float *, float *, float *) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplaySetColorParam(uint32_t display,
                                                    EHwcsColorControl color,
                                                    float value) {
  // TO DO
  return OK;
}
Vector<HwcsDisplayModeInfo> HwcService::Controls::DisplayModeGetAvailableModes(
    uint32_t display) {
  // TO DO
  Vector<HwcsDisplayModeInfo> modes;

  return modes;
}

status_t HwcService::Controls::DisplayModeGetMode(uint32_t display,
                                                  HwcsDisplayModeInfo *pMode) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::DisplayModeSetMode(
    uint32_t display, const HwcsDisplayModeInfo *pMode) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::VideoEnableEncryptedSession(
    uint32_t sessionID, uint32_t instanceID) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::VideoDisableAllEncryptedSessions(
    uint32_t sessionID) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::VideoDisableAllEncryptedSessions() {
  // TO DO
  return OK;
}

bool HwcService::Controls::VideoIsEncryptedSessionEnabled(uint32_t sessionID,
                                                          uint32_t instanceID) {
  // TO DO
  return OK;
}

bool HwcService::Controls::needSetKeyFrameHint() {
  // TO DO
  return OK;
}

status_t HwcService::Controls::VideoSetOptimizationMode(
    EHwcsOptimizationMode mode) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::MdsUpdateVideoState(int64_t videoSessionID,
                                                   bool isPrepared) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::MdsUpdateVideoFPS(int64_t videoSessionID,
                                                 int32_t fps) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::MdsUpdateInputState(bool state) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::WidiGetSingleDisplay(bool *pEnabled) {
  // TO DO
  return OK;
}

status_t HwcService::Controls::WidiSetSingleDisplay(bool enable) {
  // TO DO
  return OK;
}
void HwcService::RegisterListener(ENotification notify,
                                  NotifyCallback *pCallback) {
  // TO DO
}

void HwcService::UnregisterListener(ENotification notify,
                                    NotifyCallback *pCallback) {
  // TO DO
}

void HwcService::Notify(ENotification notify, int32_t paraCnt, int64_t para[]) {
  // TO DO
}

}  //  android
