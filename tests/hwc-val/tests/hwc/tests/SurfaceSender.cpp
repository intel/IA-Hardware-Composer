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

/**
 *  \mainpage Hwc Test Documentation
 *
 *  Author: Kelvin Gardiner
 *
 *  Version: 1.0
 *
 * \ref TestParts
 *
 * \ref OutstandingIssues
 *
 * \ref Execution
 *
 */

/** \page TestParts Test Parts
 *
 *  \ref UseCaseTests
 *
 */

/** \defgroup UseCaseTests Use Case Test
 *  @{
 *  \brief Tests which replicate surfaces of common use cases
 *  @}
 */

/**
 *  \defgroup OutstandingIssues Outstanding Issues
 *  @{
 *  \brief This page lists the current issues which are blocking test
 *  development.
 *  @}
 */

/**
   \page Execution Execution

   \section TestEnvironment Test Environment

   \subsection SystemSetup System Setup


   \subsection RunnigATest Running A Test (expect the monitor test):

    See tests/run_hwc_test.sh for commands to do the following.

   -# stop
   -# Follow the sets in \ref FrameworkDetails section Using The Framework
   -# start
   -# setprop service.bootanim.exit 1 // stop the boot animation
   -# Run the test binary

   If the shims are in place the test can ran on a running
   android system. But the surfaces created by the test
   may not be seen ans android surfaces may be displayed
   in front of them. Additional not just the test surfaces
   will getting send to hwc.

   \subsection RunningTheMonitorTest Running The Monitor Test

   The monitor test runs on all shim checks and runs for a
   set period. It is designed for using the framework on a
   when running android normally to help debug an issue.
   and to help debug the shims. To run the monitor test run the following
   commands:

   -# stop
   -# Follow the sets in \ref FrameworkDetails section Using The Framework
   -# start
   -# Run the test binary

   \section CommandLineOptions Command Line Options


   \section FurtherInformation Further Information
    Details of how the framework work can be of here \ref FrameworkDetails.

    A understanding of this is not needed to execute the tests.

    \ref FutureWork

 */

/** \defgroup FutureWork Future Work
 *  @{
 *  Add a tests the produce surface large than the screen.
 *  This will require a surface sender change to change the viewable area.
 *  @}
 */

/** \defgroup FutureWork Future Work
 *  @{
 *  A stress test is need to create a set of surface and destroy then after some
 *  amount of time.
 *  Create a "display set" class to hold the surface properties and display time
 *  for the set.
 *  Modify base class to iterate of these set during the test.
 *  @}
 */

/** \defgroup FutureWork Future Work
 *  @{
 *  The current surfaces are a single solid color. Some Android surface seen on
 *  common use cases are a full screen surface with only an opaque area which is
 *  not the full surface size.
 *
 *  Surface sender need to be extended to allow only partial shading of a
 *  surface.
 *  @}
 */

#undef LOG_TAG
#define LOG_TAG "VAL_HWC_SURFACE_SENDER"

#include "SurfaceSender.h"
#include "HwcTestLog.h"
#include "HwcTestUtil.h"  // for sync headers

void dumpFence(const char *label, const char *surfaceName, int fence) {
  if (fence != -1) {
#if 0  
      sync_fence_info_data *pf = sync_fence_info(fence);
        struct sync_pt_info *ps;
        int i = 0;

        if(pf)
        {
            HWCLOGI("%s(%s) - frame fence(%d) sync name(%s) status(%d) pt(%d)",
                    label, surfaceName, fence, pf->name, pf->status, pf->pt_info[0]);

            for(ps = sync_pt_info(pf, NULL); ps; ps = sync_pt_info(pf, ps))
            {
                float seconds = float(ps->timestamp_ns) * (1/1000000000.0f);

                HWCLOGI("%s(%s) - fence(%d) syncPoint[%d] = driver(%s) status(%d) timestamp(%.03fs)",
                        label, surfaceName, fence, i,
                        ps->driver_name, ps->status, seconds);
                ++i;
            }
            sync_fence_info_free(pf);
        }
        else
#endif
    {
      // if we've got a fence fd, we should be able to get its info???
      HWCLOGW("%s - frame fence %d: can't get more info", label, fence);
    }
  }
}

