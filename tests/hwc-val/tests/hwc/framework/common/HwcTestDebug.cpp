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

#include "HwcTestDebug.h"

bool HwcTestDumpBufferToDisk(const char* pchFilename, uint32_t num,
                             HWCNativeHandle grallocHandle,
                             uint32_t outputDumpMask) {
  HWCVAL_UNUSED(pchFilename);
  HWCVAL_UNUSED(num);
  HWCVAL_UNUSED(grallocHandle);
  HWCVAL_UNUSED(outputDumpMask);
  ETRACE("HwcTestDumpBufferToDisk is not implemented \n");
  return false;
}

bool HwcTestDumpAuxBufferToDisk(const char* pchFilename, uint32_t num,
                                HWCNativeHandle grallocHandle) {
  // No AUX buffer support yet.
  HWCVAL_UNUSED(pchFilename);
  HWCVAL_UNUSED(num);
  HWCVAL_UNUSED(grallocHandle);
  ETRACE("HwcTestDumpAuxBufferToDisk is not implemented \n");
  return false;
}

bool HwcTestDumpMemBufferToDisk(const char* pchFilename, uint32_t num,
                                const void* handle, uint32_t outputDumpMask,
                                uint8_t* pData) {
  HWCVAL_UNUSED(pchFilename);
  HWCVAL_UNUSED(num);
  HWCVAL_UNUSED(handle);
  HWCVAL_UNUSED(outputDumpMask);
  HWCVAL_UNUSED(pData);
  ETRACE("HwcTestDumpMemBufferToDisk is not implemented \n");
  return false;
}
