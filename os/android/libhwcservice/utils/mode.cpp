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
#include <utils/String8.h>
#include <cinttypes>
#include "hwcserviceapi.h"
#include "iservice.h"

using namespace android;
using namespace hwcomposer;

int main(int argc, char** argv) {
  // Argument parameters
  uint32_t display = 0;
  uint32_t displayModeIndex = 0;
  bool printModes = false;
  bool bPreferred = false;
  bool getMode = false;
  bool setMode = false;
  int argIndex = 1;
  int nonOptions = 0;

  // Process options
  while (argIndex < argc) {
    // Process any non options
    switch (nonOptions) {
      case 0:
        display = atoi(argv[argIndex]);
        break;
      case 1:
        if (strcmp(argv[argIndex], "get") == 0) {
          getMode = true;
        } else if (strcmp(argv[argIndex], "set") == 0) {
          setMode = true;
          displayModeIndex = atoi(argv[argIndex++]);
        } else if (strcmp(argv[argIndex], "print") == 0)
          printModes = true;
        break;
    }
    nonOptions++;
    argIndex++;
  }

  if (nonOptions == 0) {
    printf("Usage: %s  [displayId <print/get/set <displayconfigindex>>]\n",
           argv[0]);
    return 1;
  }

  // Find and connect to HWC service
  sp<IService> hwcService = interface_cast<IService>(
      defaultServiceManager()->getService(String16(IA_HWC_SERVICE_NAME)));
  if (hwcService == NULL) {
    printf("Could not connect to service %s\n", IA_HWC_SERVICE_NAME);
    return -1;
  }

  // Connect to HWC service
  HWCSHANDLE hwcs = HwcService_Connect();
  if (hwcs == NULL) {
    printf("Could not connect to service\n");
    return -1;
  }

  std::vector<HwcsDisplayModeInfo> modes;
  HwcService_DisplayMode_GetAvailableModes(hwcs, display, modes);
  if (printModes) {
    for (uint32_t i = 0; i < modes.size(); i++) {
      printf("\nMode WidthxHeight\tRefreshRate\tXDpi\tYDpi\n");
      printf("%-6d %-4d %-6d\t%d\t%d\t%d\t\n", i, modes[i].width,
             modes[i].height, modes[i].refresh, modes[i].xdpi, modes[i].ydpi);
    }
  }
  if (getMode) {
    HwcsDisplayModeInfo mode;
    HwcService_DisplayMode_GetMode(hwcs, display, &mode);
    printf("%-4d %-6d\t%d\t%d\t%d\t\n", mode.width, mode.height, mode.refresh,
           mode.xdpi, mode.ydpi);
  }
  if (setMode) {
    status_t ret =
        HwcService_DisplayMode_SetMode(hwcs, display, displayModeIndex);
    if (ret != OK) {
      printf("Mode set failed\n");
      HwcService_Disconnect(hwcs);
      return 1;
    }
  }
  HwcService_Disconnect(hwcs);
  return 0;
}