// SurfaceSenderProperties constructors

SurfaceSender::SurfaceSenderProperties::SurfaceSenderProperties() {
  mUseScreenWidth = true;
  mUseScreenHeight = true;
  mHeight = 0;
  mWidth = 0;

  mXOffset = 0;
  mYOffset = 0;

  mLayer = epsBackground;

  mColorSpace = ecsRGBA;
  mRGBAColor = ecRed;

  mSurfaceName = (char *)"Default surface";

  mFps = 1000;
  mFpsThreshold = 60;
}

SurfaceSender::SurfaceSenderProperties::SurfaceSenderProperties(
    ePredefinedSurface surface) {
  // Surfaces requiring full width
  switch (surface) {
    case epsBackground:
    case epsStaticBackground:
    case epsWallpaper:
    case epsLauncher:
    case epsNavigationBar:
    case epsStatusBar:
    case epsRecentAppsPanel:
    case epsKeyGuard:
    case epsGallerySurface:
    case epsGalleryUI:
    case epsGameSurfaceFullScreen:
    case epsCameraSurface:
    case epsCameraUI:
    case epsVideoFullScreenNV12:
      mUseScreenWidth = true;
      break;
    default:
      mUseScreenWidth = false;
  }

  // Surfaces requiring full height
  switch (surface) {
    case epsBackground:
    case epsStaticBackground:
    case epsWallpaper:
    case epsVideoFullScreenNV12:
      mUseScreenHeight = true;
      break;
    default:
      mUseScreenHeight = false;
  }

  // Default values for all surfaces

  mHeight = 0;
  mWidth = 0;

  mXOffset = 0;
  mYOffset = 0;

  mLayer = epsBackground;

  mFps = 1000;  // As fast as possible

  mColorSpace = ecsRGBA;

  mFpsThreshold = 59;

  // Properties unique to each surfaces
  // Give each predefined surface a unique color
  // Set a layer order to mimic Android
  // TODO layers a in order of surface in switch statement as the
  // order if not listed in the HLD use dumpsys to check these.
  switch (surface) {
    case epsBackground:
      mRGBAColor = ecBlue;
      mLayer = epsBackground;
      mFps = 1;  // Background rarely needs an update
      mSurfaceName = (char *)"epsBackground";
      break;

    case epsStaticBackground:
      mRGBAColor = ecBlue;
      mLayer = epsStaticBackground;
      mFps = 1;  // Background rarely needs an update
      mSurfaceName = (char *)"epsStaticBackground";
      break;

    case epsWallpaper:
      mRGBAColor = ecGreen;
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mYOffset = eassStatusBarHeight + 1;
      mLayer = epsWallpaper;
      mFps = 1;  // Wallpaper rarely needs an update (unless it's animated)
      mSurfaceName = (char *)"epsWallpaper";
      break;

    case epsKeyGuard:
      mRGBAColor = ecRed;
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mYOffset = eassStatusBarHeight + 1;
      mLayer = epsKeyGuard;
      mSurfaceName = (char *)"epsKeyGuard";
      break;

    case epsNavigationBar:
      mColorSpace = ecsRGB565;
      mRGBAColor = ecBlue;
      mHeight = eassNavigationBarHeight;
      mYOffset = eassTODOScreenHeight - mHeight;
      mLayer = epsNavigationBar;
      mFps = 10;  // updating, but not continuously
      mSurfaceName = (char *)"epsNavigationBar";
      break;

    case epsStatusBar:
      // TODO RGB buffer not working
      mColorSpace = ecsRGBA;
      mRGBAColor = ecWhite;
      mHeight = eassStatusBarHeight;
      mLayer = epsStatusBar;
      mFps = 1;  // 1 update per second
      mSurfaceName = (char *)"epsStatusBar";
      break;

    case epsLauncher:
      mRGBAColor = ecCyanAlpha;
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mYOffset = eassStatusBarHeight + 1;
      mLayer = epsLauncher;
      mFps = 2;  // Not much updating
      mSurfaceName = (char *)"epsLauncher";
      break;

    case epsNotificationPanel:
      mRGBAColor = ecPurple;
      mWidth = 512;
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mXOffset = eassTODOScreenWidth - mWidth;
      mYOffset = eassStatusBarHeight + 1;
      mLayer = epsNotificationPanel;
      mFps = 2;  // Not much updating
      mSurfaceName = (char *)"epsNotificationPanel";
      break;

    case epsRecentAppsPanel:
      mRGBAColor = ecGrey;
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mYOffset = eassStatusBarHeight + 1;
      mLayer = epsRecentAppsPanel;
      mSurfaceName = (char *)"epsRecentPanel";
      break;

    case epsDialogBox:
      mRGBAColor = ecLightRedAlpha;
      mWidth = 401;
      mHeight = 112;
      mXOffset = 759;
      mYOffset = 460;
      mLayer = epsDialogBox;
      mSurfaceName = (char *)"epsDialogBox";
      break;

    case epsGameSurfaceFullScreen:
      mColorSpace = ecsRGB565;
      mRGBAColor = ecLightGreen;
      mHeight = eassTODOScreenHeight - eassNavigationBarHeight;
      mLayer = epsGameSurfaceFullScreen;
      mSurfaceName = (char *)"epsGameSurfaceFullScreen";
      break;

    case epsAdvertPane:
      mRGBAColor = ecLightBlue;
      mWidth = 400;
      mHeight = 112;
      mXOffset = (eassTODOScreenWidth / 2) - (mWidth / 2);
      mYOffset = eassTODOScreenHeight - eassNavigationBarHeight - mHeight;
      mLayer = epsAdvertPane;
      mSurfaceName = (char *)"epsAdvertPane";
      break;

    case epsMediaUI:
      mRGBAColor = ecLightCyan;
      mLayer = 26009;
      mSurfaceName = (char *)"epsMediaUI";
      break;

    // TODO no camera on the board to check the surfaces
    case epsCameraSurface:
      mColorSpace = ecsYCbCr422i;
      mRGBAColor = ecLightPurple;
      mWidth = eassTODOScreenHeight - 260;  // 260 is camera UI width
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mXOffset = 0;
      mYOffset = 0;
      mLayer = epsCameraSurface;
      mSurfaceName = (char *)"epsCameraSurface";
      break;

    case epsCameraUI:
      mRGBAColor = ecDarkPurple;
      mWidth = 260;
      mHeight = eassTODOScreenHeight - eassStatusBarHeight;
      mXOffset = eassTODOScreenWidth - mWidth;
      mYOffset = 0;
      mLayer = epsCameraUI;
      mFps = 2;  // Not much updating
      mSurfaceName = (char *)"epsCameraUI";
      break;

    case epsSkype:
      mRGBAColor = ecLightGrey;
      mLayer = epsSkype;
      mSurfaceName = (char *)"epsSkype";
      break;

    case epsMenu:
      mRGBAColor = ecDarkRed;
      mWidth = 220;
      mHeight = 260;
      mXOffset = eassTODOScreenWidth - mWidth;
      mYOffset = eassStatusBarHeight + 1;
      mLayer = epsMenu;
      mFps = 2;  // Not much updating
      mSurfaceName = (char *)"epsMenu";
      break;

    case epsGallerySurface:
      mRGBAColor = ecDarkGreen;
      mHeight = eassTODOScreenHeight - eassNavigationBarHeight;
      mLayer = epsGallerySurface;
      mSurfaceName = (char *)"epsGallerySurface";
      break;

    case epsGalleryUI:
      mRGBAColor = ecLightCyan;
      mHeight = 40;
      mLayer = epsGalleryUI;
      mSurfaceName = (char *)"epsGalleryUI";
      break;

    case epsVideoFullScreenNV12:
      mRGBAColor = ecDarkBlue;
      mColorSpace = ecsNV12YTiledIntel;
      mLayer = epsVideoFullScreenNV12;
      mSurfaceName = (char *)"epsVideoFullScreenNV12";
      break;

    case epsVideoPartScreenNV12:
      mRGBAColor = ecDarkBlue;
      mWidth = 220;
      mHeight = 260;
      mXOffset = 759;
      mYOffset = 460;
      mColorSpace = ecsNV12YTiledIntel;
      mLayer = epsVideoPartScreenNV12;
      mSurfaceName = (char *)"epsVideoPartScreenNV12";
      break;

    default:
      HWCERROR(eCheckSurfaceSender, "Request for unknown predefined surface");
  }
  mFpsThreshold = min(mFps, mFpsThreshold);
}

