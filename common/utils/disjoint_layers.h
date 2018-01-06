/*
// Copyright (c) 2016 Intel Corporation
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

#ifndef COMMON_UTILS_DISJOINT_LAYERS_H_
#define COMMON_UTILS_DISJOINT_LAYERS_H_

#include <stdint.h>

#include <hwcrect.h>

#include <vector>

#include <hwcdefs.h>

namespace hwcomposer {

// Some of the structs are adopted from drm_hwcomposer
struct RectIDs {
 public:
  typedef uint64_t TId;

  RectIDs() : bitset(0) {
  }

  explicit RectIDs(TId id) : bitset(0) {
    add(id);
  }

  void add(TId id) {
    bitset |= ((uint64_t)1) << id;
  }

  void subtract(TId id) {
    bitset &= ~(((uint64_t)1) << id);
  }

  bool isEmpty() const {
    return bitset == 0;
  }

  uint64_t getBits() const {
    return bitset;
  }

  bool operator==(const RectIDs &rhs) const {
    return bitset == rhs.bitset;
  }

  bool operator<(const RectIDs &rhs) const {
    return bitset < rhs.bitset;
  }

  RectIDs operator|(const RectIDs &rhs) const {
    RectIDs ret;
    ret.bitset = bitset | rhs.bitset;
    return ret;
  }

  RectIDs operator|(TId id) const {
    RectIDs ret;
    ret.bitset = bitset;
    ret.add(id);
    return ret;
  }

  static const int max_elements = sizeof(TId) * 8;

 private:
  uint64_t bitset;
};

template <typename TNum>
struct RectSet {
  RectIDs id_set;
  Rect<TNum> rect;

  RectSet(const RectIDs &i, const Rect<TNum> &r) : id_set(i), rect(r) {
  }

  bool operator==(const RectSet<TNum> &rhs) const {
    return (id_set == rhs.id_set) && (rect == rhs.rect);
  }
};

void get_draw_regions(const std::vector<Rect<int>> &in,
                      const HwcRect<int> &damage_region,
                      std::vector<RectSet<int>> *out);
}  // namespace hwcomposer

#endif  // COMMON_UTILS_DISJOINT_LAYERS_H_
