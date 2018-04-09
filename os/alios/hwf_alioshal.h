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

#ifndef _ALIOS_HWC1_H_
#define _ALIOS_HWC1_H_

#include <cutils/hwflinger.h>
#include <cutils/hwflinger_defs.h>

#include <nativedisplay.h>
#include <hwclayer.h>
#include <gpudevice.h>
#include <fcntl.h>

#include <Hal.h>

#include <memory>

#define USER_FENCE_SYNC 0

#if 0
typedef struct hwf_module_wrapper_t
{
    hwf_module_t base;
    void *priv;          /**< Driver private data */
} hwf_module_wrapper_t;
#endif

namespace hwcomposer {

class DisplayTimeLine {
 public:
  int Init() {
#if USER_FENCE_SYNC
    timeline_fd_ = open("/dev/sw_sync", O_RDWR);
    if (timeline_fd_ < 0)
      return -1;

    return 0;

#else
    return -1;
#endif
  }

  ~DisplayTimeLine() {
#if USER_FENCE_SYNC
    if (timeline_fd_ > 0) {
      close(timeline_fd_);
    }
#endif
  }

  int32_t IncrementTimeLine() {
#if USER_FENCE_SYNC
    int ret =
        sw_sync_fence_create(timeline_fd_, "display fence", timeline_pt_ + 1);
    if (ret < 0) {
      LOG_E("Failed to create display fence %d %d", ret, timeline_fd_);
      return ret;
    }:

      int32_t ret_fd(ret);

    ret = sw_sync_timeline_inc(timeline_fd_, 1);
    if (ret) {
      LOG_E("Failed to increment display sync timeline %d", ret);
      return ret;
    }

    ++timeline_pt_;
    return ret_fd;

#else
    return -1;
#endif
  }

 private:
  int32_t timeline_fd_;
  int timeline_pt_ = 0;
};

typedef struct HwfLayer {
  ~HwfLayer() {
    delete hwc_layer_;  // TO DO
    hwc_layer_ = NULL;
  }

  HwfLayer() = default;

  HwfLayer(const HwfLayer& rhs) = delete;
  HwfLayer& operator=(const HwfLayer& rhs) = delete;

  struct yalloc_handle native_handle_;  // TO DO
  hwcomposer::HwcLayer* hwc_layer_ = NULL;
  uint32_t index_ = 0;

  int InitFromHwcLayer(hwf_layer_t* sf_layer);
} HwfLayer;

typedef struct HwfDisplay {
  // struct HwfDevice *ctx;
  hwcomposer::NativeDisplay* display_ = NULL;  // TO DO
  uint32_t display_id_ = 0;
  int32_t fence_ = -1;
  int last_render_layers_size = -1;
  std::vector<HwfLayer*> layers_;
  DisplayTimeLine timeline_;
  bool gl_composition_ = false;
} HwfDisplay;

struct HwfDevice {
  hwf_device_t base;

  //~HwfDevice() {
  //};

  // hwf_device_t device;
  // hwc_procs_t const *procs = NULL;

  hwcomposer::GpuDevice device_;
  std::vector<HwfDisplay> extended_displays_;
  HwfDisplay primary_display_;
  HwfDisplay virtual_display_;

  bool disable_explicit_sync_ = false;

  hwf_callback* m_phwf_callback;

  HwfDisplay* GetDisplay(int display);

  static int detect(struct hwf_device_t* device, int dispCount,
                    hwf_display_t** displays);

  static int flip(struct hwf_device_t* device, int dispCount,
                  hwf_display_t** displays);

  static int setEventState(struct hwf_device_t* device, int disp, int event,
                           int enabled);

  static int setDisplayState(struct hwf_device_t* device, int disp, int state);

  static int lookup(struct hwf_device_t* device, int what, int* value);

  static void registerCallback(struct hwf_device_t* device,
                               hwf_callback_t const* callback);

  static int queryDispConfigs(struct hwf_device_t* device, int disp,
                              uint32_t* configs, int* numConfigs);

  static int queryDispAttribs(struct hwf_device_t* device, int disp,
                              uint32_t config, const uint32_t* attributes,
                              int32_t* values);

  static void dump(struct hwf_device_t* device, char* buff, int buff_len);
};

class ia_hwf_yunhal {
 private:
  HwfDevice m_Device;

 public:
  ia_hwf_yunhal();
  ~ia_hwf_yunhal();

  HwfDevice* get_hwf_hw();
};
}

#endif
