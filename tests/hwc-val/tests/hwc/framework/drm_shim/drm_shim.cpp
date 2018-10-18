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

/// \futurework
/// @{
///  Clean up pass through. Only have if (pass through) if the functions can be
///  completely handle by drm.
/// @}

#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>

extern "C" {
#include "drm_shim.h"
}

#include "DrmShimChecks.h"
#include "DrmShimEventHandler.h"
#include "DrmShimPropertyManager.h"
#include "DrmShimCallbackBase.h"
#include "HwcTestDefs.h"
#include "HwcTestState.h"
#include "HwcTestDebug.h"
#include "HwcvalThreadTable.h"
#include "DrmShimCrtc.h"

#include <utils/Mutex.h>

#include "i915_drm.h"

#undef LOG_TAG
#define LOG_TAG "DRM_SHIM"

#ifdef DRM_CALL_LOGGING

#define FUNCENTER(F) HWCLOGD("Enter " #F);
#define FUNCEXIT(F) HWCLOGD("Exit " #F);

#define WRAPFUNC(F, ARGS)      \
  {                            \
    HWCVAL_LOCK(_l, drmMutex); \
    HWCLOGD("Enter " #F);      \
    F ARGS;                    \
    HWCLOGD("Exit " #F);       \
  }

#define WRAPFUNCRET(TYPE, F, ARGS) \
  {                                \
    TYPE ret;                      \
    WRAPFUNC(ret = F, ARGS);       \
    return ret;                    \
  }

#else

#define FUNCENTER(F)
#define FUNCEXIT(F)
#define WRAPFUNC(F, ARGS) F ARGS
#define WRAPFUNCRET(TYPE, F, ARGS) return F ARGS

#endif  // DRM_CALL_LOGGING

using namespace Hwcval;

// Checking object for DRM
DrmShimChecks *checks = 0;

// Test kernel. Used for ADF, even when DRM validation is not enabled.
HwcTestKernel *testKernel = 0;

std::unique_ptr<DrmShimEventHandler> eventHandler = 0;
extern DrmShimCallbackBase *drmShimCallback;
static DrmShimPropertyManager propMgr;
static bool bUniversalPlanes = false;

static volatile uint32_t libraryIsInitialized = 0;

// Local functions
static int TimeIoctl(int fd, unsigned long request, void *arg,
                     int64_t &durationNs);
static void IoctlLatencyCheck(unsigned long request, int64_t durationNs);

// Handles for real libraries
void *libDrmHandle;
void *libDrmIntelHandle;
char *libError;

const char *cLibDrmRealPath = HWCVAL_LIBPATH "/libdrm.real.so";
const char *cLibDrmRealVendorPath = HWCVAL_VENDOR_LIBPATH "/libdrm.real.so";

// Function pointers to real DRM
//-----------------------------------------------------------------------------
// libdrm function pointers to real drm functions

void (*fpDrmModeFreeResources)(drmModeResPtr ptr);

void (*fpDrmModeFreeCrtc)(drmModeCrtcPtr ptr);

void (*fpDrmModeFreeConnector)(drmModeConnectorPtr ptr);

void (*fpDrmModeFreeEncoder)(drmModeEncoderPtr ptr);

void (*fpDrmModeFreePlane)(drmModePlanePtr ptr);

void (*fpDrmModeFreePlaneResources)(drmModePlaneResPtr ptr);

drmModeResPtr (*fpDrmModeGetResources)(int fd);

int (*fpDrmModeAddFB2)(int fd, uint32_t width, uint32_t height,
                       uint32_t pixel_format, uint32_t bo_handles[4],
                       uint32_t pitches[4], uint32_t offsets[4],
                       uint32_t *buf_id, uint32_t flags);

int (*fpDrmModeRmFB)(int fd, uint32_t bufferId);

drmModeEncoderPtr (*fpDrmModeGetEncoder)(int fd, uint32_t encoder_id);

drmModeConnectorPtr (*fpDrmModeGetConnector)(int fd, uint32_t connectorId);

drmModePropertyPtr (*fpDrmModeGetProperty)(int fd, uint32_t propertyId);

void (*fpDrmModeFreeProperty)(drmModePropertyPtr ptr);

int (*fpDrmModeConnectorSetProperty)(int fd, uint32_t connector_id,
                                     uint32_t property_id, uint64_t value);

drmModePlaneResPtr (*fpDrmModeGetPlaneResources)(int fd);

drmModePlanePtr (*fpDrmModeGetPlane)(int fd, uint32_t plane_id);

drmModeObjectPropertiesPtr (*fpDrmModeObjectGetProperties)(
    int fd, uint32_t object_id, uint32_t object_type);

void (*fpDrmModeFreeObjectProperties)(drmModeObjectPropertiesPtr ptr);

int (*fpDrmModeObjectSetProperty)(int fd, uint32_t object_id,
                                  uint32_t object_type, uint32_t property_id,
                                  uint64_t value);

int (*fpDrmIoctl)(int fd, unsigned long request, void *arg);

int (*fpDrmOpen)(const char *name, const char *busid);

int (*fpDrmClose)(int fd);

drmVersionPtr (*fpDrmGetVersion)(int fd);
drmVersionPtr (*fpDrmGetLibVersion)(int fd);


int (*fpDrmGetCap)(int fd, uint64_t capability, uint64_t *value);

void (*fpDrmFreeVersion)(drmVersionPtr);

int (*fpDrmCommandRead)(int fd, unsigned long drmCommandIndex, void *data,
                        unsigned long size);


int (*fpDrmCommandWrite)(int fd, unsigned long drmCommandIndex, void *data,
                         unsigned long size);

int (*fpDrmCommandWriteRead)(int fd, unsigned long drmCommandIndex, void *data,
                             unsigned long size);

int (*fpDrmWaitVBlank)(int fd, drmVBlankPtr vbl);

void *(*fpDrmMalloc)(int size);

int (*fpDrmPrimeHandleToFD)(int fd, uint32_t handle, uint32_t flags,
                            int *prime_fd);


void *(*fpDrmHashCreate)(void);

int (*fpDrmHashDestroy)(void *t);

int (*fpDrmHashLookup)(void *t, unsigned long key, void **value);

int (*fpDrmHashInsert)(void *t, unsigned long key, void *value);

int (*fpDrmHashDelete)(void *t, unsigned long key);

int (*fpDrmHashFirst)(void *t, unsigned long *key, void **value);

int (*fpDrmHashNext)(void *t, unsigned long *key, void **value);

void (*fpDrmMsg)(const char *format, ...);


int (*fpDrmHandleEvent)(int fd, drmEventContextPtr evctx);

int (*fpDrmPrimeFDToHandle)(int fd, int prime_fd, uint32_t *handle);

int (*fpDrmSetClientCap)(int fd, uint64_t capability, uint64_t value);

int (*fpDrmModeAtomicCommit)(int fd, drmModeAtomicReqPtr req, uint32_t flags,
                             void *user_data);
int (*fpDrmModeAtomicAddProperty)(drmModeAtomicReqPtr req, uint32_t object_id,
                                  uint32_t property_id, uint64_t value);

int (*fpDrmModeCreatePropertyBlob)(int fd, const void *data, size_t size,
                                   uint32_t *id);
int (*fpDrmModeDestroyPropertyBlob)(int fd, uint32_t id);

drmModeAtomicReqPtr (*fpDrmModeAtomicAlloc)(void);

#define CHECK_LIBRARY_INIT       \
  if (libraryIsInitialized == 0) \
    (void) drmShimInit(false, false);

static Hwcval::Mutex drmShimInitMutex;
static Hwcval::Mutex drmMutex;

/// Drm Shim only functions
/// First DRM function call will result in drmShimInit(false, false) in non-HWC
/// process
/// In HWC, sequence should be
///     drmShimInit(true, false)
///     HwcTestStateInit
///     drmShimInit(true, true)
int drmShimInit(bool isHwc, bool isDrm) {
  HWCLOGI("Enter: drmShimInit");
  int result = ercOK;
  HwcTestState *state = 0;

  if (isHwc) {
    state = HwcTestState::getInstance();
    HWCLOGV("drmShimInit: got state %p", state);

    state->SetRunningShim(HwcTestState::eDrmShim);

    testKernel = state->GetTestKernel();

    if (isDrm) {
      checks = static_cast<DrmShimChecks *>(testKernel);
      checks->SetUniversalPlanes(bUniversalPlanes);
      HWCLOGV("drmShimInit: got DRM Checks %p (pid %d)", checks, getpid());
      checks->SetPropertyManager(propMgr);
      return 0;
    }
  }

  if (libraryIsInitialized == 0) {
    dlerror();

    // Open drm library
    HWCLOGI("Doing dlopen for real libDrm in process %d", getpid());
    libDrmHandle = dll_open(cLibDrmRealPath, RTLD_NOW);

    if (!libDrmHandle) {
      dlerror();
      libDrmHandle = dll_open(cLibDrmRealVendorPath, RTLD_NOW);

      if (!libDrmHandle) {
        HWCERROR(eCheckDrmShimBind, "Failed to open real DRM in %s or %s",
                 cLibDrmRealPath, cLibDrmRealVendorPath);
        result = -EFAULT;
        return result;
      }
    }

    libError = (char *)dlerror();

    if (libError != NULL) {
      result |= -EFAULT;
      HWCLOGI("In drmShimInit Error getting libDrmHandle %s", libError);
    }

    // Clear any existing error
    dlerror();

    // Set function pointers to NUll
    fpDrmModeGetPlane = NULL;
    fpDrmModeGetResources = NULL;
    fpDrmModeGetConnector = NULL;
    fpDrmModeFreeConnector = NULL;
    fpDrmModeFreeResources = NULL;
    fpDrmModeGetEncoder = NULL;
    fpDrmModeFreeEncoder = NULL;
    fpDrmModeGetPlaneResources = NULL;
    fpDrmModeFreePlane = NULL;
    fpDrmIoctl = NULL;

    HWCLOGI("About to get function pointers");

    if (result == 0) {
      int err;
#define GET_FUNC_PTR(FN)                                                       \
  err =                                                                        \
      getFunctionPointer(libDrmHandle, "drm" #FN, (void **)&fpDrm##FN, state); \
  if (err) {                                                                   \
    HWCLOGE("Failed to load function drm" #FN);                                \
    result |= err;                                                             \
  } else {                                                                     \
    HWCLOGI("Loaded function drm" #FN);                                        \
  }

      /// Get function pointers functions in real libdrm
      GET_FUNC_PTR(ModeFreeResources)
      GET_FUNC_PTR(ModeFreeCrtc)
      GET_FUNC_PTR(ModeFreeConnector)
      GET_FUNC_PTR(ModeFreeEncoder)
      GET_FUNC_PTR(ModeFreePlane)
      GET_FUNC_PTR(ModeFreePlaneResources)
      GET_FUNC_PTR(ModeGetResources)
      GET_FUNC_PTR(ModeAddFB2)
      GET_FUNC_PTR(ModeRmFB)
      GET_FUNC_PTR(ModeGetEncoder)
      GET_FUNC_PTR(ModeGetConnector)
      GET_FUNC_PTR(ModeGetProperty)
      GET_FUNC_PTR(ModeFreeProperty)
      GET_FUNC_PTR(ModeConnectorSetProperty)
      GET_FUNC_PTR(ModeGetPlaneResources)
      GET_FUNC_PTR(ModeGetPlane)
      GET_FUNC_PTR(ModeObjectGetProperties)
      GET_FUNC_PTR(ModeFreeObjectProperties)
      GET_FUNC_PTR(ModeObjectSetProperty)
      GET_FUNC_PTR(Ioctl)
      GET_FUNC_PTR(Open)
      GET_FUNC_PTR(Close)
      GET_FUNC_PTR(FreeVersion)
      GET_FUNC_PTR(GetVersion)
      GET_FUNC_PTR(GetCap)
      GET_FUNC_PTR(WaitVBlank)
      GET_FUNC_PTR(Malloc)
      GET_FUNC_PTR(PrimeFDToHandle)
      GET_FUNC_PTR(SetClientCap)
      GET_FUNC_PTR(ModeAtomicCommit)
      GET_FUNC_PTR(ModeAtomicAddProperty)
      GET_FUNC_PTR(ModeCreatePropertyBlob)
      GET_FUNC_PTR(ModeDestroyPropertyBlob)
      GET_FUNC_PTR(ModeAtomicAlloc)
      GET_FUNC_PTR(GetLibVersion)
      GET_FUNC_PTR(CommandRead)
      GET_FUNC_PTR(CommandWrite)
      GET_FUNC_PTR(CommandWriteRead)
      GET_FUNC_PTR(HashCreate)
      GET_FUNC_PTR(HashDestroy)
      GET_FUNC_PTR(HashLookup)
      GET_FUNC_PTR(HashInsert)
      GET_FUNC_PTR(HashDelete)
      GET_FUNC_PTR(HashFirst)
      GET_FUNC_PTR(HashNext)
      GET_FUNC_PTR(Msg)
      GET_FUNC_PTR(PrimeHandleToFD)

    }

    libraryIsInitialized = 1;
  }

  HWCLOGI("Out drmShimInit");

  return result;
}

void drmShimEnableVSyncInterception(bool intercept) {
  int drmFd;
  int err = false;

  if (err) {
    ALOGE("drmShimEnableVSyncInterception: Failed to query for master drm fd");
    return;
  }

  ALOG_ASSERT(drmFd != -1);

  if (checks) {
    HWCLOGD("drmShimEnableVSyncInterception: gralloc fd is 0x%x", drmFd);
    checks->SetFd(drmFd);
  }

  propMgr.SetFd(drmFd);

  if (eventHandler == 0 && intercept) {
    eventHandler = std::unique_ptr<DrmShimEventHandler>(new DrmShimEventHandler(checks));
  }
}

// Returns the device we are running on as an enum
bool drmShimPushDeviceType(int32_t device_id) {
  HwcTestState *state = HwcTestState::getInstance();
  ALOG_ASSERT(state);

  switch (device_id) {
    // BYT
    case 0x0f30: /* Baytrail M */
    case 0x0f31: /* Baytrail M */
    case 0x0f32: /* Baytrail M */
    case 0x0f33: /* Baytrail M */
    case 0x0157: /* Baytrail M */
    case 0x0155: /* Baytrail D */

      HWCLOGI("drmShimPushDeviceType: detected BayTrail device");
      state->SetDeviceType(HwcTestState::eDeviceTypeBYT);
      return true;

    // CHT (reference: Source/inc/common/igfxfmid.h)
    case 0x22b2: /* Cherrytrail D  */
    case 0x22b0: /* Cherrytrail M  */
    case 0x22b3: /* Cherrytrail D+ */
    case 0x22b1: /* Cherrytrail M+ */

      HWCLOGI("drmShimPushDeviceType: detected CherryTrail device");
      state->SetDeviceType(HwcTestState::eDeviceTypeCHT);
      return true;

    // SKL / BXT
    case 0x1913: /* SKL ULT GT1.5 */
    case 0x1915: /* SKL ULX GT1.5 */
    case 0x1917: /* SKL DT  GT1.5 */
    case 0x1906: /* SKL ULT GT1 */
    case 0x190E: /* SKL ULX GT1 */
    case 0x1902: /* SKL DT  GT1 */
    case 0x190B: /* SKL Halo GT1 */
    case 0x190A: /* SKL SRV GT1 */
    case 0x1916: /* SKL ULT GT2 */
    case 0x1921: /* SKL ULT GT2F */
    case 0x191E: /* SKL ULX GT2 */
    case 0x1912: /* SKL DT  GT2 */
    case 0x191B: /* SKL Halo GT2 */
    case 0x191A: /* SKL SRV GT2 */
    case 0x191D: /* SKL WKS GT2 */
    case 0x1926: /* SKL ULT GT3 */
    case 0x192B: /* SKL Halo GT3 */
    case 0x192A: /* SKL SRV GT3 */
    case 0x1932: /* SKL DT  GT4 */
    case 0x193B: /* SKL Halo GT4 */
    case 0x193A: /* SKL SRV GT4 */
    case 0x193D: /* SKL WKS GT4 */
    case 0x0A84: /* Broxton */
    case 0x1A84: /* Broxton */
    case 0x1A85: /* Broxton - Intel HD Graphics 500 */
    case 0x5A84: /* Apollo Lake - Intel HD Graphics 505 */
    case 0x5A85: /* Apollo Lake - Intel HD Graphics 500 */

      HWCLOGI("drmShimPushDeviceType: detected Skylake/Broxton device");
      state->SetDeviceType(HwcTestState::eDeviceTypeBXT);
      return true;

    default:

      ALOGE("drmShimPushDeviceType: could not detect device type!");
      HWCERROR(eCheckSessionFail, "Device type %x unknown.", device_id);
      printf("Device type %x unknown. ABORTING!\n", device_id);
      ALOG_ASSERT(0);

      state->SetDeviceType(HwcTestState::eDeviceTypeUnknown);
      return false;
  }
}

void drmShimRegisterCallback(void *cbk) {
  HWCLOGD("Registered drmShimCallback %p", cbk);
  drmShimCallback = (DrmShimCallbackBase *)cbk;
}

/// Close handles
int drmShimCleanup() {
  int result = ercOK;

  result |= dlclose(libDrmHandle);
  result |= dlclose(libDrmIntelHandle);

  return result;
}

int getFunctionPointer(void *LibHandle, const char *Symbol,
                       void **FunctionHandle, HwcTestState *testState) {
  HWCVAL_UNUSED(testState);
  int result = ercOK;

  const char *error = NULL;

  if (LibHandle == NULL) {
    result = -EINVAL;
  } else {
    dlerror();
    *FunctionHandle = dlsym(LibHandle, Symbol);

    error = dlerror();

    if ((*FunctionHandle == 0) && (error != NULL)) {
      result = -EFAULT;

      HWCLOGE("getFunctionPointer %s %s", error, Symbol);
    }
  }

  return result;
}

// Shim implementations of Drm function
void drmModeFreeResources(drmModeResPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreeResources);

  if (!checks || checks->passThrough()) {
    WRAPFUNC(fpDrmModeFreeResources, (ptr));
  }
}

void drmModeFreeCrtc(drmModeCrtcPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreeCrtc);
  if (!checks || checks->passThrough()) {
    WRAPFUNC(fpDrmModeFreeCrtc, (ptr));
  }
}
void drmModeFreeConnector(drmModeConnectorPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreeConnector);

  if (!checks || checks->passThrough()) {
    WRAPFUNC(fpDrmModeFreeConnector, (ptr));
  }
}

void drmModeFreeEncoder(drmModeEncoderPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreeEncoder);

  if (!checks || checks->passThrough()) {
    WRAPFUNC(fpDrmModeFreeEncoder, (ptr));
  }
}

void drmModeFreePlane(drmModePlanePtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreePlane);
  if (!checks || checks->passThrough()) {
    WRAPFUNC(fpDrmModeFreePlane, (ptr));
  }
}

