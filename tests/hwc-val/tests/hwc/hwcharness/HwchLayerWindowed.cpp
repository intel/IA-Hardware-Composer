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

#include "HwcTestState.h"
#include "HwchSystem.h"
#include "HwchLayerWindowed.h"

HwchLayerWindowed::HwchLayerWindowed(uint32_t width, uint32_t height,
                                     buffer_handle_t handle)
    : Hwch::Layer("Windowed", width, height, HAL_PIXEL_FORMAT_RGBA_8888, 0),
      mHandle(handle) {
  Hwch::System &system = Hwch::System::getInstance();
  uint32_t panel_width = system.GetDisplay(0).GetWidth();
  uint32_t panel_height = system.GetDisplay(0).GetHeight();

  SetIgnoreScreenRotation(true);

  // Trigger a warning if the width and height of the virtual window exceeds the
  // size of the panel.
  if (((VIRTUAL_WINDOW_OFFSET + width) > panel_width) ||
      (VIRTUAL_WINDOW_OFFSET + height) > panel_height) {
    HWCLOGW(
        "The window parameters are too large for the panel - capping to "
        "screen edges.\n");
  }

  SetLogicalDisplayFrame(
      Hwch::LogDisplayRect(VIRTUAL_WINDOW_OFFSET, VIRTUAL_WINDOW_OFFSET,
                           ((VIRTUAL_WINDOW_OFFSET + width) <= panel_width)
                               ? VIRTUAL_WINDOW_OFFSET + width
                               : panel_width,
                           ((VIRTUAL_WINDOW_OFFSET + height) <= panel_height)
                               ? VIRTUAL_WINDOW_OFFSET + height
                               : panel_height));

  SetCrop(Hwch::LogCropRect(0, 0, panel_width, panel_height));
};

void HwchLayerWindowed::Send(hwc2_layer_t &hwLayer,
                             hwcomposer::HwcRect<int> *visibleRegions,
                             uint32_t &visibleRegionCount) {
  // Setup the layer properties and the visible regions
  AssignLayerProperties(hwLayer, mHandle);
  AssignVisibleRegions(hwLayer, visibleRegions, visibleRegionCount);
};

void HwchLayerWindowed::CalculateRects(Hwch::Display &display) {
  // Just calculate the source crop and the display frame
  CalculateSourceCrop(display);
  CalculateDisplayFrame(display);

  HWCLOGI("CalculateRects(%s): LogCrop %s Crop %f %f %f %f", mName.c_str(),
          mLogicalCropf.Str("%f"), mSourceCropf.left, mSourceCropf.top,
          mSourceCropf.right, mSourceCropf.bottom);
  HWCLOGI("CalculateRects(%s): LogDF   %s DF   %d %d %d %d", mName.c_str(),
          mLogicalDisplayFrame.Str("%d"), mDisplayFrame.left, mDisplayFrame.top,
          mDisplayFrame.right, mDisplayFrame.bottom);
}
