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

#ifndef VIRTUAL_DISPLAY_H_
#define VIRTUAL_DISPLAY_H_

#include "headless.h"

#include <nativebufferhandler.h>

#include "compositor.h"

namespace hwcomposer {
struct HwcLayer;

class VirtualDisplay : public Headless {
 public:
  VirtualDisplay(uint32_t gpu_fd, NativeBufferHandler &handler,
                 uint32_t pipe_id, uint32_t crtc_id);
  ~VirtualDisplay();

  void InitVirtualDisplay(uint32_t width, uint32_t height) override;

  bool GetActiveConfig(uint32_t *config) override;

  bool Present(std::vector<HwcLayer *> &source_layers) override;

  void SetOutputBuffer(HWCNativeHandle buffer, int32_t acquire_fence) override;

 private:
  HWCNativeHandle output_handle_;
  int32_t acquire_fence_;
  NativeBufferHandler &buffer_handler_;
  Compositor compositor_;
  uint32_t gpu_fd_;
  uint32_t width_;
  uint32_t height_;
};

}  // namespace hwcomposer
#endif  // VIRTUAL_DISPLAY_H_