void drmModeFreePlaneResources(drmModePlaneResPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(drmModeFreePlaneResources);

  WRAPFUNC(fpDrmModeFreePlaneResources, (ptr));
}

drmModeResPtr drmModeGetResources(int fd) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeGetResources);

  drmModeResPtr ret = 0;

  WRAPFUNC(ret = fpDrmModeGetResources, (fd));

  if (checks) {
    checks->CheckGetResourcesExit(fd, ret);
  }

  return ret;
}

static Hwcval::Statistics::CumFreqLog<float> addFbTimeStat(
    "drmModeAddFb_duration", 1);

int drmModeAddFB2(int fd, uint32_t width, uint32_t height,
                  uint32_t pixel_format, uint32_t bo_handles[4],
                  uint32_t pitches[4], uint32_t offsets[4], uint32_t *buf_id,
                  uint32_t flags) {
  int retval;
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeAddFB2);

  // Checks are done on the IOCTL, because for BXT HWC has to issue the ioctl
  // directly.
  WRAPFUNC(retval = fpDrmModeAddFB2,
           (fd, width, height, pixel_format, bo_handles, pitches, offsets,
            buf_id, flags));

  return retval;
}

static Hwcval::Statistics::CumFreqLog<float> rmFbTimeStat(
    "drmModeRmFb_duration", 1);

