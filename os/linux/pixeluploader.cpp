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

#include "pixeluploader.h"

#include "hwcutils.h"
#include "hwctrace.h"
#include "nativegpuresource.h"
#include "overlaylayer.h"
#include "renderer.h"
#include "resourcemanager.h"
#include "framebuffermanager.h"
#include "displayplanemanager.h"
#include "nativesurface.h"

#include <nativebufferhandler.h>

#include <sys/mman.h>

namespace hwcomposer {

#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)
#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

struct dma_buf_sync {
  __u64 flags;
};

PixelUploader::PixelUploader(const NativeBufferHandler* buffer_handler)
    : HWCThread(-8, "PixelUploader"), buffer_handler_(buffer_handler) {
}

PixelUploader::~PixelUploader() {
}

void PixelUploader::Initialize(uint32_t gpu_fd) {
  gpu_fd_ = gpu_fd;
  if (!InitWorker()) {
    ETRACE("Failed to initalize PixelUploader. %s", PRINTERROR());
  }
}

void PixelUploader::RegisterPixelUploaderCallback(
    std::shared_ptr<RawPixelUploadCallback> callback) {
  callback_ = callback;
}

void PixelUploader::UpdateLayerPixelData(
    HWCNativeHandle handle, uint32_t original_height, uint32_t original_stride,
    void* callback_data, uint8_t* byteaddr,
    PixelUploaderLayerCallback* layer_callback) {
  pixel_data_lock_.lock();
  pixel_data_.emplace_back();
  PixelData& temp = pixel_data_.back();
  temp.handle_ = handle;
  temp.original_height_ = original_height;
  temp.original_stride_ = original_stride;
  temp.callback_data_ = callback_data;
  temp.data_ = byteaddr;
  temp.layer_callback_ = layer_callback;
  pixel_data_lock_.unlock();

  tasks_lock_.lock();
  tasks_ |= kRefreshRawPixelMap;
  tasks_lock_.unlock();

  Resume();
}

void PixelUploader::Synchronize() {
  pixel_data_lock_.lock();
  pixel_data_lock_.unlock();
}

void PixelUploader::ExitThread() {
  HWCThread::Exit();
  std::vector<PixelData>().swap(pixel_data_);
}

void PixelUploader::HandleExit() {
}

void PixelUploader::HandleRoutine() {
  HandleRawPixelUpdate();
}

void PixelUploader::HandleRawPixelUpdate() {
  tasks_lock_.lock();
  tasks_ &= ~kRefreshRawPixelMap;
  tasks_lock_.unlock();

  pixel_data_lock_.lock();
  if (pixel_data_.empty()) {
    pixel_data_lock_.unlock();
    return;
  }

  std::vector<PixelData> texture_uploads;
  for (auto& buffer : pixel_data_) {
    texture_uploads.emplace_back(buffer);
  }

  std::vector<PixelData>().swap(pixel_data_);

  for (auto& buffer : texture_uploads) {
    if (callback_) {
      // Notify everyone that we are going to access this data.
      callback_->Callback(true, buffer.callback_data_);
    }

    uint8_t* ptr = NULL;
    size_t size = buffer.handle_->meta_data_.height_ *
                  buffer.handle_->meta_data_.pitches_[0];
    uint32_t mapStride = buffer.original_stride_;
    uint32_t prime_fd = buffer.handle_->meta_data_.prime_fds_[0];
    if (prime_fd > 0) {
      ptr = (uint8_t*)Map(buffer.handle_->meta_data_.prime_fds_[0], size);
    }

    if (!ptr) {
      // FIXME: Create texture and do texture upload.
    } else {
      for (int i = 0; i < buffer.original_height_; i++) {
        memcpy(ptr + i * buffer.handle_->meta_data_.pitches_[0],
               buffer.data_ + i * mapStride, mapStride);
      }
    }

    if (ptr)
      Unmap(buffer.handle_->meta_data_.prime_fds_[0], ptr, size);

    if (callback_) {
      // Notify everyone that we are done accessing this data.
      callback_->Callback(false, buffer.callback_data_);
    }

    if (buffer.layer_callback_) {
      buffer.layer_callback_->UploadDone();
    }
    pixel_data_lock_.unlock();
  }
}

void* PixelUploader::Map(uint32_t prime_fd, size_t size) {
  void* addr =
      mmap(nullptr, size, (PROT_READ | PROT_WRITE), MAP_SHARED, prime_fd, 0);
  if (addr == MAP_FAILED)
    return nullptr;

  struct dma_buf_sync sync_start = {0};
  sync_start.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
  int rv = ioctl(prime_fd, DMA_BUF_IOCTL_SYNC, &sync_start);
  if (rv) {
    ETRACE("DMA_BUF_IOCTL_SYNC failed during Map \n");
    munmap(addr, size);
    return nullptr;
  }

  return addr;
}

void PixelUploader::Unmap(uint32_t prime_fd, void* addr, size_t size) {
  if (addr) {
    struct dma_buf_sync sync_start = {0};
    sync_start.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    ioctl(prime_fd, DMA_BUF_IOCTL_SYNC, &sync_start);
    munmap(addr, size);
    addr = nullptr;
  }
}

}  // namespace hwcomposer