uint32_t SurfaceSender::GetPixelBytes(SPixelWord &pixel) {
  uint32_t result = 0;

  HWCLOGI("%s: %s \n\t color: %x colorspace: %d", __FUNCTION__,
          mProps.mSurfaceName, mProps.mRGBAColor, mProps.mColorSpace);

  if (mProps.mColorSpace == ecsRGBA || mProps.mColorSpace == ecsRGBX) {
    pixel.bytes[0] = ((mProps.mRGBAColor >> 24) & 0xFF);
    pixel.bytes[1] = ((mProps.mRGBAColor >> 16) & 0xFF);
    pixel.bytes[2] = ((mProps.mRGBAColor >> 8) & 0xFF);
    pixel.bytes[3] = (mProps.mRGBAColor & 0xFF);

    HWCLOGI("\t bytes[0-3]: %x %x %x %x", pixel.bytes[0], pixel.bytes[1],
            pixel.bytes[2], pixel.bytes[3]);
  } else if (mProps.mColorSpace == ecsRGB565) {
    uint32_t red = ((mProps.mRGBAColor >> 24) & 0xFF) / 255 * 31;
    uint32_t green = ((mProps.mRGBAColor >> 16) & 0xFF) / 255 * 63;
    uint32_t blue = ((mProps.mRGBAColor >> 8) & 0xFF) / 255 * 31;

    pixel.bytes[0] = ((green & 3) << 5) | blue;
    pixel.bytes[1] = red << 3 | ((green >> 3) & 3);

    HWCLOGI("\t bytes[0-1]: %x %x", pixel.bytes[0], pixel.bytes[1]);
  } else if (mProps.mColorSpace == ecsRGB) {
    pixel.bytes[0] = ((mProps.mRGBAColor >> 24) & 0xFF);
    pixel.bytes[1] = ((mProps.mRGBAColor >> 16) & 0xFF);
    pixel.bytes[2] = ((mProps.mRGBAColor >> 8) & 0xFF);

    HWCLOGI("\t bytes[0-2]: %x %x %x", pixel.bytes[0], pixel.bytes[1],
            pixel.bytes[2]);
  } else if (mProps.mColorSpace == ecsNV12YTiledIntel) {
    uint32_t R = ((mProps.mRGBAColor >> 24) & 0xFF);
    uint32_t G = ((mProps.mRGBAColor >> 16) & 0xFF);
    uint32_t B = ((mProps.mRGBAColor >> 8) & 0xFF);
    pixel.bytes[0] = ((65 * R + 128 * G + 24 * B + 128) >> 8) + 16;   // Y
    pixel.v = (((112 * R - 93 * G - 18 * B + 128) >> 8) + 128) << 8;  // V-Cr
    pixel.u = (((-37 * R - 74 * G + 112 * B + 128) >> 8) + 128);      // U-Cb

    HWCLOGI("\t Y: %x V-Cr: %x U-Cb: %x", pixel.bytes[0], pixel.v, pixel.u);
  } else if (mProps.mColorSpace == ecsYCbCr422i) {
    // RGB888
    int32_t R = ((mProps.mRGBAColor >> 24) & 0xFF);
    int32_t G = ((mProps.mRGBAColor >> 16) & 0xFF);
    int32_t B = ((mProps.mRGBAColor >> 8) & 0xFF);

    // YUV444
    int32_t Y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
    int32_t U = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
    int32_t V = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;

    // YUV422i: U0 Y0 V0 Y1
    pixel.bytes[0] = U & 0xFF;
    pixel.bytes[1] = Y & 0xFF;
    pixel.bytes[2] = V & 0xFF;
    pixel.bytes[3] = Y & 0xFF;
  } else {
    HWCERROR(eCheckSurfaceSender, "Color Space %d not supported yet",
             mProps.mColorSpace);
    result = 1;
  }

  HWCLOGI("\t result: %u", result);

  return result;
}