int drmModeRmFB(int fd, uint32_t bufferId) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeRmFB);

  if (checks) {
    checks->CheckRmFB(fd, bufferId);
  }

  int64_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
  int retval;
  WRAPFUNC(retval = fpDrmModeRmFB, (fd, bufferId));
  int64_t duration = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
  rmFbTimeStat.Add(float(duration) / HWCVAL_US_TO_NS);

  return retval;
}


drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t encoder_id) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeGetEncoder);

  drmModeEncoderPtr ret = 0;

  if (!checks || checks->passThrough()) {
    WRAPFUNC(ret = fpDrmModeGetEncoder, (fd, encoder_id));

    if (checks) {
      checks->CheckGetEncoder(encoder_id, ret);
    }
  }

  return ret;
}

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connector_id) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeGetConnector);

  drmModeConnectorPtr ret = 0;

  propMgr.SetFd(fd);

  if (!checks || checks->passThrough()) {
    WRAPFUNC(ret = fpDrmModeGetConnector, (fd, connector_id));

    if (checks) {
      checks->CheckGetConnectorExit(fd, connector_id, ret);
    }
  }

  return ret;
}

drmModePropertyPtr drmModeGetProperty(int fd, uint32_t propertyId) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeGetProperty);

  return propMgr.GetProperty(fd, propertyId);
}

