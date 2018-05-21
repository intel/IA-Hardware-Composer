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

#include "framebuffermanager.h"

#include "platformcommondefines.h"

namespace hwcomposer {

void FrameBufferManager::RegisterGemHandles(const uint32_t &num_planes,
                                            const uint32_t (&igem_handles)[4]) {
  lock_.lock();
  FBKey key(num_planes, igem_handles);
  auto it = fb_map_.find(key);
  if (it != fb_map_.end()) {
    it->second.fb_ref++;
  } else {
    FBValue value;
    value.fb_ref = 1;
    value.fb_id = 0;
    value.fb_created = false;
    fb_map_.emplace(std::make_pair(key, value));
  }

  lock_.unlock();
}

uint32_t FrameBufferManager::FindFB(
    const uint32_t &iwidth, const uint32_t &iheight, const uint64_t &modifier,
    const uint32_t &iframe_buffer_format, const uint32_t &num_planes,
    const uint32_t (&igem_handles)[4], const uint32_t (&ipitches)[4],
    const uint32_t (&ioffsets)[4]) {
  lock_.lock();
  FBKey key(num_planes, igem_handles);
  uint32_t fb_id = 0;
  auto it = fb_map_.find(key);
  if (it != fb_map_.end()) {
    if (!it->second.fb_created) {
      it->second.fb_created = true;
      CreateFrameBuffer(iwidth, iheight, modifier, iframe_buffer_format,
                        num_planes, igem_handles, ipitches, ioffsets, gpu_fd_,
                        &it->second.fb_id);
    }

    fb_id = it->second.fb_id;
  } else {
    ETRACE("Handle not found in Cache \n");
  }

  lock_.unlock();
  return fb_id;
}

int FrameBufferManager::RemoveFB(uint32_t num_planes,
                                 const uint32_t (&igem_handles)[4]) {
  lock_.lock();

  int ret = 0;
  FBKey key(num_planes, igem_handles);

  auto it = fb_map_.find(key);
  if (it != fb_map_.end()) {
    it->second.fb_ref -= 1;
    if (it->second.fb_ref == 0) {
      ret = ReleaseFrameBuffer(it->first, it->second.fb_id, gpu_fd_);
      fb_map_.erase(it);
    }
  }

  if (it == fb_map_.end()) {
    if (igem_handles[0] != 0 || igem_handles[1] != 0 || igem_handles[2] != 0 ||
        igem_handles[3] != 0) {
      ETRACE("Unable to find fb in cache. %d %d %d %d \n", igem_handles[0],
             igem_handles[1], igem_handles[2], igem_handles[3]);
    }
  }

  lock_.unlock();

  return ret;
}

void FrameBufferManager::PurgeAllFBs() {
  lock_.lock();
  auto it = fb_map_.begin();

  while (it != fb_map_.end()) {
    ReleaseFrameBuffer(it->first, it->second.fb_id, gpu_fd_);
  }

  lock_.unlock();
}

}  // namespace hwcomposer
