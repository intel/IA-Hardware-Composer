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

#include "HwchDisplay.h"
#include "HwchLayer.h"
#include "HwchSystem.h"
#include "HwchBufferFormatConfig.h"
#include "HwchAsyncEventGenerator.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"
#include "HwchInterface.h"

// Display mode control
#include "hwcserviceapi.h"

using namespace hwcomposer;

Hwch::Display::Display()
    : mFramebufferTarget(0),
      mRotation(hwcomposer::HWCRotation::kRotateNone),
      mVirtualDisplay(false),
      mConnected(false) {
  mAttributes.vsyncPeriod = 0;
  mAttributes.width = 0;
  mAttributes.height = 0;
}

Hwch::Display::~Display() {
  if (mFramebufferTarget) {
    delete mFramebufferTarget;
  }

  // Disconnect from the HWC Service Api
  if (mHwcsHandle) {
    HwcService_Disconnect(mHwcsHandle);
  }
}

void Hwch::Display::Init(hwcomposer::NativeBufferHandler *bufferHandler, uint32_t ix, Hwch::System* system) {
  bufHandler = bufferHandler;
  mDisplayIx = ix;
  mFmtCfgMgr = &system->GetBufferFormatConfigManager();
}

uint32_t Hwch::Display::GetLogicalWidth() {
  if (((uint32_t)mRotation) & hwcomposer::HWCRotation::kRotate90) {
    return mAttributes.height;
  } else {
    return mAttributes.width;
  }
}

uint32_t Hwch::Display::GetLogicalHeight() {
  if (((uint32_t)mRotation) & hwcomposer::HWCRotation::kRotate90) {
    return mAttributes.width;
  } else {
    return mAttributes.height;
  }
}

const int Hwch::Display::mRotationTable
    [hwcomposer::HWCRotation::kMaxRotate]
    [hwcomposer::HWCTransform::kMaxTransform] = {
        // Gives the result of applying each of the 4 possible rotations (in
        // HWCRotation) to each of the 8 possible transforms.
        {hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform270},
        {hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kIdentity},
        {hwcomposer::HWCTransform::kTransform180,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform90},
        {hwcomposer::HWCTransform::kTransform270,
         hwcomposer::HWCTransform::kTransform45,
         hwcomposer::HWCTransform::kTransform135,
         hwcomposer::HWCTransform::kTransform90,
         hwcomposer::HWCTransform::kIdentity,
         hwcomposer::HWCTransform::kReflectX,
         hwcomposer::HWCTransform::kReflectY,
         hwcomposer::HWCTransform::kTransform180}};

