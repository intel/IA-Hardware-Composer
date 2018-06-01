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

#ifndef COMMON_UTILS_HWCUTILS_H_
#define COMMON_UTILS_HWCUTILS_H_

#include <hwcdefs.h>
#include <sstream>

namespace hwcomposer {

// Helper functions.

// Call poll() on fd.
//  - timeout: time in miliseconds to stay blocked before returning if fd
//  is not ready.
int HWCPoll(int fd, int timeout);

// Reset's rect to include region hwc_region.
void ResetRectToRegion(const HwcRegion& hwc_region, HwcRect<int>& rect);

// Calculate new_rect to include target_rect. This is used to calculate
// Display Frame and Surface Damage rect of layers.
void CalculateRect(const HwcRect<int>& target_rect, HwcRect<int>& new_rect);

// Calculate new_rect to include target_rect. This is used to calculate
// Source Rect of layers.
void CalculateSourceRect(const HwcRect<float>& target_rect,
                         HwcRect<float>& new_rect);

// Returns true if format is a Media format.
bool IsSupportedMediaFormat(uint32_t format);

// Returns total planes for a given format.
uint32_t GetTotalPlanesForFormat(uint32_t format);

template <class T>
inline bool IsOverlapping(T l1, T t1, T r1, T b1, T l2, T t2, T r2, T b2)
// Do two rectangles overlap?
// Top-left is inclusive; bottom-right is exclusive
{
  return ((l1 < r2 && r1 > l2) && (t1 < b2 && b1 > t2));
}

inline bool IsOverlapping(const hwcomposer::HwcRect<int>& rect1,
                          const hwcomposer::HwcRect<int>& rect2) {
  return IsOverlapping(rect1.left, rect1.top, rect1.right, rect1.bottom,
                       rect2.left, rect2.top, rect2.right, rect2.bottom);
}

template <class T>
inline bool IsEnclosedBy(T l1, T t1, T r1, T b1, T l2, T t2, T r2, T b2)
// Do two rectangles overlap?
// Top-left is inclusive; bottom-right is exclusive
{
  return ((l1 >= l2 && t1 >= t2) && (r1 <= r2 && b1 <= b2));
}

inline bool IsEnclosedBy(const hwcomposer::HwcRect<int>& rect1,
                         const hwcomposer::HwcRect<int>& rect2) {
  return IsEnclosedBy(rect1.left, rect1.top, rect1.right, rect1.bottom,
                      rect2.left, rect2.top, rect2.right, rect2.bottom);
}

enum OverlapType { kEnclosed = 0, kOverlapping, kOutside };

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

inline HwcRect<int> TranslateRect(HwcRect<int> rect, int x, int y) {
  HwcRect<int> ret;
  ret.left = rect.left + x;
  ret.right = rect.right + x;
  ret.top = rect.top + y;
  ret.bottom = rect.bottom + y;
  return ret;
}

inline HwcRect<int> Intersection(const hwcomposer::HwcRect<int>& rect1,
                                 const hwcomposer::HwcRect<int>& rect2) {
  HwcRect<int> rect = {0, 0, 0, 0};

  int lmax = std::max(rect1.left, rect2.left);
  int rmax = std::max(rect1.right, rect2.right);
  int tmax = std::max(rect1.top, rect2.top);
  int bmax = std::max(rect1.bottom, rect2.bottom);

  int lmin = std::min(rect1.left, rect2.left);
  int rmin = std::min(rect1.right, rect2.right);
  int tmin = std::min(rect1.top, rect2.top);
  int bmin = std::min(rect1.bottom, rect2.bottom);

  if (lmax == rmax || tmax == bmax)
    return rect;

  if (lmin == rmin || tmin == bmin)
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

}  // namespace hwcomposer

#endif  // COMMON_UTILS_HWCUTILS_H_
