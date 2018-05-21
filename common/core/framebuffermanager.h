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
#include <hwctrace.h>
#include <platformdefines.h>

#include <memory>
#include <unordered_map>

#include <spinlock.h>

namespace hwcomposer {

struct HwcLayer;
class OverlayBuffer;
class NativeBufferHandler;

typedef struct {
  uint32_t fb_id;
  uint32_t fb_ref;
  bool fb_created;
} FBValue;

struct FBHash {
  size_t operator()(FBKey const &key) const {
    return key.gem_handles_[0];
  }
};

struct FBEqual {
  bool operator()(const FBKey &p1, const FBKey &p2) const {
    bool equal = (p1.gem_handles_[0] == p2.gem_handles_[0]) &&
                 (p1.gem_handles_[1] == p2.gem_handles_[1]) &&
                 (p1.gem_handles_[2] == p2.gem_handles_[2]) &&
                 (p1.gem_handles_[3] == p2.gem_handles_[3]);
    return equal;
  }
};

class FrameBufferManager {
 public:
  FrameBufferManager(uint32_t gpu_fd) : gpu_fd_(gpu_fd) {
  }
  ~FrameBufferManager() {
    PurgeAllFBs();
  }

  void RegisterGemHandles(const uint32_t &num_planes,
                          const uint32_t (&igem_handles)[4]);
  uint32_t FindFB(const uint32_t &iwidth, const uint32_t &iheight,
                  const uint64_t &modifier,
                  const uint32_t &iframe_buffer_format,
                  const uint32_t &num_planes, const uint32_t (&igem_handles)[4],
                  const uint32_t (&ipitches)[4], const uint32_t (&ioffsets)[4]);
  int RemoveFB(uint32_t num_planes, const uint32_t (&igem_handles)[4]);

 private:
  SpinLock lock_;
  void PurgeAllFBs();

  std::unordered_map<FBKey, FBValue, FBHash, FBEqual> fb_map_;
  uint32_t gpu_fd_ = 0;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_RESOURCE_MANAGER_H_