void drmModeFreeProperty(drmModePropertyPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreeProperty);

  WRAPFUNC(fpDrmModeFreeProperty, (ptr));
}

int drmModeConnectorSetProperty(int fd, uint32_t connector_id,
                                uint32_t property_id, uint64_t value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeConnectorSetProperty);

  WRAPFUNCRET(int, fpDrmModeConnectorSetProperty,
              (fd, connector_id, property_id, value));
}

drmModePlaneResPtr drmModeGetPlaneResources(int fd) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeGetPlaneResources);

  drmModePlaneResPtr ret = 0;

  if (!checks || checks->passThrough()) {
    WRAPFUNC(ret = fpDrmModeGetPlaneResources, (fd));

    if (checks) {
      checks->CheckGetPlaneResourcesExit(ret);
    }
  }

  return ret;
}

drmModePlanePtr drmModeGetPlane(int fd, uint32_t plane_id) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeGetPlane);

  drmModePlanePtr ret = 0;

  if (!checks || checks->passThrough()) {
    WRAPFUNC(ret = fpDrmModeGetPlane, (fd, plane_id));

    if (checks) {
      checks->CheckGetPlaneExit(plane_id, ret);
    }
  }

  return ret;
}

drmModeObjectPropertiesPtr drmModeObjectGetProperties(int fd,
                                                      uint32_t object_id,
                                                      uint32_t object_type) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeObjectGetProperties);

  return propMgr.ObjectGetProperties(fd, object_id, object_type);
}

