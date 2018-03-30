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

#ifndef __HwcTestDefs_h__
#define __HwcTestDefs_h__

#include <inttypes.h>
#include "xf86drmMode.h"

#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#ifndef PRIx64  // should be defined in inttypes.h
#define PRIx64 "llx"
#endif
#ifndef PRIX64  // should be defined in inttypes.h
#define PRIX64 "llX"
#endif
#ifndef PRId64  // should be defined in inttypes.h
#define PRId64 "lld"
#endif
#ifndef PRIi64  // should be defined in inttypes.h
#define PRIi64 "lld"
#endif
#ifndef PRIu64  // should be defined in inttypes.h
#define PRIu64 "llu"
#endif

#if __x86_64__
#define HWCVAL_VENDOR_LIBPATH "/vendor/lib64"
#define HWCVAL_LIBPATH "/vendor/lib64"
#else
#define HWCVAL_VENDOR_LIBPATH "/system/vendor/lib"
#define HWCVAL_LIBPATH "/system/lib"
#endif

#define HWCVAL_MAX_CRTCS 3
#define HWCVAL_MAX_PIPES 5
#define HWCVAL_MAX_LOG_DISPLAYS 3

// The default of the default! This is overriden at run time by the environment
// variable HWCVAL_LOG_DEFAULT_PRIORITY
#define HWCVAL_DEFAULT_LOG_PRIORITY ANDROID_LOG_WARN

// Window on cropf and display frame size matching
// Needs to be at least 2 to allow for adjustment to even values on 422/NV12
// layers
#define HWCVAL_CROP_MARGIN 2.0
#define HWCVAL_DISPLAYFRAME_SIZE_MARGIN 2

// Normally DRM calls should not take >1ms
#define HWCVAL_DRM_CALL_DURATION_WARNING_LEVEL_NS 1000000

// Max no of layer lists we can handle behind present OnSet
#define HWCVAL_LAYERLISTQUEUE_DEPTH 100

// Maximum number of bitmap files we will dump out on SF-REF target comparison
#define MAX_SF_MISMATCH_DUMP_FILES 20

// Number of frames for which errors are ignored after an extended mode change
#define HWCVAL_EXTENDED_MODE_CHANGE_WINDOW 4

// At what interval in number of frames do we perform DrmShimBuffer garbage
// collection?
#define HWCVAL_BUFFER_GARBAGECOLLECT_FREQUENCY 100

// What is the minimum length of time after last seeing a DrmShimBuffer that we
// should retain reference to it?
#define HWCVAL_BUFFER_LIFETIME 100

// Number of current buffers that can be tracked before we generate an error
// message
#define CURRENT_BUFFER_COUNT_WARNING_LEVEL 1000

// Number of current and historical buffers that can be tracked before we
// generate an error mesage
#define TOTAL_BUFFER_COUNT_WARNING_LEVEL 5000

// Times as required by sync_wait
#define HWCVAL_SYNC_WAIT_100MS 100
#define HWCVAL_SYNC_WAIT_1MS 1

// Times as required by usleep
#define HWCVAL_USLEEP_100MS 100000
#define HWCVAL_USLEEP_1MS 1000

// Time conversion factors
#define HWCVAL_US_TO_NS 1000
#define HWCVAL_MS_TO_US 1000
#define HWCVAL_MS_TO_NS 1000000
#define HWCVAL_SEC_TO_US 1000000
#define HWCVAL_SEC_TO_NS INT64_C(1000000000)

// Multiplication factor to convert floating point [0,1] alpha values to integer
// [0,255].
#define HWCVAL_ALPHA_FLOAT_TO_INT 255

// Max time we permit from unblanking request to content on screen
#define HWCVAL_MAX_UNBLANKING_LATENCY_DEFAULT_US (600 * HWCVAL_MS_TO_US)

// Composition validation ensures caller does not destroy input buffers using
// fences
#define HWCVAL_MERGE_COMPVAL_FENCES

// When we start a reference composition, wait for it to be submitted to the GPU
#define HWCVAL_WAIT_FOR_REFERENCE_COMPOSITION

// Pass/fail threshold for SSIM comparison
#define HWCVAL_SSIM_ACCEPTANCE_LEVEL 0.999

// Display index for HDMI displays
#define HWCVAL_HDMI_DISPLAY_INDEX 1

// This is the CRTC id (and array / display index) for the virtual display (if
// present)
#define HWCVAL_VD_CRTC_ID 100
#define HWCVAL_VD_DISPLAY_INDEX 2

// Number of events in GEM event queue
#define HWCVAL_MAX_GEM_EVENTS 2000

// Enable MultiDisplayService shim, for checking on extended mode
#define HWCVAL_ENABLE_MDS_SHIM

// Default max string length for ID strings
#define HWCVAL_DEFAULT_STRLEN 256

// ADF Definitions

// Maximum ADF interfaces supported
#define HWCVAL_ADF_MAX_INTERFACES 3

// Minimum number of frames to keep in the Layer List Queue
#define HWCVAL_MIN_LLQ_RETENTION 6

// Enable debug on mutexes
#define HWCVAL_LOCK_DEBUG 0
#if HWCVAL_SYSTRACE
#define HWCVAL_LOCK_TRACE
#endif

// Enable specific features
// These will be defined by make file system - see Hwcval.mk
//#define HWCVAL_TARGET_HAS_MULTIPLE_DISPLAY

// Number of entries in the thread hash table
#define HWCVAL_THREAD_TABLE_SIZE 257

// Number of frames after a hot plug before HWC should respond by issuing a
// teardown
// message in the log
//
// This can take a surprisingly long time, because HWC is busy carrying out the
// drmModeGetConnector stuff - and it has to do that before it can decide if a
// display
// has been newly plugged or not.
#define HWCVAL_HOTPLUG_DETECTION_WINDOW_NS (2 * HWCVAL_SEC_TO_NS)

// Enable Render Compression on BXT builds
// TODO Enable render compression patch here

// Allow these to be defined on all builds
#define DRM_MODE_FB_AUX_PLANE (1 << 2)
#define HWCVAL_BXT_FIRST_UNSUPPORTED_NV12_PLANE 2

/*
 * START SECTION
 */

// Extended DRM features:

// DRRS property values used by DRM/HWC
enum {
  HWCVAL_DRRS_NOT_SUPPORTED = 0,
  HWCVAL_STATIC_DRRS_SUPPORT = 1,
  HWCVAL_SEAMLESS_DRRS_SUPPORT = 2,
  HWCVAL_SEAMLESS_DRRS_SUPPORT_SW = 3,
};

// define a all is well return code use errno.h for error codes
enum eReturnCode {
  ercOK = 0,
};

// Miscellaneous values to signify "Unknown"
enum { eNoLayer = 0xff, eNoDisplayIx = 0xffffffff };

#define HWCVAL_UNDEFINED_FRAME_NUMBER 0xffffffff

/*
 * END SECTION
 */

#endif  // __HwcTestDefs_h__
