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

// A class for providing display information to a SurfaceSender.
// This abstracts SurfaceSender from the source of the information.
// Currently only width and height are returned for the embbeded display via
// SurfaceComposerClient but DRM could be used here.

#ifndef __DISPLAY_INFO_H__
#define __DISPLAY_INFO_H__

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#include <ui/DisplayInfo.h>

using namespace android;

class Display {
 private:
  uint32_t width;
  uint32_t height;

 public:
  Display();

  /// Get display height
  uint32_t GetWidth(void) {
    return width;
  }

  /// Get display width
  uint32_t GetHeight(void) {
    return height;
  }
};

#endif  // __DISPLAY_INFO_H__