/// Constructor create surface with given properties
SurfaceSender::SurfaceSender(SurfaceSender::SurfaceSenderProperties &in)
    : mProps(in) {
  Display displayInfo = Display();

  if (in.mUseScreenHeight) {
    mProps.mHeight = displayInfo.GetHeight();
  }

  if (in.mUseScreenWidth) {
    mProps.mWidth = displayInfo.GetWidth();
  }

  // State
  mCount = 0;
  mFrameNumber = 0;
  mLine = 0;
  mBufferLine.emplace((buffer_handle_t)-1, (uint32_t)0);
  calculatePeriod();

  // DEBUG
  // HWCLOGI("surface %s color %x", in.mSurfaceName, (uint32_t)in.mRGBAColor);
}

// Default constructor
SurfaceSender::SurfaceSender() {
  // Config
  mProps.mColorSpace = ecsRGBA;
  mProps.mRGBAColor = ecWhite;
  mProps.mSurfaceName = "-1";
  mProps.mXOffset = 0;
  mProps.mHeight = 100;
  mProps.mWidth = 100;
  mProps.mFps = 60;
  calculatePeriod();

  // State
  mCount = 0;
  mFrameNumber = 0;
  mLine = 0;
  mBufferLine.emplace((buffer_handle_t)-1, (uint32_t)0);
}