void drmModeFreeObjectProperties(drmModeObjectPropertiesPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeFreeObjectProperties);

  WRAPFUNC(fpDrmModeFreeObjectProperties, (ptr));
}

int drmModeObjectSetProperty(int fd, uint32_t object_id, uint32_t object_type,
                             uint32_t property_id, uint64_t value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeObjectSetProperty);

  char propName[DRM_PROP_NAME_LEN + 1];
  propName[0] = '\0';

  HwcTestCrtc *crtc = 0;
  bool reenableDPMS = false;

  std::string tsName("drmModeObjectSetProperty ");

  if (checks) {
    // What property is being set?
    drmModePropertyPtr prop = fpDrmModeGetProperty(fd, property_id);

    if (prop) {
      char *destPtr = propName;
      for (int i = 0; ((i < DRM_PROP_NAME_LEN) && (prop->name[i] != 0)); ++i) {
        if (isprint(prop->name[i])) {
          *destPtr++ = prop->name[i];
        }
      }
      *destPtr = '\0';

      if (strcmp(propName, "DPMS") == 0) {
        if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
          checks->CheckSetDPMS(object_id, value, eventHandler.get(), crtc,
                               reenableDPMS);
        }
      }
#ifdef DRM_PFIT_PROP
      else if (strcmp(propName, DRM_PFIT_PROP) == 0) {
        if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
          checks->CheckSetPanelFitter(object_id, value);
        }
      }
#endif
#ifdef DRM_SCALING_SRC_SIZE_PROP
      else if (strcmp(propName, DRM_SCALING_SRC_SIZE_PROP) == 0) {
        if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
          checks->CheckSetPanelFitterSourceSize(object_id, (value >> 16),
                                                (value & 0xffff));
        }
      }
#endif
      else if (strcmp(propName, "ddr_freq") == 0) {
        if (object_type == DRM_MODE_OBJECT_CONNECTOR) {
          checks->CheckSetDDRFreq(value);
        }
      } else {
        HWCLOGV("Got prop, not recognized");
      }
      fpDrmModeFreeProperty(prop);
    }

    tsName += propName;
  }

  int64_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
  int status;
  {
    PushThreadState(tsName.c_str());
    WRAPFUNC(status = fpDrmModeObjectSetProperty,
             (fd, object_id, object_type, property_id, value));
  }
  int64_t durationNs = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;

  if (status != 0) {
    HWCERROR(eCheckDrmCallSuccess, "drmModeObjectSetProperty %s failed %d",
             propName, status);
  }
  HWCCHECK(eCheckDrmCallSuccess);

  if (checks && (strcmp(propName, "DPMS") == 0)) {
    checks->CheckSetDPMSExit(fd, crtc, reenableDPMS, eventHandler.get(),
                             status);
  }

  HWCCHECK(eCheckDrmSetPropLatency);
  HWCCHECK(eCheckDrmSetPropLatencyX);
  if (durationNs > 1000000) {
    double durationMs = ((double)durationNs) / 1000000.0;

    if (durationMs > 10.0) {
      HWCERROR(eCheckDrmSetPropLatencyX,
               "drmModeObjectSetProperty %s took %fms", propName, durationMs);
    } else {
      HWCERROR(eCheckDrmSetPropLatency, "drmModeObjectSetProperty %s took %fms",
               propName, durationMs);
    }
  }

  return status;
}

