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

#include "iservice.h"
#include "idiagnostic.h"

#include <binder/IServiceManager.h>

#include "logentry.h"

#define TENTH_SECOND 100000

void printHelp(void) {
  printf("\n");
  printf("HWC Log Viewer\n");
  printf(" -h   Print help\n");
  printf(" -v   More verbose trace\n");
  printf(" -f   + Include Fence trace\n");
  printf(" -b   + Include BufferManager trace\n");
  printf(" -q   + Include Queue trace\n");
  printf(" -vv  All trace - very verbose\n");
  printf(" \n");
  printf(" To merge all trace to logcat (very verbose):\n");
  printf("   adb shell service call hwc.info 99\n");
  printf(" \n");
}

int main(int argc, char** argv) {
  bool bVeryVerbose = false;
  bool bVerbose = false;
  bool bFences = false;
  bool bBufferManager = false;
  bool bQueue = false;

  // process arguments
  int argIndex = 1;
  while (argIndex < argc) {
    if (strcmp(argv[argIndex], "-h") == 0) {
      printHelp();
      return 0;
    }
    if (strcmp(argv[argIndex], "-v") == 0) {
      bVerbose = true;
      printf("bVerbose = %d\n", bVerbose);
    }
    if (strcmp(argv[argIndex], "-vv") == 0) {
      bVeryVerbose = true;
      printf("bVeryVerbose = %d\n", bVeryVerbose);
    }
    if (strcmp(argv[argIndex], "-f") == 0) {
      bFences = true;
      printf("bFences = %d\n", bFences);
    }
    if (strcmp(argv[argIndex], "-b") == 0) {
      bBufferManager = true;
      printf("bBufferManager = %d\n", bBufferManager);
    }
    if (strcmp(argv[argIndex], "-q") == 0) {
      bQueue = true;
      printf("bQueue = %d\n", bQueue);
    }
    argIndex++;
  }

  while (1) {
    // Find and connect to HWC service
    sp<IService> hwcService = interface_cast<IService>(
        defaultServiceManager()->getService(String16(IA_HWC_SERVICE_NAME)));
    if (hwcService == NULL) {
      usleep(TENTH_SECOND);
      continue;
    }

    sp<IDiagnostic> pDiagnostic = hwcService->GetDiagnostic();
    if (pDiagnostic == NULL) {
      usleep(TENTH_SECOND);
      continue;
    }

    printf("Connected to service %s and obtained diagnostic interface\n\n",
           IA_HWC_SERVICE_NAME);

    while (1) {
      LogEntry entry;
      status_t ret = entry.read(pDiagnostic);

      if (ret != OK) {
        if (ret == IDiagnostic::eLogTruncated) {
          printf("...\n");
        } else if (ret == NOT_ENOUGH_DATA) {
          fflush(stdout);
          usleep(4000);  // 4 ms sleep
          continue;
        } else {
          printf("readLogEntry error, attempting to reconnect.\n\n");
          break;
        }
      }
      entry.print(bVeryVerbose, bVerbose, bFences, bBufferManager, bQueue);
    }
  }

  return 0;
}