SurfaceSender::~SurfaceSender() {
  mBufferLine.clear();
}

void SurfaceSender::calculatePeriod() {
  mTargetFramePeriod = 1000000000ull / mProps.mFps;
  HWCLOGI("Surface %s Target frame period %lld", mProps.mSurfaceName,
          mTargetFramePeriod);
  mAllowedFramePeriod = 1000000000ull / mProps.mFpsThreshold;
  mLineJumpPixels = min(max(60 / mProps.mFps, 1U), 8U);

  mNextUpdateTime = systemTime(SYSTEM_TIME_MONOTONIC);
}

void SurfaceSender::calculateTargetUpdateTime() {
  int64_t allowedUpdateTime = mNextUpdateTime + mAllowedFramePeriod;
  mNextUpdateTime += mTargetFramePeriod;
  if (mNextUpdateTime <= systemTime(SYSTEM_TIME_MONOTONIC)) {
    if (allowedUpdateTime <= systemTime(SYSTEM_TIME_MONOTONIC)) {
      HWCERROR(eCheckSurfaceSender, "Surface %s missed frame update\n",
               mProps.mSurfaceName);
    }
    mNextUpdateTime = systemTime(SYSTEM_TIME_MONOTONIC) + mTargetFramePeriod;
  }
  // HWCLOGI("Next update time for surface %s is %lld", mProps.mSurfaceName,
  // mNextUpdateTime);
}

