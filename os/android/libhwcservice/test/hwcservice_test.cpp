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

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/TextOutput.h>
#include <utils/String8.h>
#include <cinttypes>
#include "hwcserviceapi.h"
#include "iservice.h"

using namespace android;
using namespace hwcomposer;

static void usage() {
  aout << "Usage: hwcservice_test \n"
          "\t-g: Get current display mode\n"
          "\t-h: Enable HDCP support for a given Display. \n"
          "\t-i: Disable HDCP support for a given Display. \n"
          "\t-j: Enable HDCP support for all displays. \n"
          "\t-k: Disable HDCP support for all displays. \n"
          "\t-s: Set display mode\n"
          "\t-p: Print all available display modes\n"
          "\t-u: Set Hue\n"
          "\t-a: Set Saturation\n"
          "\t-b: Set Brightness\n"
          "\t-c: Set Contrast\n"
          "\t-e: Set Sharpness\n"
          "\t-d: Set deinterlace\n"
          "\t-r: Restore all default video colors/deinterlace \n"
#ifdef ENABLE_PANORAMA
          "\t-w: Trigger Panorama with option of hotplug simulation or not\n"
          "\t-m: Shutdown Panorama with option of hotplug simulation or not\n";
#else
      ;
#endif
  exit(-1);
}

