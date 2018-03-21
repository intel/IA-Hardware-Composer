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

#ifndef __HwchLayers_h__
#define __HwchLayers_h__

#include "HwchLayer.h"

namespace Hwch {
enum { eNavigationBarHeight = 72, eStatusBarHeight = 38 };

class PngImage;

class RGBALayer : public Layer {
 public:
  RGBALayer(Coord<int32_t> w = MaxRel(0), Coord<int32_t> h = MaxRel(0),
            float updateFreq = 60.0, uint32_t fg = eWhite,
            uint32_t bg = eLightGrey, uint32_t matrix = 0);
};

class SkipLayer : public Layer {
 public:
  SkipLayer(bool needsBuffer = false);  // default to skip layer with no buffer
};

class CameraLayer : public Layer {
 public:
  CameraLayer();
};

class CameraUILayer : public Layer {
 public:
  CameraUILayer();
};

class NavigationBarLayer : public Layer {
 public:
  NavigationBarLayer();
};

class WallpaperLayer : public Layer {
 public:
  WallpaperLayer();
};

class LauncherLayer : public Layer {
 public:
  LauncherLayer();
};

class StatusBarLayer : public Layer {
 public:
  StatusBarLayer();
};

class DialogBoxLayer : public Layer {
 public:
  DialogBoxLayer();
};

class GalleryLayer : public Layer {
 public:
  GalleryLayer();
};

class GalleryUILayer : public Layer {
 public:
  GalleryUILayer();
};

class MenuLayer : public Layer {
 public:
  MenuLayer();
};

class GameFullScreenLayer : public Layer {
 public:
  GameFullScreenLayer(Coord<int32_t> w = MaxRel(0),
                      Coord<int32_t> h = MaxRel(-eNavigationBarHeight));
};

class AdvertLayer : public Layer {
 public:
  AdvertLayer();
};

class NotificationLayer : public Layer {
 public:
  NotificationLayer();
};

class NV12VideoLayer : public Layer {
 public:
  NV12VideoLayer(uint32_t w = 0, uint32_t h = 0);
};

class YV12VideoLayer : public Layer {
 public:
  YV12VideoLayer(uint32_t w = 0, uint32_t h = 0);
};

class TransparentFullScreenLayer : public Layer {
 public:
  TransparentFullScreenLayer();
};

class PngLayer : public Layer {
 public:
  PngLayer(){};
  PngLayer(Hwch::PngImage& png, float updateFreq = 60.0,
           uint32_t lineColour = eWhite);
};
}

#endif  // __HwchLayers_h__