bool SurfaceSender::Start() {
  HWCLOGI("Starting Sender %s", mProps.mSurfaceName);

  // get handle for gralloc module
  // TODO: Use GrallocBufferMapper class.

  // create surface and native window via SurfaceFlinger client
  mClient = new SurfaceComposerClient();
  mSurfaceControl =
      mClient->createSurface(String8((const char *)mProps.mSurfaceName),
                             mProps.mWidth, mProps.mHeight, ecsRGB, 0);
  mSurface = mSurfaceControl->getSurface().get();
  mWindow = mSurface.get();

  // set z-index and position
  SurfaceComposerClient::openGlobalTransaction();
  mSurfaceControl->setLayer(mProps.mLayer);
  mSurfaceControl->setPosition(mProps.mXOffset, mProps.mYOffset);
  SurfaceComposerClient::closeGlobalTransaction();

  // set surface buffers color format
  if (native_window_set_buffers_format(mWindow, mProps.mColorSpace) ==
      -ENOENT) {
    HWCERROR(eCheckSurfaceSender, "native window set buffer format failed.");
  }

  const uint32_t bc = 3;
  int err = native_window_set_buffer_count(mWindow, bc);
  if (err < 0) {
    HWCERROR(eCheckSurfaceSender,
             "SurfaceSender::Start(%s) - failed to set buffer count to %d",
             mProps.mSurfaceName, bc);
  } else {
    HWCERROR(eCheckSurfaceSender,
             "SurfaceSender::Start(%s) - set buffer count to %d",
             mProps.mSurfaceName, bc);
  }

  // set pixel format
  switch (mProps.mColorSpace) {
    case ecsRGBA:
    case ecsRGBX:
      mBytesPerPixel = 4;
      break;
    case ecsRGB:
      mBytesPerPixel = 3;
      break;
    case ecsRGB565:
    case ecsYCbCr422i:
      mBytesPerPixel = 2;
      break;
    default:
      mBytesPerPixel = 1;
      break;
  }

  mBackgroundPixel.word32 = 0;
  mForegroundPixel.word32 = 0;

  HWCLOGI(
      "SurfaceSender %s: surface size =%dx%d colour %x\n"
      "layer %d, xoffset: %d, yoffset %d, bpp %d\n",
      mProps.mSurfaceName, mProps.mWidth, mProps.mHeight, mProps.mRGBAColor,
      mProps.mLayer, mProps.mXOffset, mProps.mYOffset, mBytesPerPixel);

  GetPixelBytes(mBackgroundPixel);

  // copy value on the first byte into the next three bytes to be able to
  // to loop on a whole word during drawing process
  if (mProps.mColorSpace == ecsYCbCr422i) {
    mForegroundPixel.word32 = mBackgroundPixel.word32 ^ 0x00ff00ff;
  } else {
    switch (mBytesPerPixel) {
      case 1:
        mBackgroundPixel.bytes[3] = mBackgroundPixel.bytes[2] =
            mBackgroundPixel.bytes[1] = mBackgroundPixel.bytes[0];
        mForegroundPixel.bytes[3] = mForegroundPixel.bytes[2] =
            mForegroundPixel.bytes[1] = mForegroundPixel.bytes[0] =
                ~mBackgroundPixel.bytes[0];
        break;

      case 2:
        mBackgroundPixel.word16[1] = mBackgroundPixel.word16[0];
        mForegroundPixel.word16[1] = mForegroundPixel.word16[0] =
            ~mBackgroundPixel.word16[0];
        break;

      default:  // 3 or 4 bytes
        mForegroundPixel.word32 =
            mBackgroundPixel.word32 ^
            0xffffff;  // Don't invert alpha which is in byte 3
        break;
    }

    mBackgroundPixel.chroma |= (mBackgroundPixel.chroma << 16);
    mForegroundPixel.chroma |= (mForegroundPixel.chroma << 16);
  }

  if (mWindow->dequeueBuffer(mWindow, &mBuffer, &mFence)) {
    HWCLOGE("SurfaceSender::Start - Buffer acquisition failed");
    return false;
  }

  if (mFence != -1) {
    // we've a valid fence, wait for it to be signalled
    err = sync_wait(mFence, 5000);
    if (err < 0) {
      HWCERROR(
          eCheckSurfaceSender,
          "SurfaceSender::Start(%s) - ERROR(%d): fence(%d) NEVER SIGNALLED",
          mProps.mSurfaceName, err, mFence);
      dumpFence("SurfaceSender::Start", mProps.mSurfaceName, mFence);
    }

    // the caller is responsible for closing the fence, see
    // $ANDROID_BUILD_TOP/system/core/include/system/window.h, dequeueBuffer()
    close(mFence);
    mFence = -1;
  }

  // initialise the lines
  //
  uint32_t stride = mBuffer->stride * mBytesPerPixel;

  mpBackgroundLine = new unsigned char[stride];
  mpForegroundLine = new unsigned char[stride];

  memset(mpBackgroundLine, 0, stride);
  memset(mpForegroundLine, 0, stride);

  WritePixels(mpBackgroundLine, mBackgroundPixel, mBuffer->width);
  WritePixels(mpForegroundLine, mForegroundPixel, mBuffer->width);

  if (mProps.mColorSpace == ecsNV12YTiledIntel) {
    mpBackgroundChromaNV12 = new unsigned char[stride];
    mpForegroundChromaNV12 = new unsigned char[stride];

    memset(mpBackgroundChromaNV12, 0, stride);
    memset(mpForegroundChromaNV12, 0, stride);

    WriteNV12Chroma(mpBackgroundChromaNV12, mBackgroundPixel.chroma,
                    mBuffer->width);
    WriteNV12Chroma(mpForegroundChromaNV12, mForegroundPixel.chroma,
                    mBuffer->width);
  } else {
    mpBackgroundChromaNV12 = NULL;
    mpForegroundChromaNV12 = NULL;
  }

  void *dstPtr;

  GraphicBuffer *graphBuf = static_cast<GraphicBuffer *>(mBuffer);
  graphBuf->lock(GRALLOC_USAGE_SW_WRITE_MASK,
                       &dstPtr);
  if (err) {
    HWCERROR(eCheckSurfaceSender,
             "SurfaceSender::Start - Gralloc lock failed with err = %d", err);
  } else {
    unsigned char *ptr = (unsigned char *)dstPtr;
    FillBufferBackground(ptr);
    graphBuf->unlock();
  }

  mWindow->queueBuffer(mWindow, mBuffer, mFence);
  return true;
}

