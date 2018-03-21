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

#include "HwchInternalTests.h"
#include "HwchPngImage.h"
#include "HwchLayers.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"
#include "SSIMUtils.h"

REGISTER_TEST(SSIMCompare)
Hwch::SSIMCompareTest::SSIMCompareTest(Hwch::Interface &interface)
    : Hwch::Test(interface) {
}

int Hwch::SSIMCompareTest::RunScenario() {
  const char *filename1 = "SSIM_refimage_1.png";
  const char *filename2 = "SSIM_refimage_2.png";
  int blur_type = ebtLinear;

  if (!strcmp(GetStrParam("blur", "linear"), "gaussian")) {
    blur_type = ebtGaussian;
  }

  // load png images

  PngImage *pngimage1 = new Hwch::PngImage();
  if (!pngimage1->ReadPngFile(filename1)) {
    HWCERROR(eCheckTestFail, "Failed reading input png file\n");
    return 1;
  }

  PngImage *pngimage2 = new Hwch::PngImage();
  if (!pngimage2->ReadPngFile(filename2)) {
    HWCERROR(eCheckTestFail, "Failed reading input png file\n");
    return 1;
  }

  uint32_t image_width = pngimage1->GetWidth();
  uint32_t image_height = pngimage1->GetHeight();

  if ((image_width != pngimage2->GetWidth()) ||
      (image_height != pngimage2->GetHeight())) {
    HWCERROR(eCheckTestFail, "The two images are different in size. Exit.");
    return 1;
  }

  // SSIM preliminary calculations

  double SSIMIndex = 0;
  dssim_info *dinf = new dssim_info();

  DoSSIMCalculations(dinf, (dssim_rgba **)pngimage1->GetRowPointers(),
                     (dssim_rgba **)pngimage2->GetRowPointers(), image_width,
                     image_height, blur_type);

  // calculate SSIM index averaged on channels

  for (int ch = 0; ch < CHANS; ch++) {
    SSIMIndex += GetSSIMIndex(&dinf->chan[ch]);
  }
  SSIMIndex /= (double)CHANS;

  printf("%s SSIM index = %.6f\n", __FUNCTION__, SSIMIndex);

  // deallocations

  delete dinf;
  delete pngimage1;
  delete pngimage2;

  return 0;
}
