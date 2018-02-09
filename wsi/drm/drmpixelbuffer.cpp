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

#include "drmpixelbuffer.h"

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

DrmPixelBuffer::DrmPixelBuffer() {
}

DrmPixelBuffer::~DrmPixelBuffer() {
}

void* DrmPixelBuffer::Map(uint32_t prime_fd, size_t size) {
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

void DrmPixelBuffer::Unmap(uint32_t prime_fd, void* addr, size_t size) {
  if (addr) {
    struct dma_buf_sync sync_start = {0};
    sync_start.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    ioctl(prime_fd, DMA_BUF_IOCTL_SYNC, &sync_start);
    munmap(addr, size);
    addr = nullptr;
  }
}

PixelBuffer* PixelBuffer::CreatePixelBuffer() {
  return new DrmPixelBuffer();
}
};
