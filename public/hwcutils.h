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

#ifndef COMMON_UTILS_HWCUTILS_H_
#define COMMON_UTILS_HWCUTILS_H_

#include <hwcdefs.h>

namespace hwcomposer {

// Helper functions.

// Call poll() on fd.
//  - timeout: time in miliseconds to stay blocked before returning if fd
//  is not ready.
int HWCPoll(int fd, int timeout);

// Reset's rect to include region hwc_region.
void ResetRectToRegion(const HwcRegion& hwc_region, HwcRect<int>& rect);

}  // namespace hwcomposer

#endif  // COMMON_UTILS_HWCUTILS_H_