void SurfaceSender::WritePixels(unsigned char *ptr, SPixelWord &currentPixel,
                                unsigned numPixels) {
  switch (mBytesPerPixel) {
    case 4:
      for (uint32_t px = 0; px < numPixels; ++px) {
        *((uint32_t *)ptr) = currentPixel.word32;
        ptr += 4;
      }
      break;

    case 3:
      for (uint32_t px = 0; px < numPixels; ++px) {
        *ptr++ = currentPixel.bytes[0];
        *ptr++ = currentPixel.bytes[1];
        *ptr++ = currentPixel.bytes[2];
      }
      break;

    case 2:
      for (uint32_t px = 0; px < numPixels; px += 2) {
        *((uint32_t *)ptr) = currentPixel.word32;
        ptr += 4;
      }
      break;

    case 1:
      for (uint32_t px = 0; px < numPixels; px += 4) {
        *((uint32_t *)ptr) = currentPixel.word32;
        ptr += 4;
      }
      break;
  }
}

void SurfaceSender::WriteNV12Chroma(unsigned char *ptr, uint32_t chroma,
                                    unsigned numPixels) {
  numPixels >>= 2;
  for (uint32_t px = 0; px < numPixels; ++px) {
    *((uint32_t *)ptr) = chroma;
    ptr += 4;
  }
}

bool SurfaceSender::Iterate() {
  if (systemTime(SYSTEM_TIME_MONOTONIC) > mNextUpdateTime) {
    PreFrame();
    Frame();
    if (!PostFrame()) {
      return false;
    }
  }
  return true;
}

void SurfaceSender::PreFrame() {
  // printf("%04d: PreFrame -- %s\n", int((android::elapsedRealtime() / 1000) %
  // 20), mProps.mSurfaceName);
  if (mWindow->dequeueBuffer(mWindow, &mBuffer, &mFence)) {
    HWCERROR(eCheckSurfaceSender, "Buffer acquisition failed");
  }
  dumpFence("SurfaceSender::PreFrame", mProps.mSurfaceName, mFence);
}

bool SurfaceSender::PostFrame() {
  // printf("%04d: PostFrame - %s\n\n", int((android::elapsedRealtime() / 1000)
  // % 20), mProps.mSurfaceName);
  if (mWindow->queueBuffer(mWindow, mBuffer, mFence)) {
    HWCERROR(eCheckSurfaceSender,
             "SurfaceSender::PostFrame - Buffer unlock and post failed");
    return false;
  }
  return true;
}

