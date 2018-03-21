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

#include <cutils/log.h>
#include <math.h>

#include "HwcTestState.h"
#include "HwcTestUtil.h"
#include "SSIMUtils.h"

typedef void rowcallback(float *, const int width);

void chan_dealloc(dssim_info_chan *chan) {
  delete[](chan->img1);
  chan->img1 = NULL;

  delete[](chan->img2);
  chan->img2 = NULL;

  delete[](chan->mu1);
  chan->mu1 = NULL;

  delete[](chan->mu2);
  chan->mu2 = NULL;

  delete[](chan->sigma1_sq);
  chan->sigma1_sq = NULL;

  delete[](chan->sigma2_sq);
  chan->sigma2_sq = NULL;

  delete[](chan->sigma12);
  chan->sigma12 = NULL;
}

static void set_gamma(const double invgamma) {
  for (int i = 0; i < 256; i++) {
    gamma_lut[i] = pow(i / 255.0, 1.0 / invgamma);
  }
}

static void square_row(float *row, const int width) {
  for (int i = 0; i < width; i++) {
    row[i] = row[i] * row[i];
  }
}

inline static laba rgba_to_laba(const dssim_rgba px) {
  const double r = gamma_lut[px.r], g = gamma_lut[px.g], b = gamma_lut[px.b];
  const float a = px.a / 255.f;

  double fx = (r * 0.4124 + g * 0.3576 + b * 0.1805) / D65x;
  double fy = (r * 0.2126 + g * 0.7152 + b * 0.0722) / D65y;
  double fz = (r * 0.0193 + g * 0.1192 + b * 0.9505) / D65z;

  const double epsilon = 216.0 / 24389.0;
  const double k = (24389.0 / 27.0) /
                   116.f;  // http://www.brucelindbloom.com/LContinuity.html
  const float X = (fx > epsilon) ? powf(fx, 1.f / 3.f) - 16.f / 116.f : k * fx;
  const float Y = (fy > epsilon) ? powf(fy, 1.f / 3.f) - 16.f / 116.f : k * fy;
  const float Z = (fz > epsilon) ? powf(fz, 1.f / 3.f) - 16.f / 116.f : k * fz;

  return (laba){
      Y * 1.16f,
      (86.2f / 220.0f +
       500.0f / 220.0f * (X - Y)),  // 86 is a fudge to make the value positive
      (107.9f / 220.0f +
       200.0f / 220.0f * (Y - Z)),  // 107 is a fudge to make the value positive
      a};
}

// Conversion is not reversible
inline static laba convert_pixel(dssim_rgba px, int i, int j,
                                 uint32_t alphaMask) {
  px.a |= alphaMask;
  laba f1 = rgba_to_laba(px);
  assert((f1.l >= 0.f) && (f1.l <= 1.0f));
  assert((f1.A >= 0.f) && (f1.A <= 1.0f));
  assert((f1.b >= 0.f) && (f1.b <= 1.0f));
  assert((f1.a >= 0.f) && (f1.a <= 1.0f));

  // Compose image on coloured background to better judge dissimilarity with
  // various backgrounds
  if (f1.a < 1.0) {
    f1.l *= f1.a;  // using premultiplied alpha
    f1.A *= f1.a;
    f1.b *= f1.a;

    int n = i ^ j;
    if (n & 4) {
      f1.l += 1.0 - f1.a;
    }
    if (n & 8) {
      f1.A += 1.0 - f1.a;
    }
    if (n & 16) {
      f1.b += 1.0 - f1.a;
    }
  }

  return f1;
}