static const char *DrmDecode(unsigned long request) {
#define DECODE_DRM(REQUEST) \
  if (request == REQUEST) { \
    return #REQUEST;        \
  }
  DECODE_DRM(DRM_IOCTL_I915_INIT)
  else DECODE_DRM(DRM_IOCTL_I915_FLUSH) else DECODE_DRM(DRM_IOCTL_I915_FLIP) else DECODE_DRM(DRM_IOCTL_I915_BATCHBUFFER) else DECODE_DRM(DRM_IOCTL_I915_IRQ_EMIT) else DECODE_DRM(
      DRM_IOCTL_I915_IRQ_WAIT) else DECODE_DRM(DRM_IOCTL_I915_GETPARAM) else DECODE_DRM(DRM_IOCTL_I915_SETPARAM) else DECODE_DRM(DRM_IOCTL_I915_ALLOC) else DECODE_DRM(DRM_IOCTL_I915_FREE) else DECODE_DRM(DRM_IOCTL_I915_INIT_HEAP) else DECODE_DRM(DRM_IOCTL_I915_CMDBUFFER) else DECODE_DRM(DRM_IOCTL_I915_DESTROY_HEAP) else DECODE_DRM(DRM_IOCTL_I915_SET_VBLANK_PIPE) else DECODE_DRM(DRM_IOCTL_I915_GET_VBLANK_PIPE) else DECODE_DRM(DRM_IOCTL_I915_VBLANK_SWAP) else DECODE_DRM(DRM_IOCTL_I915_HWS_ADDR) else DECODE_DRM(DRM_IOCTL_I915_GEM_INIT) else DECODE_DRM(DRM_IOCTL_I915_GEM_EXECBUFFER) else DECODE_DRM(DRM_IOCTL_I915_GEM_EXECBUFFER2) else DECODE_DRM(DRM_IOCTL_I915_GEM_PIN) else DECODE_DRM(DRM_IOCTL_I915_GEM_UNPIN) else DECODE_DRM(DRM_IOCTL_I915_GEM_BUSY)
      else DECODE_DRM(DRM_IOCTL_I915_GEM_THROTTLE) else DECODE_DRM(DRM_IOCTL_I915_GEM_ENTERVT) else DECODE_DRM(DRM_IOCTL_I915_GEM_LEAVEVT) else DECODE_DRM(DRM_IOCTL_I915_GEM_CREATE) else DECODE_DRM(DRM_IOCTL_I915_GEM_PREAD) else DECODE_DRM(DRM_IOCTL_I915_GEM_PWRITE) else DECODE_DRM(DRM_IOCTL_I915_GEM_MMAP) else DECODE_DRM(DRM_IOCTL_I915_GEM_MMAP_GTT) else DECODE_DRM(DRM_IOCTL_I915_GEM_SET_DOMAIN) else DECODE_DRM(DRM_IOCTL_I915_GEM_SW_FINISH) else DECODE_DRM(
          DRM_IOCTL_I915_GEM_SET_TILING) else DECODE_DRM(DRM_IOCTL_I915_GEM_GET_TILING) else DECODE_DRM(DRM_IOCTL_I915_GEM_GET_APERTURE) else DECODE_DRM(DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID) else DECODE_DRM(DRM_IOCTL_I915_GEM_MADVISE) else DECODE_DRM(DRM_IOCTL_I915_OVERLAY_PUT_IMAGE) else DECODE_DRM(DRM_IOCTL_I915_OVERLAY_ATTRS) else DECODE_DRM(DRM_IOCTL_I915_SET_SPRITE_COLORKEY) else DECODE_DRM(DRM_IOCTL_I915_GET_SPRITE_COLORKEY) else DECODE_DRM(DRM_IOCTL_I915_GEM_WAIT) else DECODE_DRM(DRM_IOCTL_I915_GEM_CONTEXT_CREATE) else DECODE_DRM(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY) else DECODE_DRM(DRM_IOCTL_I915_REG_READ)

      else DECODE_DRM(DRM_IOCTL_GEM_OPEN) else DECODE_DRM(
          DRM_IOCTL_GEM_FLINK) else DECODE_DRM(DRM_IOCTL_GEM_CLOSE)
#ifdef DRM_IOCTL_I915_EXT_IOCTL
      else DECODE_DRM(DRM_IOCTL_I915_EXT_IOCTL) else DECODE_DRM(
          DRM_IOCTL_I915_EXT_USERDATA)
#endif
      else DECODE_DRM(DRM_IOCTL_MODE_ATOMIC) else {
    static char buf[20];
    sprintf(buf, "0x%lx", request);
    return buf;
  }

#undef DECODE_DRM
}

static Hwcval::Statistics::CumFreqLog<float> flipRequestTimeStat(
    "flip_request_duration", 1);

int drmIoctl(int fd, unsigned long request, void *arg) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmIoctl);
  DrmShimCrtc *crtc = 0;
  // Pre-IOCTL checks
  WRAPFUNCRET(int, fpDrmIoctl, (fd, request, arg));
  // Execute the IOCTL
  int64_t durationNs;
  int status = TimeIoctl(fd, request, arg, durationNs);
  if (checks && request == DRM_IOCTL_MODE_ATOMIC) {
    drm_mode_atomic *drmAtomic = (drm_mode_atomic *)arg;
    int st;

    // Execute the IOCTL
    int64_t durationNs;
    checks->AtomicShimUserData(drmAtomic);
    PushThreadState ts("Nuclear flip request");
    st = TimeIoctl(fd, request, arg, durationNs);
    checks->AtomicUnshimUserData(drmAtomic);
    IoctlLatencyCheck(request, durationNs);
    flipRequestTimeStat.Add(float(durationNs) / HWCVAL_US_TO_NS);
    return st;
  } else {
    // Post-IOCTL checks
    if (testKernel) {
      if (request == DRM_IOCTL_GEM_OPEN) {
        struct drm_gem_open *gemOpen = (struct drm_gem_open *)arg;
        checks->CheckIoctlGemOpen(fd, gemOpen);
      } else if (request == DRM_IOCTL_MODE_ADDFB2) {
        // Record addFB duration in statistics
        addFbTimeStat.Add(float(durationNs) / HWCVAL_US_TO_NS);

        drm_mode_fb_cmd2 *addFb2 = (drm_mode_fb_cmd2 *)arg;
        HWCLOGV_COND(
            eLogDrm,
            "drmModeAddFB2(fd=%u,width=%u,height=%u,pixel_format=0x%x, "
            "bo_handles=(%x %x %x %x), ",
            fd, addFb2->width, addFb2->height, addFb2->pixel_format,
            addFb2->handles[0], addFb2->handles[1], addFb2->handles[2],
            addFb2->handles[3]);
        HWCLOGV_COND(eLogDrm,
                     "  pitches=(%d %d %d %d), offsets=(%d %d %d %d), flags=%x",
                     addFb2->pitches[0], addFb2->pitches[1], addFb2->pitches[2],
                     addFb2->pitches[3], addFb2->offsets[0], addFb2->offsets[1],
                     addFb2->offsets[2], addFb2->offsets[3], addFb2->flags);

#ifndef DRM_MODE_FB_MODIFIERS
        // Some old platforms (e.g. MCGR5.1) do not have support for modifiers.
        // In
        // this
        // case, bypass the checks.
        __u64 dummyModifier[] = {0, DrmShimPlane::ePlaneYTiled, 0, 0};
#endif

        checks->CheckAddFB(fd, addFb2->width, addFb2->height,
                           addFb2->pixel_format, 0, 0, addFb2->handles,
                           addFb2->pitches, addFb2->offsets, addFb2->fb_id,
                           addFb2->flags,
#ifdef DRM_MODE_FB_MODIFIERS
                           addFb2->modifier,
#else
                           dummyModifier,
#endif
                           status);
      } else if (request == DRM_IOCTL_I915_GETPARAM) {
        if (arg) {
          struct drm_i915_getparam *params = (struct drm_i915_getparam *)arg;
          if (params && (params->param == I915_PARAM_CHIPSET_ID) &&
              params->value) {
            int32_t *device = params->value;

            if (!drmShimPushDeviceType(*device)) {
              HWCLOGE("drmIoctl: could not push device type!");
            }
          }
        }
      } else if (request == DRM_IOCTL_GEM_CLOSE) {
        struct drm_gem_close *gemClose = (struct drm_gem_close *)arg;
        testKernel->CheckIoctlGemClose(fd, gemClose);
      } else if (request == DRM_IOCTL_I915_GEM_CREATE) {
        struct drm_i915_gem_create *gemCreate =
            (struct drm_i915_gem_create *)arg;
        testKernel->CheckIoctlGemCreate(fd, gemCreate);
      } else if (request == DRM_IOCTL_PRIME_HANDLE_TO_FD) {
        struct drm_prime_handle *prime = (struct drm_prime_handle *)arg;
        testKernel->CheckIoctlPrime(fd, prime);
      } else if (request == DRM_IOCTL_I915_GEM_WAIT) {
        struct drm_i915_gem_wait *gemWait = (struct drm_i915_gem_wait *)arg;
        HWCCHECK(eCheckDrmIoctlGemWaitLatency);
        if (durationNs > 1000000000)  // 1 sec
        {
          // HWCERROR is logged from the test kernel
          HWCLOGE("drmIoctl DRM_IOCTL_I915_GEM_WAIT boHandle 0x%x took %fs",
                  gemWait->bo_handle, double(durationNs) / 1000000000.0);

          // Pass into the kernel to determine
          // which buffer had the timeout
          testKernel->CheckIoctlGemWait(fd, gemWait, status, durationNs);
        }
      }
    }
  }

  IoctlLatencyCheck(request, durationNs);
  return status;
}