bool SurfaceSender::Frame() {
  // printf("%04d: Frame ----- %s\n", int((android::elapsedRealtime() / 1000) %
  // 20), mProps.mSurfaceName);
  calculateTargetUpdateTime();

  int err = 0;
  uint32_t surfaceWidth = mBuffer->width;
  uint32_t surfaceHeight = mBuffer->height;

  if (mFence != -1) {
    // we've a valid fence, wait for it to be signalled
    err = sync_wait(mFence, 5000);
    if (err < 0) {
      HWCERROR(
          eCheckSurfaceSender,
          "SurfaceSender::Frame(%s) - ERROR(%d): fence(%d) NEVER SIGNALLED",
          mProps.mSurfaceName, err, mFence);
      dumpFence("SurfaceSender::Frame", mProps.mSurfaceName, mFence);
    }

    // the caller is responsible for closing the fence, see
    // $ANDROID_BUILD_TOP/system/core/include/system/window.h, dequeueBuffer()
    close(mFence);
    mFence = -1;
  }

  unsigned char *dstPtr = 0;
  GraphicBuffer *graphBuf = static_cast<GraphicBuffer *>(mBuffer);

  graphBuf->lock(GRALLOC_USAGE_SW_WRITE_MASK, (void **)&dstPtr);
 
  if (err) {
    HWCERROR(eCheckSurfaceSender, "Gralloc lock failed with err = %d", err);
  }

  uint32_t lastLine = mBufferLine[mBuffer->handle];

  if (lastLine == uint32_t(-1)) {
    // add the line number of the foreground line (we draw it below)
    mBufferLine.emplace(mBuffer->handle, mLine);

    // not written to this buffer yet, fill the entire buffer
    FillBufferBackground(dstPtr);
  } else if (mProps.mLayer != epsStaticBackground) {
    uint32_t endLine = min(surfaceHeight, lastLine + LINE_THICKNESS);

    // edit the line number of the foreground line (we draw it below)
    mBufferLine[mBuffer->handle] =  mLine;

    // draw over the previous foreground line
    if (mProps.mColorSpace == ecsNV12YTiledIntel) {
      for (uint32_t line = lastLine; line < endLine; ++line) {
        DrawLineNV12(line, dstPtr, mpBackgroundLine, mpBackgroundChromaNV12);
      }
    } else {
      for (uint32_t line = lastLine; line < endLine; ++line) {
        DrawLine(line, dstPtr, mpBackgroundLine);
      }
    }
  }

  if (mProps.mLayer != epsStaticBackground) {
    uint32_t endLine = min(surfaceHeight, mLine + LINE_THICKNESS);

    // draw the foreground line
    if (mProps.mColorSpace == ecsNV12YTiledIntel) {
      for (uint32_t line = mLine; line < endLine; ++line) {
        DrawLineNV12(line, dstPtr, mpForegroundLine, mpForegroundChromaNV12);
      }
    } else {
      for (uint32_t line = mLine; line < endLine; ++line) {
        DrawLine(line, dstPtr, mpForegroundLine);
      }
    }
  }

  mLine += mLineJumpPixels;

  if (mLine > (surfaceHeight - 2)) {
    mLine = 0;
  }

  graphBuf->unlock();
  return true;
}

void SurfaceSender::DrawLine(unsigned lineNum, unsigned char *pBfr,
                             unsigned char *pLineSrc) {
  uint32_t stride = mBuffer->stride * mBytesPerPixel;
  unsigned char *pLine;

  pLine = pBfr + lineNum * stride;
  memcpy(pLine, pLineSrc, mBuffer->width * mBytesPerPixel);
}

void SurfaceSender::DrawLineNV12(unsigned lineNum, unsigned char *pBfr,
                                 unsigned char *pLineSrc,
                                 unsigned char *pNV12ChromaSrc) {
  uint32_t surfaceHeight = mBuffer->height;
  uint32_t stride = mBuffer->stride * mBytesPerPixel;
  unsigned char *pLine;

  // copy luminance
  pLine = pBfr + lineNum * stride;
  memcpy(pLine, pLineSrc, stride);

  // copy chroma
  pBfr += surfaceHeight * stride;
  pLine = pBfr + (lineNum >> 1) * stride;
  memcpy(pLine, pNV12ChromaSrc, stride);
}

void SurfaceSender::FillBufferBackground(unsigned char *pBfr) {
  uint32_t surfaceHeight = mBuffer->height;

  if (mProps.mColorSpace == ecsNV12YTiledIntel) {
    for (uint32_t row = 0; row < surfaceHeight; ++row) {
      DrawLineNV12(row, pBfr, mpBackgroundLine, mpBackgroundChromaNV12);
    }
  } else {
    for (uint32_t row = 0; row < surfaceHeight; ++row) {
      DrawLine(row, pBfr, mpBackgroundLine);
    }
  }
}

bool SurfaceSender::End() {
  HWCLOGI("SurfaceSender::End - %s", mProps.mSurfaceName);
  delete[] mpBackgroundLine;
  delete[] mpForegroundLine;
  delete[] mpBackgroundChromaNV12;
  delete[] mpForegroundChromaNV12;

  mpBackgroundLine = mpForegroundLine = mpBackgroundChromaNV12 =
      mpForegroundChromaNV12 = NULL;
  return true;
}
