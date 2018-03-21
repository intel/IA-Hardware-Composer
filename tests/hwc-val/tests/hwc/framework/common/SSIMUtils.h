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

#define CHANS 3             // number of color channels
#define REGULAR_BLUR_RAY 1  // ray of the regular blur
#define TRANS_BLUR_RAY 4    // ray of the transposing blur
#define BYTES_PER_PIXEL 4   // self-explained
#define SIGMA 3             // sigma ~ gaussian radius
                            // the gaussian kernel length is (6*sigma-1)

static double gamma_lut[256];
static const double D65x = 0.9505, D65y = 1.0, D65z = 1.089;

typedef void rowcallback(float *, const int width);

typedef struct { unsigned char r, g, b, a; } dssim_rgba;

typedef struct { float l, A, b, a; } laba;

typedef struct {
  int width, height;
  float *img1, *mu1, *sigma1_sq;
  float *img2, *mu2, *sigma2_sq, *sigma12;
} dssim_info_chan;

struct dssim_info {
  dssim_info_chan chan[CHANS];
};

enum BlurType { ebtLinear, ebtGaussian };

void DoSSIMCalculations(dssim_info *inf, dssim_rgba *Bufrow_pointers[],
                        dssim_rgba *Refrow_pointers[], const int width,
                        const int height, const int blur_type, bool hasAlpha);

double GetSSIMIndex(dssim_info_chan *chan);