void Hwch::Display::CloneTransform(Layer& panelLayer, Layer& layer) {
  Hwch::Display& panelDisplay = Hwch::System::getInstance().GetDisplay(0);
  HWCLOGD_COND(eLogCloning, "Cloning to display %d", mDisplayIx);

  if (mDisplayIx > 0) {
    uint32_t panelWidth = panelDisplay.mAttributes.width;
    uint32_t panelHeight = panelDisplay.mAttributes.height;
    int32_t logicalPanelWidth;
    int32_t logicalPanelHeight;
    int32_t logicalDFLeft;
    int32_t logicalDFRight;
    int32_t logicalDFTop;
    int32_t logicalDFBottom;

    layer.mSourceCropf = panelLayer.mSourceCropf;

    switch (panelDisplay.mRotation) {
      case hwcomposer::HWCRotation::kRotate90: {
        logicalPanelWidth = panelHeight;
        logicalPanelHeight = panelWidth;

        logicalDFLeft = panelLayer.mDisplayFrame.top;
        logicalDFRight = panelLayer.mDisplayFrame.bottom;
        logicalDFTop = logicalPanelHeight - panelLayer.mDisplayFrame.right;
        logicalDFBottom = logicalPanelHeight - panelLayer.mDisplayFrame.left;
        break;
      }
      case hwcomposer::HWCRotation::kRotate180: {
        logicalPanelWidth = panelWidth;
        logicalPanelHeight = panelHeight;

        logicalDFLeft = logicalPanelWidth - panelLayer.mDisplayFrame.right;
        logicalDFRight = logicalPanelWidth - panelLayer.mDisplayFrame.left;
        logicalDFTop = logicalPanelHeight - panelLayer.mDisplayFrame.bottom;
        logicalDFBottom = logicalPanelHeight - panelLayer.mDisplayFrame.top;
        break;
      }
      case hwcomposer::HWCRotation::kRotate270: {
        logicalPanelWidth = panelHeight;
        logicalPanelHeight = panelWidth;

        logicalDFLeft = logicalPanelWidth - panelLayer.mDisplayFrame.bottom;
        logicalDFRight = logicalPanelWidth - panelLayer.mDisplayFrame.top;
        logicalDFTop = panelLayer.mDisplayFrame.left;
        logicalDFBottom = panelLayer.mDisplayFrame.right;
        break;
      }
      default: {
        logicalPanelWidth = panelWidth;
        logicalPanelHeight = panelHeight;
        logicalDFLeft = panelLayer.mDisplayFrame.left;
        logicalDFRight = panelLayer.mDisplayFrame.right;
        logicalDFTop = panelLayer.mDisplayFrame.top;
        logicalDFBottom = panelLayer.mDisplayFrame.bottom;
      }
    }

    float xScale = float(mAttributes.width) / float(logicalPanelWidth);
    float yScale = float(mAttributes.height) / float(logicalPanelHeight);
    float scale = min(xScale, yScale);
    uint32_t cloneDispWidth = logicalPanelWidth * scale;
    uint32_t cloneDispHeight = logicalPanelHeight * scale;
    uint32_t cloneXOffset = (mAttributes.width - cloneDispWidth) / 2;
    uint32_t cloneYOffset = (mAttributes.height - cloneDispHeight) / 2;

    layer.mDisplayFrame.left = cloneXOffset + scale * logicalDFLeft;
    layer.mDisplayFrame.right = cloneXOffset + scale * logicalDFRight;
    layer.mDisplayFrame.top = cloneYOffset + scale * logicalDFTop;
    layer.mDisplayFrame.bottom = cloneYOffset + scale * logicalDFBottom;

    mFmtCfgMgr->AdjustDisplayFrame(layer.GetFormat(), layer.mDisplayFrame,
                                   GetWidth(), GetHeight());

    // Cloned layer is not rotated.
    layer.mLogicalTransform = panelLayer.mLogicalTransform;

    // Buffers may have been reassigned
    layer.mBufs = panelLayer.mBufs;

    // Give new layer its own name
    layer.mName = panelLayer.mName + std::string("_") + std::to_string(mDisplayIx);

    // Make sure the cloned display gets marked as geometry changed
    layer.mGeometryChanged = true;

    HWCLOGI("Cloned layer %s (%d,%d,%d,%d) to (%d,%d,%d,%d) scale (%f, %f)",
            layer.mName.c_str(), panelLayer.mDisplayFrame.left,
            panelLayer.mDisplayFrame.top, panelLayer.mDisplayFrame.right,
            panelLayer.mDisplayFrame.bottom, layer.mDisplayFrame.left,
            layer.mDisplayFrame.top, layer.mDisplayFrame.right,
            layer.mDisplayFrame.bottom, (double)xScale, (double)yScale);
  }
}

void Hwch::Display::CreateFramebufferTarget() {
  if (mAttributes.width > 0 && mAttributes.height > 0) {
    if (mFramebufferTarget) {
      delete mFramebufferTarget;
    }

    mFramebufferTarget = new Hwch::Layer(
        "FramebufferTarget", mAttributes.width, mAttributes.height,
        HAL_PIXEL_FORMAT_RGBA_8888, HWCH_FBT_NUM_BUFFERS,
        GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_COMPOSER |
            GRALLOC_USAGE_HW_RENDER);
    CopyRect(HAL_PIXEL_FORMAT_RGBA_8888,
             mFramebufferTarget->mLogicalDisplayFrame,
             mFramebufferTarget->mDisplayFrame);
    mFramebufferTarget->mSourceCropf.right = mAttributes.width;
    mFramebufferTarget->mSourceCropf.bottom = mAttributes.height;

    // Explicitly create the buffer set here since FRAMEBUFFERTARGETs don't go
    // through the
    // CalculateDisplayFrame method
    mFramebufferTarget->mBufs = std::shared_ptr<BufferSet>(new BufferSet(
        mAttributes.width, mAttributes.height, HAL_PIXEL_FORMAT_RGBA_8888,
        HWCH_FBT_NUM_BUFFERS, GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_COMPOSER |
                                  GRALLOC_USAGE_HW_RENDER));
    if (mFramebufferTarget->mBufs->GetHandle() == 0) {
      HWCERROR(eCheckTestBufferAlloc,
               "Failed to create framebuffer target for display %d",
               mDisplayIx);
    }

    mFramebufferTarget->SetCompositionType(HWC2_COMPOSITION_DEVICE);
    mFramebufferTarget->SetBlending(HWC_BLENDING_PREMULT);

    mFramebufferTarget->SetPattern(new Hwch::FramebufferTargetPtn());

    HWCLOGD("Created FramebufferTarget Layer for display %d @ %p", mDisplayIx,
            mFramebufferTarget);
  } else if (mFramebufferTarget) {
    HWCLOGD("Deleting FramebufferTarget Layer for display %d @ %p", mDisplayIx,
            mFramebufferTarget);
    delete mFramebufferTarget;
    mFramebufferTarget = 0;
  }
}

