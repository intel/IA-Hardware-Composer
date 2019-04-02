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

#ifndef OS_ANDROID_HWC_HWCSERVICEAPI_H_
#define OS_ANDROID_HWC_HWCSERVICEAPI_H_

#include <stdint.h>

#ifdef __cplusplus
#include <vector>
extern "C" {
#endif

// Header file version.  Please increment on any API additions.
// NOTE: Additions ONLY! No API modifications allowed (to maintain
// compatability).
#define HWCS_VERSION 1

typedef void *HWCSHANDLE;

typedef enum _EHwcsBool {
  HWCS_FALSE = 0,
  HWCS_TRUE = 1,
} EHwcsBool;

typedef int status_t;

HWCSHANDLE HwcService_Connect();
void HwcService_Disconnect(HWCSHANDLE hwcs);

const char *HwcService_GetHwcVersion(HWCSHANDLE hwcs);

// DisplayControl

// Should these be hard coded in the API?
enum {
  HWCS_MAX_OVERSCAN = 100,   // The limit of the control parameters are
                             // +/-HWCS_MAX_OVERSCAN inclusive.
  HWCS_OVERSCAN_RANGE = 15,  // HWCS_OVERSCAN_RANGE describes the % of the
                             // display size a max control setting will adjust
                             // by.
};

/// Set overscan in the range +/-MAX_OVERSCAN inclusive.
// -ve : zoom/crop the image  (increase display overscan).
// +ve : shrink the image (decrease display overscan).
status_t HwcService_Display_SetOverscan(HWCSHANDLE hwcs, uint32_t display,
                                        int32_t xoverscan, int32_t yoverscan);
// Get last set overscan.
// Returns INVALID_OPERATION if overscan has not been set and
// xoverscan/yoverscan are untouched.
status_t HwcService_Display_GetOverscan(HWCSHANDLE hwcs, uint32_t display,
                                        int32_t *xoverscan, int32_t *yoverscan);

typedef enum _EHwcsScalingMode {
  HWCS_SCALE_CENTRE =
      0,               // Present the content centred at 1:1 source resolution.
  HWCS_SCALE_STRETCH,  // Do not preserve aspect ratio - scale to fill the
                       // display without cropping.
  HWCS_SCALE_FIT,      // Preserve aspect ratio - scale to closest edge (may be
                       // letterboxed or pillarboxed).
  HWCS_SCALE_FILL,     // Preserve aspect ratio - scale to fill the display (may
                       // crop the content).
  HWCS_SCALE_MAX_ENUM  // End of enum.
} EHwcsScalingMode;

/// Set scaling to one of EScalingMode.
// Returns OK if succesful.
status_t HwcService_Display_SetScaling(HWCSHANDLE hwcs, uint32_t display,
                                       EHwcsScalingMode eScalingMode);

// Get last set scaling.
// Returns OK if succesful.
// Returns INVALID_OPERATION if scaling has not been set and eScalingMode is
// untouched.
status_t HwcService_Display_GetScaling(HWCSHANDLE hwcs, uint32_t display,
                                       EHwcsScalingMode *eScalingMode);

// Enable blank, true---blank, false---unblank
// Returns OK if succesful.
status_t HwcService_Display_EnableBlank(HWCSHANDLE hwcs, uint32_t display,
                                        EHwcsBool blank);

typedef enum _EHwcsColorControl {
  HWCS_COLOR_BRIGHTNESS,
  HWCS_COLOR_CONTRAST,
  HWCS_COLOR_GAMMA,
  HWCS_COLOR_SATURATION,
  HWCS_COLOR_HUE,
  HWCS_COLOR_SHARP,
} EHwcsColorControl;

typedef enum _EHwcsDeinterlaceControl {
  HWCS_DEINTERLACE_NONE,
  HWCS_DEINTERLACE_BOB,
  HWCS_DEINTERLACE_WEAVE,
  HWCS_DEINTERLACE_MOTIONADAPTIVE,
  HWCS_DEINTERLACE_MOTIONCOMPENSATED,
} EHwcsDeinterlaceControl;

// Enumerations for content type.
typedef enum _EHwcsContentType {
  HWCS_CP_CONTENT_TYPE0,  // Can support any HDCP specifiction.
  HWCS_CP_CONTENT_TYPE1,  // Can support only HDCP 2.2 and higher specification.
} EHwcsContentType;

status_t HwcService_Display_RestoreDefaultColorParam(HWCSHANDLE hwcs,
                                                     uint32_t display,
                                                     EHwcsColorControl color);
status_t HwcService_Display_RestoreDefaultDeinterlaceParam(HWCSHANDLE hwcs,
                                                           uint32_t display);
status_t HwcService_Display_GetColorParam(HWCSHANDLE hwcs, uint32_t display,
                                          EHwcsColorControl color, float *value,
                                          float *startvalue, float *endvalue);
status_t HwcService_Display_SetColorParam(HWCSHANDLE hwcs, uint32_t display,
                                          EHwcsColorControl color, float value);
status_t HwcService_Display_SetDeinterlaceParam(HWCSHANDLE hwcs,
                                                uint32_t display,
                                                uint32_t mode);

// DisplayModeControl

typedef enum _EHwcsModeFlags {
  HWCS_MODE_FLAG_NONE = 0,
  HWCS_MODE_FLAG_PREFERRED = 1 << 0,
  HWCS_MODE_FLAG_SECURE = 1 << 1,
  HWCS_MODE_FLAG_INTERLACED = 1 << 2,
  HWCS_MODE_FLAG_CURRENT = 1 << 4,
} EHwcsModeFlags;

/// Enumerations for common aspect ratios
/// Any ratio can be supported, with the upper 16 bits containing one dimension,
/// the lower 16 bits contains the lower dimension
typedef enum _EHwcsModeAspectRatio {
  HWCS_MODE_ASPECT_RATIO_ANY = 0x00000000,
  HWCS_MODE_ASPECT_RATIO_4_3 = 0x00040003,
  HWCS_MODE_ASPECT_RATIO_16_9 = 0x00100009,
} EHwcsModeAspectRatio;

typedef struct _HwcsDisplayModeInfo {
  uint32_t width;
  uint32_t height;
  uint32_t refresh;
  uint32_t xdpi;
  uint32_t ydpi;
} HwcsDisplayModeInfo;

#ifdef __cplusplus
/// query all available modes
// If non-NULL: fills pModeList with up to modeCount modes.
// Returns the number of modes available.
status_t HwcService_DisplayMode_GetAvailableModes(
    HWCSHANDLE hwcs, uint32_t display,
    std::vector<HwcsDisplayModeInfo> &pModeList);
#endif

/// get current mode
status_t HwcService_DisplayMode_GetMode(HWCSHANDLE hwcs, uint32_t display,
                                        HwcsDisplayModeInfo *pMode);

/// set mode
status_t HwcService_DisplayMode_SetMode(HWCSHANDLE hwcs, uint32_t display,
                                        const uint32_t config);

// VideoControl

// The control enables the usage of HDCP for all planes supporting this feature
// on display. Some displays can support latest HDCP specification and also
// have ability to fallback to older specifications i.e. HDCP 2.2 and 1.4
// in case latest specification cannot be supported for some reason. Type
// of content can be set by content_type.
status_t HwcService_Video_EnableHDCPSession_ForDisplay(
    HWCSHANDLE hwcs, uint32_t connector, EHwcsContentType content_type);

// The control enables the usage of HDCP for all planes supporting this
// feature on all connected displays. Some displays can support latest HDCP
// specification and also have ability to fallback to older specifications
// i.e. HDCP 2.2 and 1.4 in case latest specification cannot be supported
// for some reason. Type of content can be set by content_type.
status_t HwcService_Video_EnableHDCPSession_AllDisplays(
    HWCSHANDLE hwcs, EHwcsContentType content_type);

// The control disables the usage of HDCP for all planes supporting this feature
// on display.
status_t HwcService_Video_DisableHDCPSession_ForDisplay(HWCSHANDLE hwcs,
                                                        uint32_t connector);

// The control disables the usage of HDCP for all planes supporting this feature
// on all connected displays.
status_t HwcService_Video_DisableHDCPSession_AllDisplays(HWCSHANDLE hwcs);

#ifdef ENABLE_PANORAMA
status_t HwcService_TriggerPanorama(HWCSHANDLE hwcs,
                                    uint32_t hotplug_simulation);
status_t HwcService_ShutdownPanorama(HWCSHANDLE hwcs,
                                     uint32_t hotplug_simulation);
#endif

status_t HwcService_Video_SetHDCPSRM_ForDisplay(HWCSHANDLE hwcs,
                                                uint32_t connector,
                                                const int8_t *SRM,
                                                uint32_t SRMLengh);

status_t HwcService_Video_SetHDCPSRM_AllDisplays(HWCSHANDLE hwcs,
                                                 const int8_t *SRM,
                                                 uint32_t SRMLengh);

uint32_t HwcService_GetDisplayIDFromConnectorID(HWCSHANDLE hwcs,
                                                uint32_t connector_id);

status_t HwcService_EnableDRMCommit(HWCSHANDLE hwcs, uint32_t enable,
                                    uint32_t display_id);

status_t HwcService_ResetDrmMaster(HWCSHANDLE hwcs, uint32_t drop_master);

// The control enables a the protected video subsystem to control when to
// replace any
// encrypted content with a default bitmap (usually black).

// Enable the display of encrypted buffers with the specified sessionID and
// instanceID.
// This will take effect from the next composed frame.
// Any previously enabled instanceID will be disabled (replaced by the default
// image)
status_t HwcService_Video_EnableEncryptedSession(HWCSHANDLE hwcs,
                                                 uint32_t sessionID,
                                                 uint32_t instanceID);

// Disable specific encrypted session.
// This call will trigger the HWC to remove any encrypted buffers with the
// specified sessionID
// from the screen and replace with a default image.
// The function will block until the screen no longer contains any encrypted
// data with this session.
// This should be called by any subsystem that knows that a specific encrypted
// video session is about to
// become invalid.
status_t HwcService_Video_DisableEncryptedSession(HWCSHANDLE hwcs,
                                                  uint32_t sessionID);

// Disable all protected sessions.
// This call will trigger the HWC to remove any encrypted buffers from the
// screen and replace
// with a default image.
// The function will block until the screen no longer contains any encrypted
// data with any session.
// This should be called by any subsystem that knows that all encrypted video
// sessions are about to
// become invalid.
status_t HwcService_Video_DisableAllEncryptedSessions(HWCSHANDLE hwcs);

// Return whether or not the specified session/instance is enabled.
EHwcsBool HwcService_Video_IsEncryptedSessionEnabled(HWCSHANDLE hwcs,
                                                     uint32_t sessionID,
                                                     uint32_t instanceID);

// Hint provided by the application about the global optimization mode for the
// driver
typedef enum _EHwcsOptimizationMode {
  HWCS_OPTIMIZE_NORMAL,
  HWCS_OPTIMIZE_VIDEO,
  HWCS_OPTIMIZE_CAMERA,
} EHwcsOptimizationMode;
status_t HwcService_Video_SetOptimizationMode(HWCSHANDLE hwcs,
                                              EHwcsOptimizationMode mode);

// MDS
status_t HwcService_MDS_UpdateVideoState(HWCSHANDLE hwcs,
                                         int64_t videoSessionID,
                                         EHwcsBool isPrepared);

status_t HwcService_MDS_UpdateVideoFPS(HWCSHANDLE hwcs, int64_t videoSessionID,
                                       int32_t fps);

status_t HwcService_MDS_UpdateInputState(HWCSHANDLE hwcs, EHwcsBool state);

// Widi
status_t HwcService_Widi_GetSingleDisplay(HWCSHANDLE hwcs, EHwcsBool *enable);
status_t HwcService_Widi_SetSingleDisplay(HWCSHANDLE hwcs, EHwcsBool enable);

#ifdef __cplusplus
}
#endif

#endif  // OS_ANDROID_HWC_HWCSERVICEAPI_H_
