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

#ifndef __DRM_SHIM_H__
#define __DRM_SHIM_H__

#include <dlfcn.h>
#include <assert.h>
#include <cutils/log.h>
#include <pthread.h>

#include <xf86drm.h>      //< For structs and types.
#include <xf86drmMode.h>  //< For structs and types.
#include <i915_drm.h>

#include "hardware/hwcomposer2.h"

#include "HwcTestDefs.h"

// Detect DRM version - this is only in the newer DRM
#ifdef DRM_IOCTL_I915_GEM_USERPTR
#define DRM_COORD_INT_TYPE int32_t
#else
#define DRM_COORD_INT_TYPE uint32_t
#endif

class HwcTestState;

// See Drm.c for function descriptions

//-----------------------------------------------------------------------------
// Shim functions
int drmShimInit(bool isHwc, bool isDrm);
void drmShimEnableVSyncInterception(bool intercept);
void drmShimRegisterCallback(void *cbk);
bool drmShimPushDeviceType(int32_t device_id);
int drmShimCleanup();
int getFunctionPointer(void *LibHandle, const char *Symbol,
                       void **FunctionHandle, HwcTestState *testState);

//-----------------------------------------------------------------------------
// libdrm shim functions of real libdrm functions these will be used by the
// calls in to drm shim.
// from xf86drmMode.h functions

void drmModeFreeResources(drmModeResPtr ptr);

void drmModeFreeCrtc(drmModeCrtcPtr ptr);

void drmModeFreeConnector(drmModeConnectorPtr ptr);

void drmModeFreeEncoder(drmModeEncoderPtr ptr);

void drmModeFreePlane(drmModePlanePtr ptr);

void drmModeFreePlaneResources(drmModePlaneResPtr ptr);

drmModeResPtr drmModeGetResources(int fd);

int drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
                 uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
                 uint32_t *buf_id);

int drmModeAddFB2(int fd, uint32_t width, uint32_t height,
                  uint32_t pixel_format, const uint32_t bo_handles[4],
                  const uint32_t pitches[4], const uint32_t offsets[4],
                  uint32_t *buf_id, uint32_t flags);

int drmModeRmFB(int fd, uint32_t bufferId);

drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t crtcId);

int drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId, uint32_t x,
                   uint32_t y, uint32_t *connectors, int count,
                   drmModeModeInfoPtr mode);

drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t encoder_id);

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connectorId);

drmModePropertyPtr drmModeGetProperty(int fd, uint32_t propertyId);

void drmModeFreeProperty(drmModePropertyPtr ptr);

int drmModeConnectorSetProperty(int fd, uint32_t connector_id,
                                uint32_t property_id, uint64_t value);

drmModePlaneResPtr drmModeGetPlaneResources(int fd);

drmModePlanePtr drmModeGetPlane(int fd, uint32_t plane_id);

drmModeObjectPropertiesPtr drmModeObjectGetProperties(int fd,
                                                      uint32_t object_id,
                                                      uint32_t object_type);

void drmModeFreeObjectProperties(drmModeObjectPropertiesPtr ptr);

int drmModeObjectSetProperty(int fd, uint32_t object_id, uint32_t object_type,
                             uint32_t property_id, uint64_t value);

// Functions from xf86.h

int drmIoctl(int fd, unsigned long request, void *arg);

int drmOpen(const char *name, const char *busid);

int drmClose(int fd);

drmVersionPtr drmGetVersion(int fd);

drmVersionPtr drmGetLibVersion(int fd);
int drmGetCap(int fd, uint64_t capability, uint64_t *value);

void drmFreeVersion(drmVersionPtr);

int drmCommandRead(int fd, unsigned long drmCommandIndex, void *data,
                   unsigned long size);

int drmCommandWrite(int fd, unsigned long drmCommandIndex, void *data,
                    unsigned long size);

int drmCommandWriteRead(int fd, unsigned long drmCommandIndex, void *data,
                        unsigned long size);

int drmWaitVBlank(int fd, drmVBlankPtr vbl);

void *drmMalloc(int size);

void *drmHashCreate(void);

int drmHashDestroy(void *t);

int drmHashLookup(void *t, unsigned long key, void **value);

int drmHashInsert(void *t, unsigned long key, void *value);

int drmHashDelete(void *t, unsigned long key);

int drmHashFirst(void *t, unsigned long *key, void **value);

int drmHashNext(void *t, unsigned long *key, void **value);

void drmMsg(const char *, ...);

int drmHandleEvent(int fd, drmEventContextPtr evctx);

int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd);

int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd);

int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle);

int drmSetClientCap(int fd, uint64_t capability, uint64_t value);

#endif  // __DRM_SHIM_H__
