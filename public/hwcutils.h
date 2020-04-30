/*
// Copyright (c) 2017 Intel Corporation
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
/** \file */
#ifndef COMMON_UTILS_HWCUTILS_H_
#define COMMON_UTILS_HWCUTILS_H_

#ifdef USE_ANDROID_PROPERTIES
#include <cutils/properties.h>
#endif

#include <hwcdefs.h>
#include <sstream>
#include "overlaylayer.h"

#define ALL_EDID_FLAG_PROPERTY "vendor.hwcomposer.edid.all"

namespace hwcomposer {

/**
 * Wait until a file descriptor has data ready for reading
 *
 * @param fd file descriptor for an open file
 * @param timeout number of milliseconds to block while waiting for the file
 * descriptor to become ready.
 * @return 1 on success
 * @return 0 on timeout
 * @return -1 on error
 */
int HWCPoll(int fd, int timeout);

bool IsLayerAlphaBlendingCommitted(OverlayLayer* layer);

/**
 * Reset the bounds of a rectangle to enclose all rectangles in a region
 *
 * If the region is empty then the bounds of the rectangle will be set to 0.
 * Previous bounds of the rectangle will be reset.
 * @param hwc_region The region to enclose
 * @param rect The rectangle to reset
 */
void ResetRectToRegion(const HwcRegion& hwc_region, HwcRect<int>& rect);

/**
 * Expand the bounds of a rectangle to enclose the bounds of a target rectangle
 *
 * Has no effect if the target rectangle has no bounds.
 * Has no effect if the target rectangle is already enclosed.
 * @param target_rect The rectangle to enclose
 * @param new_rect The rectangle to be expanded
 */
void CalculateRect(const HwcRect<int>& target_rect, HwcRect<int>& new_rect);

/**
 * Expand the bounds of a rectangle to enclose the bounds of a target rectangle
 *
 * Has no effect if the target rectangle has no bounds.
 * Has no effect if the target rectangle is already enclosed.
 * @param target_rect The rectangle to enclose
 * @param new_rect The rectangle to be expanded
 */
void CalculateSourceRect(const HwcRect<float>& target_rect,
                         HwcRect<float>& new_rect);

/**
 * Check if a format is used for media
 *
 * @param format fourcc based pixel format (see drm_fourcc.h)
 * @return True for recognized media formats
 */
bool IsSupportedMediaFormat(uint32_t format);

/**
 * Check how many planes are used for a given pixel format
 *
 * @param format fourcc based pixel format (see drm_fourcc.h)
 * @return The number of planes for a given format
 */
uint32_t GetTotalPlanesForFormat(uint32_t format);

#ifdef KVM_HWC_PROPERTY
/**
 * Check if running on KVM
 *
 * @return True when running on KVM/QEMU
 */
bool IsKvmPlatform();
#endif

/**
 * Check if need to send all EDID, or only preferred and perf
 */
bool IsEdidFilting();

/**
 * Check if two rectangles overlap
 *
 * Top-left is inclusive; bottom-right is exclusive.
 * Origin is top-left of the coordinate plane.
 * @param l1 left bound of rectangle 1
 * @param t1 top bound of rectangle 1
 * @param r1 right bound of rectangle 1
 * @param b1 bottom bound of rectangle 1
 * @param l2 left bound of rectangle 2
 * @param t2 top bound of rectangle 2
 * @param r2 right bound of rectangle 2
 * @param b2 bottom bound of rectangle 2
 * @return True if rectangles overlap
 */
template <class T>
inline bool IsOverlapping(T l1, T t1, T r1, T b1, T l2, T t2, T r2, T b2)

{
  return ((l1 < r2 && r1 > l2) && (t1 < b2 && b1 > t2));
}

/**
 * Check if two rectangles overlap
 *
 * Top-left is inclusive; bottom-right is exclusive.
 * @param rect1 rectangle 1
 * @param rect2 rectangle 2
 * @return True if rectangles overlap
 */
inline bool IsOverlapping(const hwcomposer::HwcRect<int>& rect1,
                          const hwcomposer::HwcRect<int>& rect2) {
  return IsOverlapping(rect1.left, rect1.top, rect1.right, rect1.bottom,
                       rect2.left, rect2.top, rect2.right, rect2.bottom);
}

/**
 * Check if one rectangle is enclosed by another
 *
 * Top-left is inclusive; bottom-right is exclusive.
 * Origin is top-left of the coordinate plane.
 * @param l1 left bound of rectangle 1
 * @param t1 top bound of rectangle 1
 * @param r1 right bound of rectangle 1
 * @param b1 bottom bound of rectangle 1
 * @param l2 left bound of rectangle 2
 * @param t2 top bound of rectangle 2
 * @param r2 right bound of rectangle 2
 * @param b2 bottom bound of rectangle 2
 * @return True if rectangle 1 is enclosed by rectangle 2
 */
template <class T>
inline bool IsEnclosedBy(T l1, T t1, T r1, T b1, T l2, T t2, T r2, T b2) {
  return ((l1 >= l2 && t1 >= t2) && (r1 <= r2 && b1 <= b2));
}

/**
 * Check if one rectangle is enclosed by another
 *
 * Top-left is inclusive; bottom-right is exclusive.
 * @param rect1 rectangle 1
 * @param rect2 rectangle 2
 * @return True if rectangle 1 is enclosed by rectangle 2
 */
inline bool IsEnclosedBy(const hwcomposer::HwcRect<int>& rect1,
                         const hwcomposer::HwcRect<int>& rect2) {
  return IsEnclosedBy(rect1.left, rect1.top, rect1.right, rect1.bottom,
                      rect2.left, rect2.top, rect2.right, rect2.bottom);
}

enum OverlapType { kEnclosed = 0, kOverlapping, kOutside };

/**
 * Check the bounds overlap of two rectangles
 *
 * Top-left is inclusive; bottom-right is exclusive.
 * @param rect rectangle 1
 * @param bounds rectangle 2
 * @return kEnclosed if rectangle 1 is enclosed by rectangle 2
 * @return kOverlapping if rectangle 1 overlaps with but is not enclosed by
 * rectangle 2
 * @return kOutside if the two rectangles are separate
 */
inline OverlapType AnalyseOverlap(const hwcomposer::HwcRect<int>& rect,
                                  const hwcomposer::HwcRect<int>& bounds) {
  if (IsEnclosedBy(rect, bounds)) {
    return kEnclosed;
  } else if (IsOverlapping(rect, bounds)) {
    return kOverlapping;
  } else {
    return kOutside;
  }
}

/**
 * Translate a rectangle across the coordinate plane
 *
 * Origin is top-left of the coordinate plane
 * @param x number of units to translate rightward
 * @param y number of units to translate downward
 * @return Translated rectangle
 */
inline HwcRect<int> TranslateRect(HwcRect<int> rect, int x, int y) {
  HwcRect<int> ret;
  ret.left = rect.left + x;
  ret.right = rect.right + x;
  ret.top = rect.top + y;
  ret.bottom = rect.bottom + y;
  return ret;
}

/**
 * Find the rectangle that bounds the intersection between two rectangles
 *
 * @param rect1 rectangle 1
 * @param rect2 rectangle 2
 * @return Rectangle bounding the intersection
 * @return Empty rectangle if no intersection is found
 */
inline HwcRect<int> Intersection(const hwcomposer::HwcRect<int>& rect1,
                                 const hwcomposer::HwcRect<int>& rect2) {
  HwcRect<int> rect = {0, 0, 0, 0};

  int lmax = std::max(rect1.left, rect2.left);
  int tmax = std::max(rect1.top, rect2.top);

  int rmin = std::min(rect1.right, rect2.right);
  int bmin = std::min(rect1.bottom, rect2.bottom);

  if (rmin <= lmax || bmin <= tmax)
    return rect;

  rect.left = lmax;
  rect.right = rmin;
  rect.top = tmax;
  rect.bottom = bmin;

  return rect;
}

/**
 * Pretty-print HwcRect for debugging.
 */
std::string StringifyRect(HwcRect<int> rect);

/**
 * Pretty-print HwcRegion for debugging.
 */
std::string StringifyRegion(HwcRegion region);

HwcRect<int> RotateRect(const HwcRect<int>& rect, int disp_width,
                        int disp_height, uint32_t transform);
HwcRect<int> ScaleRect(HwcRect<int> rect, float x_scale, float y_scale);
HwcRect<int> RotateScaleRect(HwcRect<int> rect, int width, int height,
                             uint32_t plane_transform);
}  // namespace hwcomposer

#endif  // COMMON_UTILS_HWCUTILS_H_