static int TimeIoctl(int fd, unsigned long request, void *arg,
                     int64_t &durationNs) {
  char threadState[HWCVAL_DEFAULT_STRLEN];
  strcpy(threadState, "In Ioctl: ");
  strcat(threadState, DrmDecode(request));
  Hwcval::PushThreadState ts((const char *)threadState);

  int64_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
  int status;
  WRAPFUNC(status = fpDrmIoctl, (fd, request, arg));

  if (status != 0) {
    HWCLOGD_COND(eLogAllIoctls, "fd %d Ioctl %s return status 0x%x=%d", fd,
                 DrmDecode(request), status, status);
  }

  durationNs = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
  return status;
}

static void IoctlLatencyCheck(unsigned long request, int64_t durationNs) {
  // technically this is not right as some IOCTLs don't exercise this check
  // but getting the count right is not very important in this case
  HWCCHECK(eCheckDrmIoctlLatency);
  HWCCHECK(eCheckDrmIoctlLatencyX);

  if (durationNs > 1000000) {
    double durationMs = ((float)durationNs) / 1000000.0;
    const char *drmName = DrmDecode(request);

    // For GEM WAIT, we are waiting for rendering to complete, which could take
    // a very long time
    if (request == DRM_IOCTL_I915_GEM_WAIT) {
    } else if ((request == DRM_IOCTL_I915_GEM_BUSY) ||
               (request == DRM_IOCTL_I915_GEM_SET_DOMAIN) ||
               (request == DRM_IOCTL_I915_GEM_MADVISE) ||
               (request == DRM_IOCTL_GEM_OPEN) ||
               (request == DRM_IOCTL_GEM_CLOSE) ||
               (request == DRM_IOCTL_I915_GEM_SW_FINISH)) {
      // We know these sometimes take a long time, but we don't know what they
      // are for, so don't generate
      // errors
      HWCLOGW_COND(eLogDrm, "drmIoctl %s took %fms", drmName, durationMs);
    } else if (request == DRM_IOCTL_I915_GEM_EXECBUFFER2) {
      // This request should not take a long time, but when using the harness it
      // often does.
      // This is believed to be something to do with the fact that we are
      // filling the buffers
      // from the CPU rather than the GPU.
      // Correct fix is to use some form of GPU composition, perhaps by invoking
      // the GLComposer
      // directly from the harness.
      // Incidentally, using -no_fill does not help even though this means we
      // never
      // access the buffers from the CPU. Gary says this introduces different
      // optimizations
      // in the kernel which will assume that it is a blanking buffer.
      //
      // So, only log the warning, not the error.
      HWCERROR(eCheckDrmIoctlLatency, "drmIoctl %s took %fms", drmName,
               durationMs);
    } else {
      if (durationMs > 10.0) {
        HWCERROR(eCheckDrmIoctlLatencyX, "drmIoctl %s took %fms", drmName,
                 durationMs);
      } else {
        HWCERROR(eCheckDrmIoctlLatency, "drmIoctl %s took %fms", drmName,
                 durationMs);
      }
    }
  }
}

drmVersionPtr drmGetVersion(int fd) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmGetVersion);

  WRAPFUNCRET(drmVersionPtr, fpDrmGetVersion, (fd));
}

drmVersionPtr drmGetLibVersion(int fd) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmGetLibVersion);

  WRAPFUNCRET(drmVersionPtr, fpDrmGetLibVersion, (fd));
}


