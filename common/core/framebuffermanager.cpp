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

FrameBufferManager *FrameBufferManager::pInstance = NULL;

FrameBufferManager *FrameBufferManager::GetInstance(uint32_t gpu_fd) {
  if (pInstance == NULL)
    pInstance = new FrameBufferManager(gpu_fd);
  return pInstance;
}

uint32_t FrameBufferManager::FindFB(const uint32_t &iwidth,
                                    const uint32_t &iheight,
                                    const uint32_t &iframe_buffer_format,
                                    const uint32_t &num_planes,
                                    const uint32_t (&igem_handles)[4],
                                    const uint32_t (&ipitches)[4],
                                    const uint32_t (&ioffsets)[4]) {
  lock_.lock();
  FBKey key(num_planes, igem_handles);
  auto it = fb_map_.find(key);
  if (it != fb_map_.end()) {
    it->second.fb_ref++;
    lock_.unlock();
    return it->second.fb_id;
  } else {
    uint32_t fb_id = 0;
    int ret =
        CreateFrameBuffer(iwidth, iheight, iframe_buffer_format, igem_handles,
                          ipitches, ioffsets, gpu_fd_, &fb_id);
    if (ret) {
      lock_.unlock();
      return fb_id;
    }

    FBValue value;
    value.fb_id = fb_id;
    value.fb_ref = 1;
    fb_map_.emplace(std::make_pair(key, value));
    lock_.unlock();
    return fb_id;
  }
}

int FrameBufferManager::RemoveFB(const uint32_t &fb, bool release_gem_handles) {
  lock_.lock();
  auto it = fb_map_.begin();
  int ret = 0;

  while (it != fb_map_.end()) {
    if (it->second.fb_id == fb) {
      it->second.fb_ref -= 1;
      if (it->second.fb_ref == 0) {
        ret = ReleaseFrameBuffer(it->first, fb, gpu_fd_, release_gem_handles);
        fb_map_.erase(it);
      }
      break;
    }
    it++;
  }

  if(it == fb_map_.end()) {
    ETRACE("RemoveFB not meet!");
  }

  lock_.unlock();

  return ret;
}

void FrameBufferManager::PurgeAllFBs() {
  lock_.lock();
  auto it = fb_map_.begin();

  while (it != fb_map_.end()) {
    ReleaseFrameBuffer(it->first, it->second.fb_id, gpu_fd_, false);
  }

  lock_.unlock();
}

}  // namespace hwcomposer
