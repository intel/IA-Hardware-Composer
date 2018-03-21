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

#define LOG_TAG "HWC_TEST"

#include <cutils/memory.h>

#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include <unistd.h>

#include "test_base.h"

/** \addtogroup HwcTestNV12FullScreenVideo NV12 Full ScreenVideo
    \ingroup UseCaseTests
    @{
        \brief Create a surface to mimic full screen video in NV12 format.
    @}
*/


class HwcNv12vpTestTest : public HwcTestBase {
 public:
  // Constructor
  HwcNv12vpTestTest(int argc, char** argv);

  /// Create surfaces and start test
  int Run(void);
  /// Set checks required by the shims
  int SetChecks(void);
};

HwcNv12vpTestTest::HwcNv12vpTestTest(int argc, char** argv)
    : HwcTestBase(argc, argv) {
  mTestName = "hwc_nv12_video_part_test";
}

int HwcNv12vpTestTest::SetChecks(void) {
  SetDefaultChecks();
  return 0;
}

int HwcNv12vpTestTest::Run(void) {
  SurfaceSender::SurfaceSenderProperties sSSP1(SurfaceSender::epsWallpaper);
  CreateSurface(sSSP1);

  SurfaceSender::SurfaceSenderProperties sSSP2(SurfaceSender::epsLauncher);
  CreateSurface(sSSP2);

  // this corresponds to the SurfaceView layer in SurfaceFlinger dumpsys
  SurfaceSender::SurfaceSenderProperties sSSP3(
      SurfaceSender::epsVideoPartScreenNV12);
  CreateSurface(sSSP3);

  SurfaceSender::SurfaceSenderProperties sSSP4(SurfaceSender::epsStatusBar);
  CreateSurface(sSSP4);

  SurfaceSender::SurfaceSenderProperties sSSP5(SurfaceSender::epsNavigationBar);
  CreateSurface(sSSP5);

  // Set test mode frame or time
  SetTestRunTime(HwcTestBase::etlTenSeconds);
  SetTestEndType(etetRunTime);

  StartTest();

  return mResult.IsGlobalFail() ? 1 : 0;
}

int main(int argc, char** argv) {
  HwcNv12vpTestTest test(argc, argv);

  if (argc == 2 && strcmp(argv[1], "-h") == 0) {
    test.PrintArgs();
    return 1;
  }
  return test.Run();
}