Hwch::Layer& Hwch::Display::GetFramebufferTarget() {
  return *mFramebufferTarget;
}

bool Hwch::Display::IsConnected() {
  return (mConnected && (mAttributes.width > 0) && (mAttributes.height > 0));
}

hwcomposer::HWCRotation Hwch::Display::SetRotation(
    hwcomposer::HWCRotation rotation) {
  // Calculate the relative rotation that should be applied to achieve this
  hwcomposer::HWCRotation rot = SubtractRotation(rotation, mRotation);
  mRotation = rotation;
  HWCLOGI("Display %d New rotation is %d", mDisplayIx, mRotation);

  return rot;
}

hwcomposer::HWCRotation Hwch::Display::GetRotation() {
  return mRotation;
}

int Hwch::Display::RotateTransform(int transform, hwcomposer::HWCRotation rot) {
  return mRotationTable[rot][transform];
}

int Hwch::Display::RotateTransform(int transform) {
  return RotateTransform(transform, mRotation);
}

void Hwch::Display::ConvertRect(uint32_t bufferFormat, LogicalRect<int>& lr,
                                hwcomposer::HwcRect<int>& r) {
  switch (mRotation) {
    case hwcomposer::HWCRotation::kRotateNone: {
      r.left = lr.left.Phys(GetWidth());
      r.top = lr.top.Phys(GetHeight());
      r.right = lr.right.Phys(GetWidth());
      r.bottom = lr.bottom.Phys(GetHeight());
      break;
    }

    case hwcomposer::HWCRotation::kRotate90: {
      r.left = GetWidth() - lr.bottom.Phys(GetWidth());
      r.top = lr.left.Phys(GetHeight());
      r.right = GetWidth() - lr.top.Phys(GetWidth());
      r.bottom = lr.right.Phys(GetHeight());
      break;
    }

    case hwcomposer::HWCRotation::kRotate180: {
      r.left = GetWidth() - lr.right.Phys(GetWidth());
      r.top = GetHeight() - lr.bottom.Phys(GetHeight());
      r.right = GetWidth() - lr.left.Phys(GetWidth());
      r.bottom = GetHeight() - lr.top.Phys(GetHeight());
      break;
    }

    case hwcomposer::HWCRotation::kRotate270: {
      r.left = lr.top.Phys(GetWidth());
      r.top = GetHeight() - lr.right.Phys(GetHeight());
      r.right = lr.bottom.Phys(GetWidth());
      r.bottom = GetHeight() - lr.left.Phys(GetHeight());
      break;
    }

    default: {
      HWCERROR(eCheckFrameworkProgError, "Invalid Rotation %d", mRotation);
    }
  }

  mFmtCfgMgr->AdjustDisplayFrame(bufferFormat, r, GetWidth(), GetHeight());
}

void Hwch::Display::CopyRect(uint32_t bufferFormat, LogicalRect<int>& lr,
                             hwcomposer::HwcRect<int>& r) {
  r.left = lr.left.Phys(GetWidth());
  r.top = lr.top.Phys(GetHeight());
  r.right = lr.right.Phys(GetWidth());
  r.bottom = lr.bottom.Phys(GetHeight());

  mFmtCfgMgr->AdjustDisplayFrame(bufferFormat, r, GetWidth(), GetHeight());
}

