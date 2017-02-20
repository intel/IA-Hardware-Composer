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

#ifndef NATIVE_DISPLAY_H_
#define NATIVE_DISPLAY_H_

#include <stdint.h>

#include <hwcdefs.h>
#include <platformdefines.h>

typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeModeInfo drmModeModeInfo;

namespace hwcomposer {
struct HwcLayer;
class GpuDevice;

class VsyncCallback {
 public:
  virtual ~VsyncCallback() {
  }
  virtual void Callback(uint32_t display, int64_t timestamp) = 0;
};

class NativeDisplay {
 public:
  virtual ~NativeDisplay() {
  }

  virtual bool Initialize() = 0;

  virtual DisplayType Type() const = 0;

  virtual uint32_t Pipe() const = 0;

  virtual int32_t Width() const = 0;

  virtual int32_t Height() const = 0;

  virtual int32_t GetRefreshRate() const = 0;

  virtual uint32_t PowerMode() const = 0;

  virtual bool GetDisplayAttribute(uint32_t config,
                                   HWCDisplayAttribute attribute,
                                   int32_t *value) = 0;

  virtual bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) = 0;
  virtual bool GetDisplayName(uint32_t *size, char *name) = 0;
  virtual bool SetActiveConfig(uint32_t config) = 0;
  virtual bool GetActiveConfig(uint32_t *config) = 0;

  virtual bool SetDpmsMode(uint32_t dpms_mode) = 0;

  virtual bool Present(std::vector<HwcLayer *> &source_layers) = 0;

  virtual int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                                    uint32_t display_id) = 0;
  virtual void VSyncControl(bool enabled) = 0;

  // Virtual display related.
  virtual void InitVirtualDisplay(uint32_t /*width*/, uint32_t /*height*/) {
  }
  virtual void SetOutputBuffer(HWCNativeHandle /*buffer*/,
                               int32_t /*acquire_fence*/) {
  }

 protected:
  virtual uint32_t CrtcId() const = 0;
  virtual bool Connect(const drmModeModeInfo &mode_info,
                       const drmModeConnector *connector) = 0;

  virtual bool IsConnected() const = 0;

  virtual void DisConnect() = 0;

  virtual void ShutDown() = 0;

  friend class GpuDevice;
};
}  // namespace hwcomposer
#endif  // NATIVE_DISPLAY_H_