int drmGetCap(int fd, uint64_t capability, uint64_t *value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmGetCap);

  WRAPFUNCRET(int, fpDrmGetCap, (fd, capability, value));
}

void drmFreeVersion(drmVersionPtr ptr) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmFreeVersion);

  WRAPFUNC(fpDrmFreeVersion, (ptr));
}



int drmOpen(const char *name, const char *busid) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmOpen);
  HWCLOGI("Enter fpDrmOpen %p", fpDrmOpen);

  int rc;
  WRAPFUNC(rc = fpDrmOpen, (name, busid));
  HWCLOGI("drmopen name %s, id %s -> fd %d", name, busid, rc);

  return rc;
}

int drmCommandRead(int fd, unsigned long drmCommandIndex, void *data,
                   unsigned long size) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmCommandRead);

  WRAPFUNCRET(int, fpDrmCommandRead, (fd, drmCommandIndex, data, size));
}



int drmCommandWrite(int fd, unsigned long drmCommandIndex, void *data,
                    unsigned long size) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmCommandWrite);

  WRAPFUNCRET(int, fpDrmCommandWrite, (fd, drmCommandIndex, data, size));
}

int drmCommandWriteRead(int fd, unsigned long drmCommandIndex, void *data,
                        unsigned long size) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmCommandWriteRead);

  WRAPFUNCRET(int, fpDrmCommandWriteRead, (fd, drmCommandIndex, data, size));
}


int drmClose(int fd) {
  CHECK_LIBRARY_INIT
  HWCLOGI("DrmClose %d", fd);
  ALOG_ASSERT(fpDrmClose);

  WRAPFUNCRET(int, fpDrmClose, (fd));
}

int drmWaitVBlank(int fd, drmVBlankPtr vbl) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmWaitVBlank);

  if (eventHandler != 0) {
    return eventHandler->WaitVBlank(vbl);
  } else {
    WRAPFUNCRET(int, fpDrmWaitVBlank, (fd, vbl));
  }
}

void *drmMalloc(int size) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmMalloc);

  WRAPFUNCRET(void *, fpDrmMalloc, (size));
}

void *drmHashCreate(void) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(drmHashCreate);

  WRAPFUNCRET(void *, fpDrmHashCreate, ());
}

int drmHashDestroy(void *t) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHashDestroy);

  WRAPFUNCRET(int, fpDrmHashDestroy, (t));
}

int drmHashLookup(void *t, unsigned long key, void **value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHashLookup);

  WRAPFUNCRET(int, fpDrmHashLookup, (t, key, value));
}

int drmHashInsert(void *t, unsigned long key, void *value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHashInsert);

  WRAPFUNCRET(int, fpDrmHashInsert, (t, key, value));
}

int drmHashDelete(void *t, unsigned long key) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHashDelete);

  WRAPFUNCRET(int, fpDrmHashDelete, (t, key));
}

int drmHashFirst(void *t, unsigned long *key, void **value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHashFirst);

  WRAPFUNCRET(int, fpDrmHashFirst, (t, key, value));
}
int drmHashNext(void *t, unsigned long *key, void **value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHashNext);

  WRAPFUNCRET(int, fpDrmHashNext, (t, key, value));
}


void drmMsg(const char *, ...) {
  CHECK_LIBRARY_INIT
}




int drmHandleEvent(int fd, drmEventContextPtr evctx) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmHandleEvent);

  if (eventHandler != 0) {
    return eventHandler->HandleEvent(fd, evctx);
  } else {
    return fpDrmHandleEvent(fd, evctx);
  }
}


int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmPrimeHandleToFD);

  WRAPFUNCRET(int, fpDrmPrimeHandleToFD, (fd, handle, flags, prime_fd));
}

int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmPrimeFDToHandle);

  int ret;
  WRAPFUNC(ret = fpDrmPrimeFDToHandle, (fd, prime_fd, handle));

  if (testKernel && (ret == 0)) {
    struct drm_gem_open gemOpen;
    gemOpen.name = prime_fd;
    gemOpen.handle = *handle;
    testKernel->CheckIoctlGemOpen(fd, &gemOpen);
  }

  return ret;
}

int drmSetClientCap(int fd, uint64_t capability, uint64_t value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmSetClientCap);
  if (value) {
    HWCLOGD("drmSetClientCap enabled universal planes");
    bUniversalPlanes = true;

    if (checks) {
      checks->SetUniversalPlanes();
    }
  }

  WRAPFUNCRET(int, fpDrmSetClientCap, (fd, capability, value));
}

int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr req, uint32_t flags,
                        void *user_data) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeAtomicCommit);

  WRAPFUNCRET(int, fpDrmModeAtomicCommit, (fd, req, flags, user_data));
}

int drmModeAtomicAddProperty(drmModeAtomicReqPtr req, uint32_t object_id,
                             uint32_t property_id, uint64_t value) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeAtomicAddProperty);

  WRAPFUNCRET(int, fpDrmModeAtomicAddProperty,
              (req, object_id, property_id, value));
}
int drmModeCreatePropertyBlob(int fd, const void *data, size_t size,
                              uint32_t *id) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeCreatePropertyBlob);

  WRAPFUNCRET(int, fpDrmModeCreatePropertyBlob, (fd, data, size, id));
}

int drmModeDestroyPropertyBlob(int fd, uint32_t id) {
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeDestroyPropertyBlob);

  WRAPFUNCRET(int, fpDrmModeDestroyPropertyBlob, (fd, id));
}

drmModeAtomicReqPtr drmModeAtomicAlloc(void){
  CHECK_LIBRARY_INIT
  ALOG_ASSERT(fpDrmModeAtomicAlloc);

  WRAPFUNCRET(int, fpDrmModeAtomicAlloc, ());
}