// Creates an external buffer set that is suitable for use with the pOutBuf
void Hwch::Display::CreateExternalBufferSet(void) {
  // Create an Hwch::BufferSet if it has not been created yet
  if (mExternalBufferSet == NULL) {
    ALOG_ASSERT(Hwch::System::getInstance().GetVirtualDisplayWidth());
    ALOG_ASSERT(Hwch::System::getInstance().GetVirtualDisplayHeight());
    mExternalBufferSet =
        std::shared_ptr<BufferSet>(new BufferSet(Hwch::System::getInstance().GetVirtualDisplayWidth(),
                      Hwch::System::getInstance().GetVirtualDisplayHeight(),
                      HAL_PIXEL_FORMAT_RGBA_8888, HWCH_VIRTUAL_NUM_BUFFERS));
  }
}

// Returns the next buffer in the external (Virtual display) output buffer set.
HWCNativeHandle Hwch::Display::GetNextExternalBuffer(void) {
  HWCNativeHandle ret = NULL;

  // Create the Virtual Display buffer set, if not created already
  CreateExternalBufferSet();

  if (mExternalBufferSet != 0) {
    ret = mExternalBufferSet->GetNextBuffer();
    if (!ret) {
      HWCERROR(eCheckInternalError,
               "GetNextBuffer returned NULL when external (pOutBuf) buffer is "
               "required");
    }
  }

  return ret;
}

// Enables Virtual Display emulation on this display.
void Hwch::Display::EmulateVirtualDisplay(void) {
  mVirtualDisplay = true;
  mConnected = true;
}

// Returns whether Virtual Display emulation is enabled on this display.
bool Hwch::Display::IsVirtualDisplay(void) {
  return mVirtualDisplay;
}

bool Hwch::Display::GetHwcsHandle() {
  if (!mHwcsHandle) {
    // Attempt to connect to the new HWC Service Api
    mHwcsHandle = HwcService_Connect();

    if (!mHwcsHandle) {
      HWCERROR(eCheckSessionFail,
               "HWC Service Api could not connect to service");
      return false;
    }
  }

  return true;
}

uint32_t Hwch::Display::GetModes() {
  if (!GetHwcsHandle()) {
    HWCLOGE_COND(eLogHarness, "Handle to HWC Service is not setup!");
    return 0;
  }

  HwcService_DisplayMode_GetAvailableModes(mHwcsHandle, mDisplayIx, mModes);

  return mModes.size();
}

bool Hwch::Display::GetCurrentMode(Mode& mode) {
  if (!GetHwcsHandle()) {
    HWCLOGE_COND(eLogHarness, "Handle to HWC Service is not setup!");
    return false;
  }

  status_t st = HwcService_DisplayMode_GetMode(mHwcsHandle, mDisplayIx, &mode);

  return (st == 0);
}

bool Hwch::Display::GetCurrentMode(uint32_t& ix) {
  Hwch::Display::Mode mode;

  if (GetCurrentMode(mode)) {
    if (mModes.size() == 0) {
      GetModes();
      if (mModes.size() == 0) {
        return false;
      }
    }

    for (uint32_t i = 0; i < mModes.size(); ++i) {
      Hwch::Display::Mode testMode = mModes[i];
      // workaround gcc 4.8 bug? This was not compiling using operator== once I
      // declared additional templatized
      // operator== functions in HwchCoord.h. Hence change to a normal function.
      if (IsEqual(mode, testMode)) {
        ix = i;
        return true;
      }
    }
  }

  return false;
}

Hwch::Display::Mode Hwch::Display::GetMode(uint32_t ix) {
  ALOG_ASSERT(ix < mModes.size());

  return mModes[ix];
}

bool Hwch::Display::SetMode(uint32_t ix, int32_t delayUs) {
  ALOG_ASSERT(ix < mModes.size());
  const Mode& mode = mModes[ix];

  return SetMode(mode, delayUs);
}

bool Hwch::Display::SetMode(const Hwch::Display::Mode& mode, int32_t delayUs) {
  return true;
}

bool Hwch::Display::ClearMode() {
  return true;
}

bool Hwch::Display::HasScreenSizeChanged() {
  return ((mAttributes.width != mOldAttributes.width) ||
          (mAttributes.height != mOldAttributes.height));
}

void Hwch::Display::RecordScreenSize() {
  mOldAttributes = mAttributes;
}
