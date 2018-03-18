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

#ifndef COMMON_CORE_RESOURCE_MANAGER_H_
#define COMMON_CORE_RESOURCE_MANAGER_H_

#include <hwcdefs.h>
#include <platformdefines.h>
#include <hwctrace.h>

#include <memory>
#include <unordered_map>

#include <spinlock.h>

#include "overlaybuffer.h"

namespace hwcomposer {

struct HwcLayer;
class OverlayBuffer;
class NativeBufferHandler;

class ResourceManager {
 public:
  ResourceManager(NativeBufferHandler* buffer_handler);
  ~ResourceManager();
  void Dump();
  std::shared_ptr<OverlayBuffer>& FindCachedBuffer(
      const uint32_t& native_buffer);
  void RegisterBuffer(const uint32_t& native_buffer,
                      std::shared_ptr<OverlayBuffer>& pBuffer);
  void MarkResourceForDeletion(const ResourceHandle& handle,
                               bool has_valid_gpu_resources);

  void MarkMediaResourceForDeletion(const MediaResourceHandle& handle);
  void RefreshBufferCache();
  void GetPurgedResources(std::vector<ResourceHandle>& gl_resources,
                          std::vector<MediaResourceHandle>& media_resources,
                          bool* has_gpu_resource);
  void PurgeBuffer();

  // This should be called by DisplayQueue at end of every present call
  // to free all purged GL, Native and Media resources. Returns true
  // if any resources are marked to be deleted else returns false.
  bool PreparePurgedResources();

  const NativeBufferHandler* GetNativeBufferHandler() const {
    return buffer_handler_;
  }

 private:
#define BUFFER_CACHE_LENGTH 4
  typedef std::unordered_map<uint32_t, std::shared_ptr<OverlayBuffer>> BUFFER_MAP;
  std::vector<BUFFER_MAP> cached_buffers_;
  // This should be used in same thread handling
  // Present in NativeDisplay.
  std::vector<ResourceHandle> purged_resources_;
  // This should be used in same thread handling
  // Present in NativeDisplay.
  std::vector<MediaResourceHandle> purged_media_resources_;
  // This should be used in same thread handling
  // NativeDisplay.
  bool has_purged_gpu_resources_ = false;
  // This can be used from any thread.
  bool destroy_gpu_resources_ = false;
  // This can be used from any thread.
  std::vector<ResourceHandle> destroy_gl_resources_;
  // This can be used from any thread.
  std::vector<MediaResourceHandle> destroy_media_resources_;
  NativeBufferHandler* buffer_handler_;
  SpinLock lock_;
#ifdef RESOURCE_CACHE_TRACING
  uint32_t hit_count_;
  uint32_t miss_count_;
#endif
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_RESOURCE_MANAGER_H_
