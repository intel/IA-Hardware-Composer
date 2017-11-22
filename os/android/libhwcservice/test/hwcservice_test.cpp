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
          "\t-s: Set display mode\n"
          "\t-p: Print all available display modes\n"
          "\t-u: Set Hue\n";
  exit(-1);
}

int main(int argc, char** argv) {
  uint32_t display = 0;
  uint32_t display_mode_index = 0;
  bool print_mode = false;
  bool get_mode = false;
  bool set_mode = false;
  bool set_hue = false;
  int ch;
  while ((ch = getopt(argc, argv, "gsphu")) != -1) {
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
      case 'h':
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
    HwcService_Display_SetColorParam(hwcs, display, HWCS_COLOR_HUE,
                                     atoi(argv[0]));
  }

  HwcService_Disconnect(hwcs);
  return 0;
}
