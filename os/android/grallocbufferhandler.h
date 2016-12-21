/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GRALLOC_BUFFER_HANDLER_H_
#define GRALLOC_BUFFER_HANDLER_H_

#include <nativebufferhandler.h>

#include <hardware/gralloc.h>

enum {
  /* perform(const struct gralloc_module_t *mod,
   *	   int op,
   *	   uint32_t drm_fd,
   *	   buffer_handle_t buffer,
   *	   struct HwcBuffer *bo);
   */
  GRALLOC_MODULE_PERFORM_DRM_IMPORT = 0xffeeff00,
  /* perform(const struct gralloc_module_t *mod,
   *	   uint32_t width,
   *	   uint32_t height,
   *	   int format,
   *	   int usage,
   *	   buffer_handle_t *buffer);
   */
  GRALLOC_MODULE_PERFORM_CREATE_BUFFER = 0xffeeff01,
  /* perform(const struct gralloc_module_t *mod,
   *	   buffer_handle_t *buffer);
   */
  GRALLOC_MODULE_PERFORM_DESTROY_BUFFER = 0xffeeff02,
};

namespace hwcomposer {

class GpuDevice;

class GrallocBufferHandler : public NativeBufferHandler {
 public:
  GrallocBufferHandler(uint32_t fd);
  ~GrallocBufferHandler() override;

  bool Init();

  bool CreateBuffer(uint32_t w, uint32_t h, int format,
                    buffer_handle_t *handle) override;
  bool DestroyBuffer(buffer_handle_t handle) override;
  bool ImportBuffer(buffer_handle_t handle, HwcBuffer *bo) override;

 private:
  uint32_t ConvertHalFormatToDrm(uint32_t hal_format);
  uint32_t fd_;
  const gralloc_module_t *gralloc_;
};

}  // namespace hardware
#endif  // GRALLOC_BUFFER_HANDLER_H_
