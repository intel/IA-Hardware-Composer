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

#include "hwclayerbuffermanager.h"

namespace hwcomposer {

HwcLayerBufferManager::HwcLayerBufferManager(
    NativeBufferHandler* buffer_handler)
    : buffer_handler_(buffer_handler) {
  for (size_t i = 0; i < BUFFER_CACHE_LENGTH; i++)
    cached_buffers_.emplace_back();
}

HwcLayerBufferManager::~HwcLayerBufferManager() {
    PurgeBuffer();
}

void HwcLayerBufferManager::PurgeBuffer() {
  for(auto &map : cached_buffers_) {
	map.clear();
  }
}

void HwcLayerBufferManager::Dump() {
}

std::shared_ptr<OverlayBuffer>& HwcLayerBufferManager::FindCachedBuffer(const HWCNativeBuffer& native_buffer) {

  BUFFER_MAP&  first_map = cached_buffers_[0];
  static std::shared_ptr<OverlayBuffer> pBufNull = nullptr;
  for (auto& map : cached_buffers_) {
    if (map.count(native_buffer)) {
      std::shared_ptr<OverlayBuffer>& pBuf = map[native_buffer];
      if (&map != &first_map) {
        first_map.emplace(std::make_pair(native_buffer, pBuf));
        map.erase(native_buffer);
      }
#ifdef CACHE_TRACING
      hit_count_++;
#endif
      return pBuf;
    }
  }

#ifdef CACHE_TRACING
  miss_count_++;
  if (miss_count_ % 100 == 0)
    ICACHETRACE("cache miss count is %llu, while hit count is %llu",
                miss_count_, hit_count_);
#endif

  return pBufNull;
}

void HwcLayerBufferManager::RegisterBuffer(const HWCNativeBuffer& native_buffer, std::shared_ptr<OverlayBuffer>& pBuffer) {

  BUFFER_MAP&  first_map = cached_buffers_[0];
  first_map.emplace(std::make_pair(native_buffer, pBuffer));
}

void HwcLayerBufferManager::MarkResourceForDeletion(
    const ResourceHandle& handle) {
  purged_resources_.emplace_back();
  ResourceHandle& temp = purged_resources_.back();
  temp.handle_ = handle.handle_;
  temp.image_ = handle.image_;
  temp.texture_ = handle.texture_;
  temp.fb_ = handle.fb_;
}

void HwcLayerBufferManager::RefreshBufferCache() {
  auto begin = cached_buffers_.begin();
  cached_buffers_.emplace(begin);
  cached_buffers_.pop_back();
}

void HwcLayerBufferManager::ResetPurgedResources() {
  std::vector<ResourceHandle>().swap(purged_resources_);
}

}  // namespace hwcomposer
