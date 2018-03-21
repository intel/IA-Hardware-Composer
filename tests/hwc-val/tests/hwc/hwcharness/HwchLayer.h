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

#ifndef __HwchLayer_h__
#define __HwchLayer_h__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <utils/Vector.h>
#include <platformdefines.h>
#include <hardware/hwcomposer2.h>

#include "HwchPattern.h"
#include "HwchBufferSet.h"
#include "HwchCoord.h"
#include "HwchDefs.h"
#include "Hwcval.h"
#include "utils/String8.h"

namespace Hwch {
class System;
class Frame;
class Display;

// Some pre-defined RGBA colors
enum RGBAColorType {
  eBlack = 0x000000FF,
  eRed = 0xFF0000FF,
  eGreen = 0x00FF00FF,
  eBlue = 0x0000FFFF,
  eYellow = 0xFF00FFFF,
  eCyan = 0x00FFFFFF,
  ePurple = 0x800080FF,
  eGrey = 0x808080FF,
  eLightRed = 0xFFA07AFF,
  eLightGreen = 0x90EE90FF,
  eLightBlue = 0xADD8E6FF,
  eLightCyan = 0xE0FFFFFF,
  eLightPurple = 0x9370DBFF,
  eLightGrey = 0xD3D3D3FF,
  eDarkRed = 0xFF0000FF,
  eDarkGreen = 0x00FF00FF,
  eDarkBlue = 0x0000FFFF,
  eDarkCyan = 0x008B8BFF,
  eDarkPurple = 0x4B0082FF,
  eDarkGrey = 0x696969FF,
  eWhite = 0xFFFFFFFF
};

inline int Alpha(int c, int a) {
  // Do premultiplication
  int r = (c >> 24) & 0xff;
  int g = (c >> 16) & 0xff;
  int b = (c >> 8) & 0xff;

  return ((((r * a) & 0xff00) << 16) | (((g * a) & 0xff00) << 8) |
          ((b * a) & 0xff00)) |
         a;
}

class Layer {
 public:
  int32_t mCompType;         // Composition type originally defined
  int32_t mCurrentCompType;  // Composition type now
  int32_t compositionType;
  uint32_t mHints;
  uint32_t mFlags;
  int32_t mLogicalTransform;
  int32_t mPhysicalTransform;
  int32_t mBlending;
  uint32_t mFormat;
  uint32_t mNumBuffers;
  uint32_t mPlaneAlpha;    // only used from Android 4.3
  Coord<int32_t> mWidth;   // buffer width
  Coord<int32_t> mHeight;  // buffer height
  uint32_t mUsage;
  HWCNativeHandle gralloc_handle;
  hwc2_layer_t hwc2_layer;
  // Forced tiling options
  enum {
    eLinear = 1,
    eXTile = 2,
    eYTile = 4,
    eAnyTile = eLinear | eXTile | eYTile
  };
  uint32_t mTile;

  enum EncryptionType {
    eNotEncrypted = 0,
    eEncrypted = 1,
    eInvalidSessionId = 2,  // bitmask
    eInvalidInstanceId = 4
  };
  uint32_t mEncrypted;

  // enum to define layer compression types
  enum CompressionType {
    eCompressionAuto = 0,  // Automatic (system defined) compression
    eCompressionRC,        // Render compressed only
    eCompressionCC_RC,     // Render and clear compressed
    eCompressionHint       // Look-up (and apply) hint for buffer
  } mCompressionType;

  bool mIgnoreScreenRotation;
  uint32_t mHwcAcquireDelay;
  bool mNeedBuffer;

  std::unique_ptr<Pattern> mPattern;

  BufferSetPtr mBufs;  // current buffer

  LogCropRect mLogicalCropf;
  hwcomposer::HwcRect<float> mSourceCropf;
  hwcomposer::HwcRect<float> mOldSourceCropf;
  LogDisplayRect mLogicalDisplayFrame;
  hwcomposer::HwcRect<int> mDisplayFrame;
  hwcomposer::HwcRect<int> mOldDisplayFrame;
  hwcomposer::HwcRegion mVisibleRegion;

  // Pointers to any layers we have cloned off this layer
  Layer* mClonedLayers[MAX_DISPLAYS];

  // Indicates co-ordinates or transform for the layer have changed
  bool mGeometryChanged;

  // Indicates that this layer should be cloned to all other active displays
  bool mIsForCloning;

  // Indicates that a framebuffer update is required in HwchFrame::Send().
  // (Bitmask per-display).
  uint32_t mUpdatedSinceFBComp;

  // Indicates that this layer is a clone from the panel
  Layer* mIsACloneOf;

  // Frame to which the layer has been assigned
  Frame* mFrame;

  // Unique name of the layer for debug and identification purposes
  std::string mName;

  // Provided to make code simpler in the subclasses
  Hwch::System& mSystem;

 public:
  // Constructors and destructors
  Layer();

  Layer(const char* name, Coord<int32_t> width, Coord<int32_t> height,
        uint32_t pixelFormat = HAL_PIXEL_FORMAT_RGBA_8888,
        int32_t numBuffers = -1,
        uint32_t usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE |
                         GRALLOC_USAGE_HW_RENDER);

  Layer(const Layer& rhs, bool clone = true);

  virtual ~Layer();

  // Assignment
  Layer& operator=(const Layer& rhs);

  // Duplicate
  // Any subclass of Layer WHICH HAS DATA MEMBERS must provide implementations
  // of Dup() and the copy constructor.
  virtual Layer* Dup();

  ///////////////////////////////
  // Functions for public use. //
  ///////////////////////////////
  void SetCrop(const LogCropRect& rect);
  const LogCropRect& GetCrop();
  void SetLogicalDisplayFrame(const LogDisplayRect& rect);
  const LogDisplayRect& GetLogicalDisplayFrame();
  void SetOffset(const Coord<int>& x, const Coord<int>& y);
  void SetIsACloneOf(Layer* clone);

