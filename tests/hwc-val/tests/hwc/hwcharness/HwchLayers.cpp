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

#include "HwchLayers.h"
#include "HwchDisplay.h"
#include "HwchSystem.h"
#include "HwchPngImage.h"

Hwch::RGBALayer::RGBALayer(Coord<int32_t> w, Coord<int32_t> h, float updateFreq,
                           uint32_t fg, uint32_t bg, uint32_t matrix)
    : Hwch::Layer("RGBA", w, h) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, updateFreq, fg,
                                                     bg, matrix));
  SetBlending(HWC_BLENDING_PREMULT);
}

Hwch::SkipLayer::SkipLayer(bool needsBuffer)
    : Hwch::Layer("Skip", MaxRel(0), MaxRel(0)) {
  SetSkip(true, needsBuffer);

  if (needsBuffer) {
    SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eRed,
                                                       Alpha(eBlack, 128)));
  }
}

Hwch::CameraLayer::CameraLayer()
    : Hwch::Layer("Camera", MaxRel(0), MaxRel(-eStatusBarHeight),
                  HAL_PIXEL_FORMAT_YCbCr_422_I) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eDarkPurple,
                                                     eLightPurple));
}

Hwch::CameraUILayer::CameraUILayer()
    : Hwch::Layer("CameraUI", MaxRel(-260), MaxRel(-eStatusBarHeight)) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 2.0, eDarkGreen,
                                                     eLightGreen));
  SetOffset(260, 0);
}

Hwch::NavigationBarLayer::NavigationBarLayer()
    : Hwch::Layer("NavigationBar", MaxRel(0), eNavigationBarHeight) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 10.0, eYellow,
                                                     eLightBlue));
  SetOffset(0, MaxRel(-eNavigationBarHeight));
}

Hwch::WallpaperLayer::WallpaperLayer()
    : Hwch::Layer("Wallpaper", WallpaperSize(), WallpaperSize(),
                  HAL_PIXEL_FORMAT_RGBA_8888,
                  1)  // Never updates so actually one buffer should be enough
{
  SetPattern(GetPatternMgr().CreateSolidColourPtn(mFormat, eGreen));
  SetCrop(LogCropRect(0, 0, MaxRelF(0), MaxRelF(-eStatusBarHeight)));
  SetLogicalDisplayFrame(
      LogDisplayRect(0, eStatusBarHeight, MaxRel(0), MaxRel(0)));
}

Hwch::LauncherLayer::LauncherLayer()
    : Hwch::Layer("Launcher", MaxRel(0), MaxRel(-eStatusBarHeight)) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 2.0, eBlack,
                                                     Alpha(eCyan, 128)));
  SetOffset(0, eStatusBarHeight);
  SetBlending(HWC_BLENDING_PREMULT);
}

Hwch::StatusBarLayer::StatusBarLayer()
    : Hwch::Layer("StatusBar", MaxRel(0), eStatusBarHeight) {
  SetPattern(
      GetPatternMgr().CreateHorizontalLinePtn(mFormat, 1.0, eBlack, eWhite));
}

Hwch::DialogBoxLayer::DialogBoxLayer() : Hwch::Layer("DialogBox", 401, 112) {
  SetPattern(
      GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eRed, eLightRed));
  SetLogicalDisplayFrame(LogDisplayRect(Scaled(759, 1920), Scaled(460, 1280),
                                        Scaled(759 + 401, 1920),
                                        Scaled(460 + 112, 1280)));
}

Hwch::GalleryLayer::GalleryLayer()
    : Hwch::Layer("Gallery", MaxRel(0), MaxRel(-eNavigationBarHeight)) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eLightGreen,
                                                     eDarkGreen));
}

Hwch::GalleryUILayer::GalleryUILayer()
    : Hwch::Layer("GalleryUI", MaxRel(0), 40) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eLightCyan,
                                                     eBlue));
}

Hwch::MenuLayer::MenuLayer() : Hwch::Layer("Menu", 220, 220) {
  SetPattern(
      GetPatternMgr().CreateHorizontalLinePtn(mFormat, 2.0, eWhite, eDarkRed));
  SetOffset(MaxRel(-220), eStatusBarHeight);
};

Hwch::GameFullScreenLayer::GameFullScreenLayer(Coord<int32_t> w,
                                               Coord<int32_t> h)
    : Hwch::Layer("GameFullScreen", w, h, HAL_PIXEL_FORMAT_RGB_565) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eDarkPurple,
                                                     eLightGreen));
}

Hwch::AdvertLayer::AdvertLayer() : Hwch::Layer("Advert", 400, 112) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 60.0, eDarkBlue,
                                                     eLightBlue));
  SetOffset(CtrRel(-400 / 2), MaxRel(-eNavigationBarHeight - 112));
};

Hwch::NotificationLayer::NotificationLayer()
    : Hwch::Layer("Notification", 512, MaxRel(-eStatusBarHeight)) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 2.0, eGreen,
                                                     eDarkPurple));
  SetOffset(MaxRel(-512), eStatusBarHeight);
}

Hwch::NV12VideoLayer::NV12VideoLayer(uint32_t w, uint32_t h)
    : Hwch::Layer("NV12Video", (w != 0) ? Coord<int32_t>(w) : MaxRel(0),
                  (h != 0) ? Coord<int32_t>(h) : MaxRel(0)) {
  SetPattern(
      GetPatternMgr().CreateHorizontalLinePtn(mFormat, 24.0, eRed, eDarkBlue));
  SetHwcAcquireDelay(0);
}

Hwch::YV12VideoLayer::YV12VideoLayer(uint32_t w, uint32_t h)
    : Hwch::Layer("YV12Video", (w != 0) ? Coord<int32_t>(w) : MaxRel(0),
                  (h != 0) ? Coord<int32_t>(h) : MaxRel(0),
                  HAL_PIXEL_FORMAT_YV12) {
  SetPattern(GetPatternMgr().CreateHorizontalLinePtn(mFormat, 24.0, eDarkRed,
                                                     eLightBlue));
  SetHwcAcquireDelay(0);
}

Hwch::TransparentFullScreenLayer::TransparentFullScreenLayer()
    : Hwch::Layer("TransparentFullScreen", MaxRel(0), MaxRel(0),
                  HAL_PIXEL_FORMAT_RGBA_8888, 1) {
  SetPattern(GetPatternMgr().CreateSolidColourPtn(mFormat, 0));
}

Hwch::PngLayer::PngLayer(Hwch::PngImage& png, float updateFreq,
                         uint32_t lineColour)
    : Hwch::Layer(png.GetName(), 0, 0, HAL_PIXEL_FORMAT_RGBA_8888) {
  Hwch::Pattern* ptn =
      GetPatternMgr().CreatePngPtn(mFormat, updateFreq, png, lineColour);

  // Set gralloc buffer width and height to width and height of png image
  mWidth.mValue = png.GetWidth();
  mHeight.mValue = png.GetHeight();

  SetPattern(ptn);
  SetOffset(0, 0);
}
