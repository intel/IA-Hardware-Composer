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

#ifndef __HwchCoord_h__
#define __HwchCoord_h__

#include <stdio.h>

#include <hwcdefs.h>
// Suppress warning on unused parameter
#define _UNUSED(x) ((void)&(x))

namespace Hwch {
// Forward Reference
uint32_t GetWallpaperSize();

// Increment operator - to be used to iterate through the rotations.
// The last value it will return will be kMaxRotate, which isn't really a
// rotation.
inline hwcomposer::HWCRotation operator++(hwcomposer::HWCRotation& rot) {
  uint32_t& r = *((uint32_t*)&rot);

  if (r < hwcomposer::HWCRotation::kMaxRotate) {
    ++r;
  }

  return (hwcomposer::HWCRotation)r;
}

// Add rotations
// This always returns a valid rotation from kRotateNone to kRotate270.
inline hwcomposer::HWCRotation operator+(hwcomposer::HWCRotation rot1,
                                         hwcomposer::HWCRotation rot2) {
  uint32_t r1 = (uint32_t)rot1;
  uint32_t r2 = (uint32_t)rot2;

  return (hwcomposer::HWCRotation)((r1 + r2) %
                                   (hwcomposer::HWCRotation::kMaxRotate));
}

// Coordinate type
enum CoordType {
  eCoordAbsolute,   // Absolute - relative to top or left of screen
  eCoordCentreRel,  // Relative to centre of screen
  eCoordMaxRel,     // Relative to right or bottom of screen
  eCoordScaled,  // Calculate by scaling relative to screen size from original
                 // frame of reference
  eCoordWallpaper,  // Special for wallpaper layers: Give the X or Y size of D0,
                    // whichever is greater
  eCoordUnassigned  // Coordinate undefined
};

inline float RoundIfNeeded(float t, float v) {
  _UNUSED(t);  // Just to give the right type of the result
  return v;
}

inline uint32_t RoundIfNeeded(uint32_t t, float v) {
  _UNUSED(t);  // Just to give the right type of the result
  return v + 0.5;
}

inline int32_t RoundIfNeeded(int32_t t, float v) {
  _UNUSED(t);  // Just to give the right type of the result
  return v + 0.5;
}

template <class T>
class Coord {
 public:
  CoordType mType;
  T mValue;

  Coord<T>(int32_t value = 0, CoordType c = eCoordAbsolute) {
    mValue = value;
    mType = c;
  }

  Coord<T>(uint32_t value, CoordType c = eCoordAbsolute) {
    mValue = value;
    mType = c;
  }

  Coord<T>(float value, CoordType c = eCoordAbsolute) {
    mValue = value;
    mType = c;
  }

  Coord<T>(double value, CoordType c = eCoordAbsolute) {
    mValue = value;
    mType = c;
  }

  T Phys(T screenMax) const {
    switch (mType) {
      case eCoordCentreRel:
        return (screenMax / 2) + mValue;

      case eCoordMaxRel:
        return screenMax + mValue;

      case eCoordScaled:
        return RoundIfNeeded(
            screenMax,
            ((static_cast<double>(mValue) + 0.5) / 65536.0) * screenMax);

      case eCoordWallpaper:
        return static_cast<T>(GetWallpaperSize());

      default:
        return mValue;
    }
  }

  inline const Coord<T>& operator=(const T value) {
    mValue = value;
    mType = eCoordAbsolute;
    return *this;
  }

  inline char* WriteStr(char* buf, const char* numFormat) {
    switch (mType) {
      case eCoordAbsolute:
        *buf++ = 'A';
        break;
      case eCoordCentreRel:
        *buf++ = 'C';
        break;
      case eCoordMaxRel:
        *buf++ = 'M';
        break;
      case eCoordScaled:
        buf += sprintf(buf, "S%f",
                       ((static_cast<double>(mValue) + 0.5) / 65536.0));
        return buf;
        break;
      case eCoordWallpaper:
        *buf++ = 'W';
        break;
      case eCoordUnassigned:
        *buf++ = 'U';
        break;
      default:
        *buf++ = '?';
    }

    if (strcmp(numFormat, "%f") == 0) {
      return buf + sprintf(buf, numFormat, (double)mValue);
    } else {
      return buf + sprintf(buf, numFormat, mValue);
    }
  }
};

template <class T>
Coord<T> operator+(Coord<T> coord1, const Coord<T>& coord2) {
  ALOG_ASSERT(coord1.mType == coord2.mType);
  coord1.mValue += coord2.mValue;
  return coord1;
}

template <class T>
Coord<T> operator-(Coord<T> coord1, const Coord<T>& coord2) {
  ALOG_ASSERT(coord1.mType == coord2.mType);
  coord1.mValue -= coord2.mValue;
  return coord1;
}

template <class T>
Coord<T> operator+(Coord<T> coord, T additional) {
  coord.mValue += additional;
  return coord;
}

template <class T>
Coord<T> operator-(Coord<T> coord, T additional) {
  coord.mValue -= additional;
  return coord;
}

template <class T>
inline bool operator==(Coord<T> a, Coord<T> b) {
  return ((a.mType == b.mType) && (a.mValue == b.mValue));
}

template <class T>
class CtrRelative : public Coord<T> {
 public:
  CtrRelative<T>(const int32_t& value) : Coord<T>(value, eCoordCentreRel) {
  }

