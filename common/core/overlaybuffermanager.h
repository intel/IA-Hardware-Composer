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
#ifndef COMMON_CORE_OVERLAYBUFFERMANAGER_H_
#define COMMON_CORE_OVERLAYBUFFERMANAGER_H_

#include <platformdefines.h>

#include <nativebufferhandler.h>
#include <nativefence.h>
#include <spinlock.h>

#include <memory>
#include <vector>

#include "nativesync.h"
#include "overlaybuffer.h"

namespace hwcomposer {

class NativeBufferHandler;
class OverlayBufferManager;
struct OverlayLayer;

struct ImportedBuffer {
 public:
  ImportedBuffer(OverlayBuffer* const buffer,
                 OverlayBufferManager* buffer_manager, int release_fence)
      : buffer_(buffer),
        release_fence_(release_fence),
        buffer_manager_(buffer_manager) {
  }

  ~ImportedBuffer();

  OverlayBuffer* const buffer_;
  int release_fence_;
  bool owned_buffer_ = true;

 private:
  OverlayBufferManager* buffer_manager_;
};

class OverlayBufferManager {
 public:
  OverlayBufferManager() = default;
  OverlayBufferManager(OverlayBufferManager&& rhs) = default;
  OverlayBufferManager& operator=(OverlayBufferManager&& other) = default;

  ~OverlayBufferManager();

  bool Initialize(uint32_t gpu_fd);

  // Creates new ImportedBuffer for bo. Also, creates
  // a sync fence object associated for this buffer.
  // Sync fence is automatically signalled when buffer
  // is destroyed. RefCount of buffer is initialized to
  // 1.
  ImportedBuffer* CreateBuffer(const HwcBuffer& bo);

  // Creates new ImportedBuffer for handle. Also, creates
  // a sync fence object associated for this buffer.
  // Sync fence is automatically signalled when buffer
  // is destroyed. RefCount of buffer is initialized to
  // 1.
  ImportedBuffer* CreateBufferFromNativeHandle(HWCNativeHandle handle);

  // Increments RefCount of buffer by 1. Buffer will not be released
  // or associated fence object signalled until UnRegisterBuffer
  // is called and RefCount decreases to zero.
  void RegisterBuffer(const OverlayBuffer* const buffer);

  // Decreases RefCount of buffer by 1. Buffer will be released
  // and associated fence object will be signalled if RefCount
  // is equal to zero.
  void UnRegisterBuffer(const OverlayBuffer* const buffer);

  // Convenient function to call together RegisterBuffer for
  // OverlayBuffers.
  void RegisterBuffers(const std::vector<const OverlayBuffer*>& buffers);

  // Convenient function to call together UnRegisterBuffers for
  // layers.
  void UnRegisterLayerBuffers(std::vector<OverlayLayer>& layers);

  NativeBufferHandler* GetNativeBufferHandler() const {
    return buffer_handler_.get();
  }

 private:
  struct Buffer {
    std::unique_ptr<OverlayBuffer> buffer_;
    std::unique_ptr<NativeSync> sync_object_;
    uint32_t ref_count_ = 0;
  };

  std::vector<Buffer> buffers_;
  std::unique_ptr<NativeBufferHandler> buffer_handler_;
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_OVERLAYBUFFERMANAGER_H_
