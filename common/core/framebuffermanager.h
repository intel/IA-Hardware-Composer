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

/*
Design of ResourceManager:
The purpose is to add cache magagement to external buffer owned by hwcLayer
to avoid import buffer and glimage/texture generation overhead

1: the ResourceManager is owned per display, as each display has a
separate
GL context
2: ResourceManager stores a refernce of external buffers in a vector
   cached_buffers, each vector member is hash map.
   The vector stores history buffer in this way, vector[0] is for the current
   frame buffers, vector[1] is for last frame (-1) buffers, vector[2] is -2
   frames buffers etc. A constant (currently 4) frames of buffers is stored.
   When a buffer refernce is stored in vector[3] and it is not used in the
   current frame present, it will go out of scope and be released.
   If a buffer is fetched from map, it will always re-registered in vector[0]
map.
3. By this way, drm_buffer now owns eglImage and gltexture and they
   can be resued.
*/

#ifndef COMMON_CORE_FRAMEBUFFER_MANAGER_H_
#define COMMON_CORE_FRAMEBUFFER_MANAGER_H_

#include <hwcdefs.h>
#include <platformdefines.h>
#include <hwctrace.h>

#include <memory>
#include <unordered_map>

#include <spinlock.h>

namespace hwcomposer {

struct HwcLayer;
class OverlayBuffer;
class NativeBufferHandler;

typedef struct FBKey {
  uint32_t gpu_fd;
  uint32_t width;
  uint32_t height;
  uint32_t frame_buffer_format;
  uint32_t gem_handles[4];
  uint32_t pitches[4];
  uint32_t offsets[4];

  FBKey(const uint32_t &igpu_fd, const uint32_t &iwidth,
        const uint32_t &iheight, const uint32_t &iframe_buffer_format,
        const uint32_t (&igem_handles)[4], const uint32_t (&ipitches)[4],
        const uint32_t (&ioffsets)[4]) {
    gpu_fd = igpu_fd;
    width = iwidth;
    height = iheight;
    frame_buffer_format = iframe_buffer_format;
    gem_handles[0] = igem_handles[0];
    gem_handles[1] = igem_handles[1];
    gem_handles[2] = igem_handles[2];
    gem_handles[3] = igem_handles[3];
    pitches[0] = ipitches[0];
    pitches[1] = ipitches[1];
    pitches[2] = ipitches[2];
    pitches[3] = ipitches[3];
    offsets[0] = ioffsets[0];
    offsets[1] = ioffsets[1];
    offsets[2] = ioffsets[2];
    offsets[3] = ioffsets[3];
  }
} FBKey;

typedef struct {
  uint32_t fb_id;
  uint32_t fb_ref;
} FBValue;

struct FBHash {
  size_t operator()(FBKey const &key) const { return key.gem_handles[0]; }
};

struct FBEqual {
  bool operator()(const FBKey &p1, const FBKey &p2) const {
    bool equal = (p1.gpu_fd == p2.gpu_fd) && (p1.width == p2.width) &&
                 (p1.height == p2.height) &&
                 (p1.frame_buffer_format == p2.frame_buffer_format);
    if (equal) {
      for (int i = 0; i < 4; i++) {
        equal = equal && (p1.gem_handles[i] == p2.gem_handles[i]);
        equal = equal && (p1.pitches[i] == p2.pitches[i]);
        equal = equal && (p1.offsets[i] == p2.offsets[i]);
        if (!equal)
          break;
      }
    }
    return equal;
  }
};

class FrameBufferManager {
public:
  static FrameBufferManager *GetInstance();
  uint32_t FindFB(const FBKey &key);
  int RemoveFB(const int32_t fb, bool &real_moved);

private:
  static FrameBufferManager *pInstance;
  SpinLock lock_;
  void PurgeAllFBs();
  FrameBufferManager() {}
  ~FrameBufferManager() { PurgeAllFBs(); }

  std::unordered_map<FBKey, FBValue, FBHash, FBEqual> fb_map_;
};

} // namespace hwcomposer
#endif // COMMON_CORE_RESOURCE_MANAGER_H_
