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

#ifndef COMMON_CORE_NESTEDDISPLAY_H_
#define COMMON_CORE_NESTEDDISPLAY_H_

#include <stdint.h>
#include <stdlib.h>

#include <drm_fourcc.h>
#include <nativedisplay.h>
#include <memory>
#ifdef NESTED_DISPLAY_SUPPORT
#include <linux/hyper_dmabuf.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <utils/threads.h>
#include <map>
#include "compositor.h"
#include "drmbuffer.h"
#include "hwcthread.h"
#include "hwctrace.h"
#include "resourcemanager.h"

#define SURFACE_NAME_LENGTH 64
#define METADATA_BUFFER_SIZE 12000
#define METADATA_STREAM_START 0xF00D
#define METADATA_STREAM_END 0xCAFE
static char buf[METADATA_BUFFER_SIZE];
#define HYPER_DMABUF_PATH "/dev/hyper_dmabuf"
#endif

namespace hwcomposer {

struct HwcLayer;
class FrameBufferManager;
class NativeBufferHandler;
class NestedDisplayManager;

#ifdef NESTED_DISPLAY_SUPPORT
struct vm_header {
  int32_t version;
  int32_t output;
  int32_t counter;
  int32_t n_buffers;
  int32_t disp_w;
  int32_t disp_h;
};

struct vm_buffer_info {
  int32_t surf_index;
  int32_t width, height;
  int32_t format;
  int32_t pitch[3];
  int32_t offset[3];
  int32_t tile_format;
  int32_t rotation;
  int32_t status;
  int32_t counter;
  union {
    hyper_dmabuf_id_t hyper_dmabuf_id;
    unsigned long ggtt_offset;
  };
  char surface_name[SURFACE_NAME_LENGTH];
  uint64_t surface_id;
  int32_t bbox[4];
};

class SocketThread : public HWCThread {
 public:
  SocketThread(int *client, bool *connection, int server);
  ~SocketThread();
  void Initialize();
  void SetEnabled(bool enabled);
  void HandleRoutine() override;

 private:
  bool mEnabled;
  int *client_sock_fd;
  bool *connected;
  int sock_fd;
};
#endif

class NestedDisplay : public NativeDisplay {
 public:
  NestedDisplay(uint32_t gpu_fd, NativeBufferHandler *buffer_handler);
  ~NestedDisplay() override;

  void InitNestedDisplay(uint32_t width, uint32_t height,
                         uint32_t port) override;

  bool Initialize(NativeBufferHandler *buffer_handler,
                  FrameBufferManager * /*frame_buffer_manager*/) override;

  DisplayType Type() const override {
    return DisplayType::kNested;
  }

  uint32_t Width() const override {
    return width_;
  }

  uint32_t Height() const override {
    return height_;
  }

  uint32_t PowerMode() const override {
    return 0;
  }

  int GetDisplayPipe() override;
  bool SetActiveConfig(uint32_t config) override;
  bool GetActiveConfig(uint32_t *config) override;

  bool SetPowerMode(uint32_t power_mode) override;

  bool Present(std::vector<HwcLayer *> &source_layers, int32_t *retire_fence,
               PixelUploaderCallback *call_back = NULL,
               bool handle_constraints = false) override;

  int RegisterVsyncCallback(std::shared_ptr<VsyncCallback> callback,
                            uint32_t display_id) override;

  void RegisterRefreshCallback(std::shared_ptr<RefreshCallback> callback,
                               uint32_t display_id) override;

  void RegisterHotPlugCallback(std::shared_ptr<HotPlugCallback> callback,
                               uint32_t display_id) override;

  void VSyncControl(bool enabled) override;
  bool CheckPlaneFormat(uint32_t format) override;
  void SetGamma(float red, float green, float blue) override;
  void SetContrast(uint32_t red, uint32_t green, uint32_t blue) override;
  void SetBrightness(uint32_t red, uint32_t green, uint32_t blue) override;
  void SetExplicitSyncSupport(bool disable_explicit_sync) override;

  bool IsConnected() const override;

  void UpdateScalingRatio(uint32_t primary_width, uint32_t primary_height,
                          uint32_t display_width,
                          uint32_t display_height) override;

  void CloneDisplay(NativeDisplay *source_display) override;

  bool PresentClone(NativeDisplay * /*display*/) override;

  bool GetDisplayAttribute(uint32_t /*config*/, HWCDisplayAttribute attribute,
                           int32_t *value) override;

  bool GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) override;
  bool GetDisplayName(uint32_t *size, char *name) override;

  bool EnableVSync() const {
    return enable_vsync_;
  }

  void VSyncUpdate(int64_t timestamp);

  void RefreshUpdate();

  void HotPlugUpdate(bool connected);
#ifdef NESTED_DISPLAY_SUPPORT
  int StartSockService();
  int HyperCommunicationNetworkSendData(void *data, int len);
  static void SignalCallbackHandler(int signum);
#endif

 private:
  std::shared_ptr<RefreshCallback> refresh_callback_ = NULL;
  std::shared_ptr<VsyncCallback> vsync_callback_ = NULL;
  std::shared_ptr<HotPlugCallback> hotplug_callback_ = NULL;
  uint32_t display_id_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t port_ = 0;
  bool enable_vsync_ = false;
  bool mconnected = false;
  uint32_t config_ = 1;

#ifdef NESTED_DISPLAY_SUPPORT
  int mHyperDmaBuf_Fd = -1;
  std::map<HWCNativeHandle, vm_buffer_info>
      mHyperDmaExportedBuffers;  // Track the hyper dmabuf metadata info mapping
  static std::unique_ptr<SocketThread> st_;
  int msock_fd = -1;
  static int mclient_sock_fd;
  std::unique_ptr<ResourceManager> resource_manager_;
  FrameBufferManager *fb_manager_ = NULL;
  Compositor compositor_;
#endif
};

}  // namespace hwcomposer
#endif  // COMMON_CORE_NESTEDDISPLAY_H_