int main(int argc, char** argv) {
  uint32_t display = 0;
  uint32_t display_mode_index = 0;
  bool print_mode = false;
  bool get_mode = false;
  bool set_mode = false;
  bool set_hue = false;
  bool set_saturation = false;
  bool set_brightness = false;
  bool set_contrast = false;
  bool set_deinterlace = false;
  bool set_sharpness = false;
  bool set_hdcp_for_display = false;
  bool set_hdcp_for_all_display = false;
  bool disable_hdcp_for_display = false;
  bool disable_hdcp_for_all_display = false;
  bool restore = false;
#ifdef ENABLE_PANORAMA
  bool trigger_panorama = false;
  bool shutdown_panorama = false;
#endif
  int ch;
#ifdef ENABLE_PANORAMA
  while ((ch = getopt(argc, argv, "gsphijkurabcdemw")) != -1) {
#else
  while ((ch = getopt(argc, argv, "gsphijkurabcde")) != -1) {
#endif
    switch (ch) {
      case 'g':
        get_mode = true;
        break;
      case 's':
        set_mode = true;
        break;
      case 'p':
        print_mode = true;
        break;
      case 'u':
        set_hue = true;
        break;
      case 'r':
        restore = true;
        break;
      case 'a':
        set_saturation = true;
        break;
      case 'b':
        set_brightness = true;
        break;
      case 'c':
        set_contrast = true;
        break;
      case 'e':
        set_sharpness = true;
        break;

      case 'd':
        set_deinterlace = true;
        break;
      case 'h':
        set_hdcp_for_display = true;
        break;
      case 'i':
        disable_hdcp_for_display = true;
        break;
      case 'j':
        set_hdcp_for_all_display = true;
        break;
      case 'k':
        disable_hdcp_for_all_display = true;
        break;
#ifdef ENABLE_PANORAMA
      case 'w':
        trigger_panorama = true;
        break;
      case 'm':
        shutdown_panorama = true;
        break;
#endif
      default:
        usage();
    }
  }
  argc -= optind;
  argv += optind;

#ifdef USE_PROCESS_STATE
  // Initialize ProcessState with /dev/vndbinder as HwcService is
  // in the vndbinder context
  android::ProcessState::initWithDriver("/dev/vndbinder");
#endif

  // Connect to HWC service
  HWCSHANDLE hwcs = HwcService_Connect();
  if (hwcs == NULL) {
    aout << "Could not connect to service\n";
    return -1;
  }

  std::vector<HwcsDisplayModeInfo> modes;
  HwcService_DisplayMode_GetAvailableModes(hwcs, display, modes);
  if (print_mode) {
    aout << "Mode Width x Height\tRefreshRate\tXDpi\tYDpi\n";
    for (uint32_t i = 0; i < modes.size(); i++) {
      aout << i << "\t" << modes[i].width << "\t" << modes[i].height << "\t"
           << modes[i].refresh << "\t" << modes[i].xdpi << "\t" << modes[i].ydpi
           << endl;
    }
  }

  if (get_mode) {
    HwcsDisplayModeInfo mode;
    HwcService_DisplayMode_GetMode(hwcs, display, &mode);
    aout << "Width x Height\tRefreshRate\tXDpi\tYDpi\n";
    aout << mode.width << "\t" << mode.height << "\t" << mode.refresh << "\t"
         << mode.xdpi << "\t" << mode.ydpi << endl;
  }

  if (set_mode) {
    status_t ret =
        HwcService_DisplayMode_SetMode(hwcs, display, display_mode_index);
    if (ret != OK) {
      aout << "Mode set failed\n";
      HwcService_Disconnect(hwcs);
      return 1;
    }
  }

  if (set_hue) {
    aout << "Set Hue to: " << atoi(argv[0]) << endl;
    HwcService_Display_SetColorParam(hwcs, display, HWCS_COLOR_HUE,
                                     atoi(argv[0]));
  }

  if (set_brightness) {
    aout << "Set Brightness to: " << atoi(argv[0]) << endl;
    HwcService_Display_SetColorParam(hwcs, display, HWCS_COLOR_BRIGHTNESS,
                                     atoi(argv[0]));
  }

  if (set_saturation) {
    aout << "Set Saturation to: " << atoi(argv[0]) << endl;
    HwcService_Display_SetColorParam(hwcs, display, HWCS_COLOR_SATURATION,
                                     atoi(argv[0]));
  }

  if (set_contrast) {
    aout << "Set Contrast to: " << atoi(argv[0]) << endl;
    HwcService_Display_SetColorParam(hwcs, display, HWCS_COLOR_CONTRAST,
                                     atoi(argv[0]));
  }

  if (set_sharpness) {
    aout << "Set Sharpness to: " << atoi(argv[0]) << endl;
    HwcService_Display_SetColorParam(hwcs, display, HWCS_COLOR_SHARP,
                                     atoi(argv[0]));
  }

  if (set_deinterlace) {
    aout << "Set Deinterlace to: " << atoi(argv[0]) << endl;
    HwcService_Display_SetDeinterlaceParam(hwcs, display, atoi(argv[0]));
  }

  if (restore) {
    aout << "Restore default colors\n";
    HwcService_Display_RestoreDefaultColorParam(hwcs, display, HWCS_COLOR_HUE);
    HwcService_Display_RestoreDefaultColorParam(hwcs, display,
                                                HWCS_COLOR_SATURATION);
    HwcService_Display_RestoreDefaultColorParam(hwcs, display,
                                                HWCS_COLOR_BRIGHTNESS);
    HwcService_Display_RestoreDefaultColorParam(hwcs, display,
                                                HWCS_COLOR_CONTRAST);
    HwcService_Display_RestoreDefaultColorParam(hwcs, display,
                                                HWCS_COLOR_SHARP);
    HwcService_Display_RestoreDefaultDeinterlaceParam(hwcs, display);
  }

  if (set_hdcp_for_display) {
    aout << "Set HDCP For Display: " << atoi(argv[0]) << endl;
    if (atoi(argv[0]) == 0) {
      HwcService_Video_EnableHDCPSession_ForDisplay(hwcs, atoi(argv[0]),
                                                    HWCS_CP_CONTENT_TYPE0);
    } else {
      HwcService_Video_EnableHDCPSession_ForDisplay(hwcs, atoi(argv[0]),
                                                    HWCS_CP_CONTENT_TYPE1);
    }
  }

  if (disable_hdcp_for_display) {
    aout << "Disabling HDCP For Display: " << atoi(argv[0]) << endl;
    HwcService_Video_DisableHDCPSession_ForDisplay(hwcs, atoi(argv[0]));
  }

  if (set_hdcp_for_all_display) {
    aout << "Set HDCP For All Displays Using Fallback: " << atoi(argv[0])
         << endl;
    if (atoi(argv[0]) == 0) {
      HwcService_Video_EnableHDCPSession_AllDisplays(hwcs,
                                                     HWCS_CP_CONTENT_TYPE0);
    } else {
      HwcService_Video_EnableHDCPSession_AllDisplays(hwcs,
                                                     HWCS_CP_CONTENT_TYPE1);
    }
  }

  if (disable_hdcp_for_all_display) {
    aout << "Disabling HDCP For All Displays. " << endl;
    HwcService_Video_DisableHDCPSession_AllDisplays(hwcs);
  }

#ifdef ENABLE_PANORAMA
  if (trigger_panorama) {
    int simulation_hotplug = 0;
    if (argc >= 1) {
      simulation_hotplug = atoi(argv[0]);
    }
    aout << "Trigger Panorama view mode, simulation hotplug: "
         << simulation_hotplug << endl;

    HwcService_TriggerPanorama(hwcs, simulation_hotplug);
  }

  if (shutdown_panorama) {
    int simulation_hotplug = 0;
    if (argc >= 1) {
      simulation_hotplug = atoi(argv[0]);
    }
    aout << "Shutdown Panorama view mode, simulation hotplug: "
         << simulation_hotplug << endl;
    HwcService_ShutdownPanorama(hwcs, simulation_hotplug);
  }
#endif

  HwcService_Disconnect(hwcs);
  return 0;
}
