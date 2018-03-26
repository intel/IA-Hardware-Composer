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

#include "framebuffermanager.h"

namespace hwcomposer {

FrameBufferManager *FrameBufferManager::pInstance = NULL;

FrameBufferManager *FrameBufferManager::GetInstance() {
  if (pInstance == NULL)
    pInstance = new FrameBufferManager();
  return pInstance;
}

uint32_t FrameBufferManager::FindFB(const FBKey &key) {
  lock_.lock();
  auto it = fb_map_.find(key);
  if (it != fb_map_.end()) {
    it->second.fb_ref++;
    lock_.unlock();
    return it->second.fb_id;
  } else {
    uint32_t fb_id = 0;
    int ret = drmModeAddFB2(key.gpu_fd, key.width, key.height,
                            key.frame_buffer_format, key.gem_handles,
                            key.pitches, key.offsets, &fb_id, 0);

    if (ret) {
      ETRACE("drmModeAddFB2 error (%dx%d, %c%c%c%c, handle %d pitch %d) (%s)",
             key.width, key.height, key.frame_buffer_format,
             key.frame_buffer_format >> 8, key.frame_buffer_format >> 16,
             key.frame_buffer_format >> 24, key.gem_handles[0], key.pitches[0],
             strerror(-ret));
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

int FrameBufferManager::RemoveFB(const int32_t fb, bool &real_moved) {
  lock_.lock();
  auto it = fb_map_.begin();
  int ret = 0;
  real_moved = false;

  while (it != fb_map_.end()) {
    if (it->second.fb_id == fb) {
      it->second.fb_ref -= 1;
      if (it->second.fb_ref == 0) {
        ret = drmModeRmFB(it->first.gpu_fd, fb);
        fb_map_.erase(it);
        real_moved = true;
      }
      break;
    }
    it++;
  }

  lock_.unlock();
  return ret;
}

void FrameBufferManager::PurgeAllFBs() {

  lock_.lock();
  auto it = fb_map_.begin();

  while (it != fb_map_.end()) {
    drmModeRmFB(it->first.gpu_fd, it->second.fb_id);
  }
  lock_.unlock();
}

} // namespace hwcomposer