  CtrRelative<T>(const uint32_t& value) : Coord<T>(value, eCoordCentreRel) {
  }

  CtrRelative<T>(const float& value) : Coord<T>(value, eCoordCentreRel) {
  }

  CtrRelative<T>(const double& value) : Coord<T>(value, eCoordCentreRel) {
  }
};
typedef CtrRelative<int32_t> CtrRel;
typedef CtrRelative<float> CtrRelF;

template <class T>
class MaxRelative : public Coord<T> {
 public:
  MaxRelative<T>(const int32_t& value) : Coord<T>(value, eCoordMaxRel) {
  }

  MaxRelative<T>(const uint32_t& value) : Coord<T>(value, eCoordMaxRel) {
  }

  MaxRelative<T>(const float& value) : Coord<T>(value, eCoordMaxRel) {
  }

  MaxRelative<T>(const double& value) : Coord<T>(value, eCoordMaxRel) {
  }
};
typedef MaxRelative<int32_t> MaxRel;
typedef MaxRelative<float> MaxRelF;

template <class T>
class Autoscaled : public Coord<T> {
 public:
  Autoscaled<T>(const int32_t& value, const int32_t& range)
      : Coord<T>((value << 16) / range, eCoordScaled) {
  }

  Autoscaled<T>(const uint32_t& value, const uint32_t& range)
      : Coord<T>((value << 16) / range, eCoordScaled) {
  }

  Autoscaled<T>(const float& value, const float& range)
      : Coord<T>((value * 65536) / range, eCoordScaled) {
  }

  Autoscaled<T>(const double& value, const double& range)
      : Coord<T>((value * 65536) / range, eCoordScaled) {
  }
};
typedef Autoscaled<int32_t> Scaled;
typedef Autoscaled<float> ScaledF;

template <class T>
class CoordWallpaper : public Coord<T> {
 public:
  CoordWallpaper<T>() : Coord<T>(0U, eCoordWallpaper) {
  }
};
typedef CoordWallpaper<int32_t> WallpaperSize;

template <class T>
class CoordUnassigned : public Coord<T> {
 public:
  CoordUnassigned<T>() : Coord<T>(0U, eCoordUnassigned) {
  }
};

template <class T>
class LogicalRect {
 public:
  Coord<T> left;
  Coord<T> top;
  Coord<T> right;
  Coord<T> bottom;

  LogicalRect<T>(const Coord<T>& l, const Coord<T>& t, const Coord<T>& r,
                 const Coord<T>& b)
      : left(l), top(t), right(r), bottom(b) {
  }

  LogicalRect<T>() : left(0), top(0), right(0), bottom(0) {
  }

  char* AppendStr(char* buf, const char* format) {
    char* ptr = buf;
    while (*ptr) {
      ++ptr;
    }
    *ptr++ = '(';
    ptr = left.WriteStr(ptr, format);
    *ptr++ = ',';
    ptr = top.WriteStr(ptr, format);
    *ptr++ = ',';
    ptr = right.WriteStr(ptr, format);
    *ptr++ = ',';
    ptr = bottom.WriteStr(ptr, format);
    *ptr++ = ')';
    *ptr = '\0';
    return ptr;
  }

  char* Str(const char* format) {
    static char sbuf[200];
    sbuf[0] = '\0';
    AppendStr(sbuf, format);
    return sbuf;
  }

  T Width() const {
    ALOG_ASSERT(left.mType == right.mType);
    return right.mValue - left.mValue;
  }

  T Height() const {
    ALOG_ASSERT(bottom.mType == top.mType);
    return bottom.mValue - top.mValue;
  }
};

template <class T>
bool operator==(const LogicalRect<T>& a, const LogicalRect<T>& b) {
  return ((a.left == b.left) && (a.top == b.top) && (a.right == b.right) &&
          (a.bottom == b.bottom));
}

template <class T>
bool operator!=(const LogicalRect<T>& a, const LogicalRect<T>& b) {
  return !(a == b);
}

typedef LogicalRect<int32_t> LogDisplayRect;
typedef LogicalRect<float> LogCropRect;
}

#undef _UNUSED

#endif  // __HwchCoord_h__