static void convert_image(dssim_rgba *row_pointers[], dssim_info *inf,
                          float *ch0, float *ch1, float *ch2, bool doAlpha) {
  const int width = inf->chan[0].width;
  const int height = inf->chan[0].height;
  uint32_t alphaMask = doAlpha ? 0 : 0xff;

  // HACK - this is the sRGB default value. Not used with the Gaussian.
  // Does not impact on SSIM calculation speed.
  const double GammaIndex = 0.45455;
  set_gamma(GammaIndex);

  const int halfwidth = inf->chan[1].width;
  for (int y = 0, offset = 0; y < height; y++) {
    dssim_rgba *const px1 = row_pointers[y];
    const int halfy = y * inf->chan[1].height / height;
    for (int x = 0; x < width; x++, offset++) {
      laba f1 = convert_pixel(px1[x], x, y, alphaMask);

      ch0[offset] = f1.l;

      if (CHANS == 3) {
        ch1[x / 2 + halfy * halfwidth] += f1.A * 0.25f;
        ch2[x / 2 + halfy * halfwidth] += f1.b * 0.25f;
      }
    }
  }
}

//
// Blurs horizontally and vertically the source image into a destination image.
// [from
// http://www.gamedev.net/topic/307417-how-to-write-gaussian-blur-filter-in-c-tool/]
// The filter is applied in two passes for an increased speed.
// Kernel size is n -> complexity is O(n) instead than O(n*n)
//
// Input: src, width and height of the original image
// Output: dst, which is the image blurred
//
#define BYTES_PER_PIXEL 4
#define GAUSS_SIGMA 3
#define GAUSS_RADIUS 5  // TODO: use radius=2

void gaussianBlur(float *src, float *tmp, float *dst, const int width,
                  const int height) {
  volatile int Bpp = BYTES_PER_PIXEL;
  float sigma_sq = GAUSS_SIGMA * GAUSS_SIGMA;
  int radius = GAUSS_RADIUS;
  float pixel[BYTES_PER_PIXEL];
  float sum;

  // TO BE FIXED! height/4 and width/4 below is just a hack to prevent this
  // algorithm to give seg fault. The value of SSIM is of course wrong as a
  // consequence of this.

  // blurs x components
  for (int y = 0; y < height / 4; y++) {
    for (int x = 0; x < width / 4; x++) {
      // process a pixel
      sum = 0;
      pixel[0] = 0;
      pixel[1] = 0;
      pixel[2] = 0;
      pixel[3] = 0;

      // accumulate colors
      for (int i = max(0, x - radius); i <= min(width - 1, x + radius); i++) {
        // TODO:  factors can be pre-calculated in an array,
        // with pre-division by sum. With this optimization,
        // result can be written directly to tmp.

        float factor = exp(-(i - x) * (i - x) / (2 * sigma_sq));
        sum += factor;

        for (int c = 0; c < Bpp; c++) {
          pixel[c] += factor * src[(i + y * width) * Bpp + c];
        };
      };

      // copy a pixel
      for (int c = 0; c < Bpp; c++) {
        tmp[(x + y * width) * Bpp + c] = pixel[c] / sum;
      };
    };
  };

  // blurs y components
  for (int y = 0; y < height / 4; y++) {
    for (int x = 0; x < width / 4; x++) {
      // accumulate colors
      sum = 0;
      pixel[0] = 0;
      pixel[1] = 0;
      pixel[2] = 0;
      pixel[3] = 0;

      for (int i = max(0, y - radius); i <= min(height - 1, y + radius); i++) {
        // TODO:  factors can be pre-calculated in an array,
        // with pre-division by sum. With this optimization,
        // result can be written directly to tmp.

        float factor = exp(-(i - y) * (i - y) / (2 * sigma_sq));
        sum += factor;
        for (int c = 0; c < Bpp; c++) {
          pixel[c] += factor * tmp[(x + i * width) * Bpp + c];
        };
      };

      // copy a pixel
      for (int c = 0; c < Bpp; c++) {
        dst[(x + y * width) * Bpp + c] = pixel[c] / sum;
      };
    };
  };
}

