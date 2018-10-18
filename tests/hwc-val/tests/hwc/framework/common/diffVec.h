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

#ifndef __diffVec_h__
#define __diffVec_h__

// Remove from BOTH vectors any items found in both.
template <class T>
void diffVec(std::set<T>& va, std::set<T>& vb) {
  // remove all items in both arrays
  size_t aIx = 0;
  size_t bIx = 0;

  for (; aIx < va.size() && bIx < vb.size();) {
    T a = va[aIx];
    T b = vb[bIx];

    if (a == b) {
      va.removeAt(aIx);
      vb.removeAt(bIx);
    } else if (a < b) {
      ++aIx;
    } else {
      ++bIx;
    }
  }
}

template <class T>
std::set<T>& operator+=(std::set<T>& va,
                                     const std::set<T>& vb) {
  for (size_t bIx = 0; bIx < vb.size(); ++bIx) {
    va.add(vb[bIx]);
  }
  return va;
}

template <class T>
std::set<T>& operator-=(std::set<T>& va,
                                     const std::set<T>& vb) {
  for (size_t bIx = 0; bIx < vb.size(); ++bIx) {
    va.remove(vb[bIx]);
  }
  return va;
}

#endif  // __HWC_SHIM_DEFS_H__
