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

#include "resourcemanager.h"

namespace hwcomposer {

ResourceManager::ResourceManager(NativeBufferHandler* buffer_handler)
    : buffer_handler_(buffer_handler) {
  for (size_t i = 0; i < BUFFER_CACHE_LENGTH; i++)
    cached_buffers_.emplace_back();
}

ResourceManager::~ResourceManager() {
  if (cached_buffers_.size() > 0) {
    ETRACE("ResourceManager destroyed with valid native resources \n");
  }

  if (purged_resources_.size() > 0) {
    ETRACE("ResourceManager destroyed with valid 3D resources \n");
  }

  if (purged_media_resources_.size() > 0) {
    ETRACE("ResourceManager destroyed with valid Media resources \n");
  }
}

void ResourceManager::PurgeBuffer() {
  for (auto& map : cached_buffers_) {
    map.clear();
  }
}

void ResourceManager::Dump() {
}

std::shared_ptr<OverlayBuffer>& ResourceManager::FindCachedBuffer(
    const HWCNativeBuffer& native_buffer) {
  BUFFER_MAP& first_map = cached_buffers_[0];
  static std::shared_ptr<OverlayBuffer> pBufNull = nullptr;
  for (auto& map : cached_buffers_) {
    if (map.count(native_buffer)) {
      std::shared_ptr<OverlayBuffer>& pBuf = map[native_buffer];
      if (&map != &first_map) {
        first_map.emplace(std::make_pair(native_buffer, pBuf));
        map.erase(native_buffer);
      }
#ifdef RESOURCE_CACHE_TRACING
      hit_count_++;
#endif
      return pBuf;
    }
  }

#ifdef RESOURCE_CACHE_TRACING
  miss_count_++;
  if (miss_count_ % 100 == 0)
    ICACHETRACE("cache miss count is %llu, while hit count is %llu",
                miss_count_, hit_count_);
#endif

  return pBufNull;
}

void ResourceManager::RegisterBuffer(const HWCNativeBuffer& native_buffer,
                                     std::shared_ptr<OverlayBuffer>& pBuffer) {
  BUFFER_MAP& first_map = cached_buffers_[0];
  first_map.emplace(std::make_pair(native_buffer, pBuffer));
}

void ResourceManager::MarkResourceForDeletion(const ResourceHandle& handle,
                                              bool has_valid_gpu_resources) {
  lock_.lock();
  purged_resources_.emplace_back();
  ResourceHandle& temp = purged_resources_.back();
  std::memcpy(&temp, &handle, sizeof temp);
  if (!has_purged_gpu_resources_)
    has_purged_gpu_resources_ = has_valid_gpu_resources;
  lock_.unlock();
}

void ResourceManager::MarkMediaResourceForDeletion(
    const MediaResourceHandle& handle) {
  lock_.lock();
  purged_media_resources_.emplace_back();
  MediaResourceHandle& temp = purged_media_resources_.back();
  std::memcpy(&temp, &handle, sizeof temp);
  lock_.unlock();
}

void ResourceManager::GetPurgedResources(
    std::vector<ResourceHandle>& gl_resources,
    std::vector<MediaResourceHandle>& media_resources, bool* has_gpu_resource) {
  lock_.lock();
  size_t purged_size = purged_resources_.size();
  *has_gpu_resource = has_purged_gpu_resources_;

  if (purged_size != 0) {
    for (size_t i = 0; i < purged_size; i++) {
      const ResourceHandle& handle = purged_resources_.at(i);
      gl_resources.emplace_back();
      ResourceHandle& temp = gl_resources.back();
      std::memcpy(&temp, &handle, sizeof temp);
    }

    std::vector<ResourceHandle>().swap(purged_resources_);
    has_purged_gpu_resources_ = false;
  }

  purged_size = purged_media_resources_.size();
  if (purged_size != 0) {
    for (size_t i = 0; i < purged_size; i++) {
      const MediaResourceHandle& handle = purged_media_resources_.at(i);
      media_resources.emplace_back();
      MediaResourceHandle& temp = media_resources.back();
      std::memcpy(&temp, &handle, sizeof temp);
    }

    std::vector<MediaResourceHandle>().swap(purged_media_resources_);
  }

  lock_.unlock();
}

bool ResourceManager::HasPurgedResources() {
  lock_.lock();
  bool status = false;
  if (!purged_resources_.empty() || !purged_media_resources_.empty())
    status = true;

  lock_.unlock();

  return status;
}

void ResourceManager::RefreshBufferCache() {
  auto begin = cached_buffers_.begin();
  cached_buffers_.emplace(begin);
  cached_buffers_.pop_back();
}

}  // namespace hwcomposer