//
// Blurs image horizontally (width 2*radius+1) and writes it transposed to dst
// (called twice gives 2d blur)
// Callback is executed on every row before blurring
static void transposing_1d_blur(float *src, float *dst, const int width,
                                const int height) {
  const int radius = TRANS_BLUR_RAY;
  const float radiusf = radius;

  for (int y = 0; y < (height / 2); y++) {
    float *row = src + (2 * y) * width;

    // accumulate total for pixels outside line
    float total = 0;
    total = row[0] * radiusf;
    for (int x = 0; x < min(width, radius); x++) {
      total += row[x];
    }

    // blur with left side outside line
    for (int x = 0; x < min(width, radius); x++) {
      total -= row[0];
      if ((x + radius) < width) {
        total += row[x + radius];
      }

      dst[x * height + (2 * y)] = total / (radiusf * 2.0f);
    }

    // blur in the middle
    for (int x = radius; x < width - radius; x++) {
      total -= row[x - radius];
      total += row[x + radius];

      dst[x * height + (2 * y)] = total / (radiusf * 2.0f);
    }

    // blur with right side outside line
    for (int x = width - radius; x < width; x++) {
      if (x - radius >= 0) {
        total -= row[x - radius];
      }
      total += row[width - 1];

      dst[x * height + (2 * y)] = total / (radiusf * 2.0f);
    }
  }
}

static void regular_1d_blur(float *src, float *dst, const int width,
                            const int height, rowcallback *const callback) {
  const int radius = 1;
  const float radiusf = radius;

  for (int j = 0; j < height; j++) {
    float *row = src + j * width;
    float *dstrow = dst + j * width;

    // preprocess line
    if (callback)
      callback(row, width);

    // accumulate total for pixels outside line
    float total = 0;
    total = row[0] * radiusf;
    for (int i = 0; i < min(width, radius); i++) {
      total += row[i];
    }

    // blur with left side outside line
    for (int i = 0; i < min(width, radius); i++) {
      total -= row[0];
      if ((i + radius) < width) {
        total += row[i + radius];
      }

      dstrow[i] = total / (radiusf * 2.0f);
    }

    for (int i = radius; i < width - radius; i++) {
      total -= row[i - radius];
      total += row[i + radius];

      dstrow[i] = total / (radiusf * 2.0f);
    }

    // blur with right side outside line
    for (int i = width - radius; i < width; i++) {
      if (i - radius >= 0) {
        total -= row[i - radius];
      }
      total += row[width - 1];

      dstrow[i] = total / (radiusf * 2.0f);
    }
  }
}

//
// Filters image with callback and blurs (lousy approximate of gaussian)
// Input: src = image to be blurred
// Output: dst = image blurred
static void blur(float *src, float *tmp, float *dst, const int width,
                 const int height, rowcallback *const callback,
                 const int blur_type) {
  if (blur_type == ebtGaussian) {
    gaussianBlur(src, tmp, dst, width, height);
  } else  // linear blur
  {
    regular_1d_blur(src, tmp, width, height, callback);
    regular_1d_blur(tmp, dst, width, height, NULL);

    transposing_1d_blur(dst, tmp, width, height);

    regular_1d_blur(tmp, dst, height, width, NULL);
    regular_1d_blur(dst, tmp, height, width, NULL);

    transposing_1d_blur(tmp, dst, height, width);
  }
}

