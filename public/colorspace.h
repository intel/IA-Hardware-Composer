/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PUBLIC_COLORSPACE_H
#define PUBLIC_COLORSPACE_H

/** A CIE 1931 color space*/
struct cie_xy {
  double x;
  double y;
};

struct color_primaries {
  struct cie_xy r;
  struct cie_xy g;
  struct cie_xy b;
  struct cie_xy white_point;
};

struct colorspace {
  struct color_primaries primaries;
  const char *name;
  const char *whitepoint_name;
};

enum colorspace_enums {
  CS_BT470M,
  CS_BT470BG,
  CS_SMPTE170M,
  CS_SMPTE240M,
  CS_BT709,
  CS_BT2020,
  CS_SRGB,
  CS_ADOBERGB,
  CS_DCI_P3,
  CS_PROPHOTORGB,
  CS_CIERGB,
  CS_CIEXYZ,
  CS_AP0,
  CS_AP1,
  CS_UNDEFINED
};
#endif
