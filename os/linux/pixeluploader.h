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

#ifndef OS_LINUX_PIXELUPLOADER_H_
#define OS_LINUX_PIXELUPLOADER_H_

#include <spinlock.h>
#include <platformdefines.h>

#include <memory>
#include <vector>

#include "factory.h"
#include "hwcthread.h"

#include "fdhandler.h"
#include "hwcevent.h"

namespace hwcomposer {

class NativeBufferHandler;

class PixelUploaderCallback {
 public:
  virtual ~PixelUploaderCallback() {
  }
  virtual void Callback(bool start_access, void* call_back_data) = 0;
};

class PixelUploaderLayerCallback {
 public:
  virtual ~PixelUploaderLayerCallback() {
  }
  virtual void UploadDone() = 0;
};

class PixelUploader : public HWCThread {
 public:
  PixelUploader(const NativeBufferHandler* buffer_handler);
  ~PixelUploader() override;

  void Initialize(uint32_t gpu_fd);

  void RegisterPixelUploaderCallback(
      std::shared_ptr<PixelUploaderCallback> callback);

  void UpdateLayerPixelData(HWCNativeHandle handle, uint32_t original_height,
                            uint32_t original_stride, void* callback_data,
                            uint8_t* byteaddr,
                            PixelUploaderLayerCallback* layer_callback);

  const NativeBufferHandler* GetNativeBufferHandler() const {
    return buffer_handler_;
  }

  void HandleRoutine() override;
  void HandleExit() override;
  void ExitThread();

  void Synchronize();

 private:
  enum Tasks {
    kNone = 0,  // No tasks
    kRefreshRawPixelMap = 1 << 1,
    kHandleTextureUpload = 1 << 2
  };

  struct PixelData {
    HWCNativeHandle handle_;
    uint32_t original_height_ = 0;
    uint32_t original_stride_ = 0;
    void* callback_data_ = 0;
    uint8_t* data_ = NULL;
    PixelUploaderLayerCallback* layer_callback_ = NULL;
  };

  void HandleRawPixelUpdate();
  void* Map(uint32_t prime_fd, size_t size);
  void Unmap(uint32_t prime_fd, void* addr, size_t size);

  std::shared_ptr<PixelUploaderCallback> callback_ = NULL;
  SpinLock tasks_lock_;
  SpinLock pixel_data_lock_;
  std::vector<PixelData> pixel_data_;
  uint32_t tasks_ = kNone;
  uint32_t gpu_fd_ = 0;
  const NativeBufferHandler* buffer_handler_ = NULL;
};

}  // namespace hwcomposer
#endif  // COMMON_COMPOSITOR_PixelUploader_H_