//
// This method generates the images required by the SSIM formula. They are:
// convert image color space from RGBA to YUV
// calculate average of imageX -> muX
// calculate variance of imageX -> sigmaX_sq
// calculate the product of image1 and image2 -> img1_img2
// calculate the covariance of the two images -> sigma12
//
// Input:
// Bufrow_pointers & Refrow_pointers = pointers to the rows of the two images
// width & height = dimension of the two images (must be equal)
//
// Output: the dssim_info structure with all the new images
//
void DoSSIMCalculations(dssim_info *inf, dssim_rgba *Bufrow_pointers[],
                        dssim_rgba *Refrow_pointers[], const int width,
                        const int height, const int blur_type, bool hasAlpha) {
  float *img2[CHANS];
  for (int ch = 0; ch < CHANS; ch++) {
    inf->chan[ch].width = ch > 0 ? width / 2 : width;
    inf->chan[ch].height = ch > 0 ? height / 2 : height;
    inf->chan[ch].img1 =
        new float[inf->chan[ch].width * inf->chan[ch].height]();
    img2[ch] = new float[inf->chan[ch].width * inf->chan[ch].height]();
  }

  // int64_t startTime = ns2ms(systemTime(SYSTEM_TIME_MONOTONIC));
  convert_image(Bufrow_pointers, inf, inf->chan[0].img1, inf->chan[1].img1,
                inf->chan[2].img1, hasAlpha);
  convert_image(Refrow_pointers, inf, img2[0], img2[1], img2[2], hasAlpha);
  // printf("%s convert_image algorithm execution time in milliseconds: %llu\n",
  // __FUNCTION__, (ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)) - startTime));

  float *sigma1_tmp = new float[width * height]();
  float *tmp = new float[width * height]();

  for (int ch = 0; ch < CHANS; ch++) {
    const int width = inf->chan[ch].width;
    const int height = inf->chan[ch].height;

    float *img1 = inf->chan[ch].img1;

    if (ch > 0) {
      blur(img1, tmp, img1, width, height, NULL, blur_type);
      blur(img2[ch], tmp, img2[ch], width, height, NULL, blur_type);
    }

    for (int j = 0; j < width * height; j++) {
      sigma1_tmp[j] = img1[j] * img1[j];
    }

    inf->chan[ch].mu1 = new float[width * height]();
    inf->chan[ch].sigma1_sq = new float[width * height]();
    inf->chan[ch].sigma12 = new float[width * height]();
    inf->chan[ch].sigma2_sq = new float[width * height]();
    float *img1_img2 = new float[width * height]();

    blur(img1, tmp, inf->chan[ch].mu1, width, height, NULL, blur_type);

    blur(sigma1_tmp, tmp, inf->chan[ch].sigma1_sq, width, height, NULL,
         blur_type);

    for (int j = 0; j < width * height; j++) {
      img1_img2[j] = img1[j] * img2[ch][j];
    }

    blur(img1_img2, tmp, inf->chan[ch].sigma12, width, height, NULL, blur_type);

    inf->chan[ch].mu2 = img1_img2;  // reuse mem
    blur(img2[ch], tmp, inf->chan[ch].mu2, width, height, NULL, blur_type);

    blur(img2[ch], tmp, inf->chan[ch].sigma2_sq, width, height, square_row,
         blur_type);

    delete[](img2[ch]);
  }

  delete[](tmp);
  delete[](sigma1_tmp);
}

//
// SSIM Algorithm based on Rabah Mehdi's C++ implementation
// Evaluates SSIM index per video channel.
//
// Input: pointer to channel struct
// Output: SSIM index
//
double GetSSIMIndex(dssim_info_chan *chan) {
  const int width = chan->width;
  const int height = chan->height;

  float *mu1 = chan->mu1;
  float *mu2 = chan->mu2;
  float *sigma1_sq = chan->sigma1_sq;
  float *sigma2_sq = chan->sigma2_sq;
  float *sigma12 = chan->sigma12;

  // Is double precision required?
  // Probably needed for 1080p images so that fractional components for a pixel
  // are accumulated properly.
  // May be able to use single precision to accumulate ssim for a row,
  // then add the row total to a running total. TODO
  const double c1 = 0.01 * 0.01, c2 = 0.03 * 0.03;
  double avgssim = 0;

  for (int offset = 0; offset < width * height; offset++) {
    const double mu1_sq = mu1[offset] * mu1[offset];
    const double mu2_sq = mu2[offset] * mu2[offset];
    const double mu1mu2 = mu1[offset] * mu2[offset];

    const double ssim =
        (c1 + 2.0 * mu1mu2) * (c2 + 2.0 * (sigma12[offset] - mu1mu2)) /
        ((c1 + mu1_sq + mu2_sq) *
         (c2 + sigma1_sq[offset] - mu1_sq + sigma2_sq[offset] - mu2_sq));

    avgssim += ssim;
  }

  chan_dealloc(chan);

  return avgssim / ((double)width * height);
}