  void SetBlending(uint32_t blending);
  void SetTransform(uint32_t transform);
  void SetPlaneAlpha(uint32_t planeAlpha);

  void SetCompression(Hwch::Layer::CompressionType compression);
  Hwch::Layer::CompressionType GetCompression(void);

  // What effect will screen rotation have on this layer?
  void SetIgnoreScreenRotation(bool ignore);

  // Define delay before acquire fence is signalled to HWC
  // Delay units are not specified here but could be frames or ms.
  // Frames will be the default because this gives repeatability.
  void SetHwcAcquireDelay(uint32_t delay);

  // Provide the pattern to be used for this layer.
  // Layer will take ownership of the pattern.
  void SetPattern(Pattern* pattern);
  Pattern& GetPattern();
  bool HasPattern();

  // Set encryption state
  void SetEncrypted(uint32_t encrypted = eEncrypted);
  bool IsEncrypted();

  // Request Z-order changes
  void SendToBack();
  void SendToFront();
  void SendForward();
  void SendBackward();

  uint32_t GetFlags();
  void SetFlags(uint32_t flags);
  void SetSkip(bool skip, bool needBuffer = true);

  uint32_t GetPanelWidth();
  uint32_t GetPanelHeight();

  Coord<int32_t> GetWidth();
  Coord<int32_t> GetHeight();

  uint32_t GetFormat();
  void SetFormat(uint32_t format);
  bool FormatSupportsRC();
  bool IsUpdatedSinceLastFBComp(uint32_t disp);
  void ClearUpdatedSinceLastFBComp(uint32_t disp);
  void SetUpdated();
  bool HasNV12Format();
  bool IsSkip();

  void SetTile(uint32_t tile);
  uint32_t GetTile();

  const char* GetName();

  ////////////////////////////////////////
  // Functions for framework usage only //
  ////////////////////////////////////////

  // void AssignLayerProperties(hwc2_layer_t& hwLayer, buffer_handle_t
  // handle);
  hwc_rect_t* AssignVisibleRegions(hwc_rect_t* visibleRegions,
                                   uint32_t& visibleRegionCount);
  virtual HWCNativeHandle Send();

  void SetCompositionType(uint32_t compType);
  void PostFrame(uint32_t compType, int releaseFenceFd);

  void DoCloning(Layer** lastClonedLayer, Frame* frame);
  void RemoveClones();
  Hwch::Layer* RemoveClone(Hwch::Layer* cloneToRemove);
  Layer& SetGeometryChanged(bool changed);
  bool IsGeometryChanged();
  bool IsForCloning();
  bool IsAClone();
  bool IsAutomaticClone();
  bool IsFullScreenRotated(Display& display);
  Layer& SetForCloning(bool forCloning);
  Layer& SetFrame(Frame* frame);
  Hwch::Frame* GetFrame();
  void SetAcquireFence(int mergeFence);

  void CalculateSourceCrop(Display& display);
  void CalculateDisplayFrame(Display& display);
  virtual void CalculateRects(Display& display);
  void AdoptBufFromPanel();

  void FillExcluding(const hwcomposer::HwcRect<int>& rect,
                     const hwcomposer::HwcRect<int>& exclRect);

  float GetBytesPerPixel();
  uint32_t GetMemoryUsage();

  static const char* CompressionTypeStr(CompressionType ct);
  static const char* AuxBufferStateStr(int state);

  hwcomposer::NativeBufferHandler *bufHandler;
 private:
  void UpdateRCResolve();
};

inline bool Layer::IsAutomaticClone() {
  return ((mIsACloneOf != 0) && (mIsACloneOf->mIsForCloning));
}

inline bool Layer::IsAClone() {
  return (mIsACloneOf != 0);
}

inline void Layer::AdoptBufFromPanel() {
  mBufs = mIsACloneOf->mBufs;
}

inline bool Layer::HasPattern() {
  return (mPattern != 0);
}

inline Coord<int32_t> Layer::GetWidth() {
  return mWidth;
}

inline Coord<int32_t> Layer::GetHeight() {
  return mHeight;
}

inline uint32_t Layer::GetFormat() {
  return mFormat;
}

inline void Layer::SetFormat(uint32_t format) {
  // Must not do this after the buffer set has been assigned.
  // i.e. it should only be done when the buffer is brand new.
  ALOG_ASSERT(mBufs.get() == 0);
  mFormat = format;
}

inline bool Layer::FormatSupportsRC() {
  switch (mFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return true;

    default:
      return false;
  }
}

inline bool Layer::IsUpdatedSinceLastFBComp(uint32_t disp) {
  return mUpdatedSinceFBComp & (1 << disp);
}

inline void Layer::ClearUpdatedSinceLastFBComp(uint32_t disp) {
  mUpdatedSinceFBComp &= ~(1 << disp);
}

inline void Layer::SetUpdated() {
  mUpdatedSinceFBComp = HWCH_ALL_DISPLAYS_UPDATED;
  mGeometryChanged = true;
}

inline void Layer::SetTile(uint32_t tile) {
  mTile = tile;
}

inline uint32_t Layer::GetTile() {
  return mTile;
}

inline void Layer::SetCompression(Hwch::Layer::CompressionType compression) {
  if (FormatSupportsRC()) {
    mCompressionType = compression;
  }
}

inline Hwch::Layer::CompressionType Layer::GetCompression(void) {
  return mCompressionType;
}

inline bool Hwch::Layer::IsSkip() {
  return ((mFlags & HWC_SKIP_LAYER) != 0);
}
};

#endif  // __HwchLayer_h__
