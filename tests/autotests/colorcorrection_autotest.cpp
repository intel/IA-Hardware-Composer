/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gpudevice.h>
#include <hwcbuffer.h>
#include <hwclayer.h>
#include <nativebufferhandler.h>
#include <nativedisplay.h>
#include <platformdefines.h>
#include <nativefence.h>
#include <spinlock.h>
#include <sys/mman.h>
#include <igt.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static uint64_t arg_frames = 0;

/* keep multiple frames, layers support for future usages */
struct frame {
  std::vector<std::unique_ptr<hwcomposer::HwcLayer>> layers;
  std::vector<std::vector<std::unique_ptr<hwcomposer::NativeFence>>>
      layers_fences;
  std::vector<int32_t> fences;
  HWCNativeHandle handle_;
  HwcBuffer bo_;
};

static struct frame test_frame;

class DisplayVSyncCallback : public hwcomposer::VsyncCallback {
 public:
  DisplayVSyncCallback() {
  }

  void Callback(uint32_t /*display */, int64_t /*timestamp */) {
  }
};

class HotPlugEventCallback : public hwcomposer::DisplayHotPlugEventCallback {
 public:
  HotPlugEventCallback(hwcomposer::GpuDevice *device) : device_(device) {
  }

  void Callback(std::vector<hwcomposer::NativeDisplay *> connected_displays) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    connected_displays_.swap(connected_displays);
    for (auto &display : connected_displays_) {
      auto callback = std::make_shared<DisplayVSyncCallback>();
      display->RegisterVsyncCallback(callback, 0);
      display->VSyncControl(true);
    }
  }

  const std::vector<hwcomposer::NativeDisplay *> &GetConnectedDisplays() {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    return connected_displays_;
  }

  void PresentLayers(
      std::vector<hwcomposer::HwcLayer *> &layers,
      std::vector<std::vector<std::unique_ptr<hwcomposer::NativeFence>>> &
          layers_fences,
      std::vector<int32_t> &fences) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      int32_t retire_fence = -1;
      display->Present(layers, &retire_fence);
      fences.emplace_back(retire_fence);
      // store fences for each display for each layer
      unsigned int fence_index = 0;
      for (auto layer : layers) {
        hwcomposer::NativeFence *fence = new hwcomposer::NativeFence();
        fence->Reset(layer->release_fence.Release());
        layers_fences[fence_index].emplace_back(fence);
      }
    }
  }

  void SetGamma(float red, float green, float blue) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      display->SetGamma(red, green, blue);
    }
  }

  void SetBrightness(char red, char green, char blue) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      display->SetBrightness(red, green, blue);
    }
  }

  void SetContrast(char red, char green, char blue) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      display->SetContrast(red, green, blue);
    }
  }

  void SetBroadcastRGB(const char *range_property) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->SetBroadcastRGB(range_property);
  }

  void SetPowerMode(uint32_t power_mode) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->SetPowerMode(power_mode);
  }

  void PopulateConnectedDisplays() {
    if (connected_displays_.empty()) {
      connected_displays_ = device_->GetConnectedPhysicalDisplays();

      for (auto &display : connected_displays_) {
        auto callback = std::make_shared<DisplayVSyncCallback>();
        display->RegisterVsyncCallback(callback, 0);
        display->VSyncControl(true);
      }
    }
  }

 private:
  std::vector<hwcomposer::NativeDisplay *> connected_displays_;
  hwcomposer::GpuDevice *device_;
  hwcomposer::SpinLock spin_lock_;
};

hwcomposer::NativeBufferHandler *buffer_handler;

static void init_frame(int32_t width, int32_t height) {
  if (!buffer_handler->CreateBuffer(width, height, 0, &test_frame.handle_)) {
    ETRACE("ColorCorrection: CreateBuffer failed");
    exit(EXIT_FAILURE);
  }

  if (!buffer_handler->ImportBuffer(test_frame.handle_, &test_frame.bo_)) {
    ETRACE("ColorCorrection: CreateBuffer failed");
    exit(EXIT_FAILURE);
  }

  test_frame.layers_fences.resize(1);
  hwcomposer::HwcLayer *hwc_layer = NULL;
  hwc_layer = new hwcomposer::HwcLayer();
  hwc_layer->SetTransform(0);
  hwc_layer->SetSourceCrop(hwcomposer::HwcRect<float>(0, 0, width, height));
  hwc_layer->SetDisplayFrame(hwcomposer::HwcRect<int>(0, 0, width, height));
  hwc_layer->SetNativeHandle(test_frame.handle_);
  test_frame.layers.push_back(std::unique_ptr<hwcomposer::HwcLayer>(hwc_layer));
}

void draw_colors(void *ptr, uint32_t height, uint32_t stride, bool gradient) {
  uint32_t c_offiset = 4;
  uint32_t c_value = 255;

  memset(ptr, 0, height * stride);
  int color_height = height / 3;

  if (color_height <= 0)
    return;

  for (int i = 0; i < height; i++) {
    if (i < color_height) {
      c_offiset = 0;  // blue
    } else if (i >= color_height && i < color_height * 2) {
      c_offiset = 1;  // green
    } else {
      c_offiset = 2;  // red
    }
    if (gradient) {
      c_value = 255 * ((float)(i % color_height) / color_height);
    }

    if (c_value > 255) {
      c_value = 255;
    } else if (c_value <= 0) {
      c_value = 1;
    }

    for (int j = c_offiset; j < stride; j = j + 4) {
      ((char *)ptr)[i * stride + j] = c_value;
    }
  }
}

void get_crc_list(const std::vector<hwcomposer::NativeDisplay *> displays,
                  igt_crc_t *crc_list) {
  int i = 0;
  for (auto &display : displays) {
    uint32_t pipe_id = -1;
    pipe_id = display->GetDisplayPipe();
    if (pipe_id == -1) {  // unconnected display
      continue;
    }
    igt_pipe_crc_t *pipe_crc = pipe_crc_new(pipe_id);
    igt_crc_t crc_fullcolors;
    igt_pipe_crc_collect_crc(pipe_crc, &crc_fullcolors);
    crc_list[i] = crc_fullcolors;
    free(pipe_crc);
    i++;
  }
}

int main(int argc, char *argv[]) {
  struct drm_fb *fb;
  int ret = 0, fd, primary_width, primary_height;
  hwcomposer::GpuDevice device;
  device.Initialize();
  auto callback = std::make_shared<HotPlugEventCallback>(&device);
  device.RegisterHotPlugEventCallback(callback);
  const std::vector<hwcomposer::NativeDisplay *> &displays =
      callback->GetConnectedDisplays();
  if (displays.empty())
    return ret;

  fd = open("/dev/dri/renderD128", O_RDWR);
  if (fd == -1) {
    printf("Can't open GPU file");
    exit(-1);
  }
  primary_width = displays.at(0)->Width();
  primary_height = displays.at(0)->Height();

  buffer_handler = hwcomposer::NativeBufferHandler::CreateInstance(fd);

  if (!buffer_handler)
    exit(-1);

  init_frame(primary_width, primary_height);

  void *pOpaque = NULL;
  uint32_t mapStride;
  void *pBo =
      buffer_handler->Map(test_frame.handle_, 0, 0, test_frame.bo_.width,
                          test_frame.bo_.height, &mapStride, &pOpaque, 0);
  if (!pBo) {
    ret = 1;
    printf("gbm_bo_map is not successful!");
    delete buffer_handler;
    return ret;
  }

  std::vector<hwcomposer::HwcLayer *> layers;

  igt_crc_t *solid_crc_list = (igt_crc_t *)malloc(sizeof(igt_crc_t) * 16);
  igt_crc_t *gamma_crc_list = (igt_crc_t *)malloc(sizeof(igt_crc_t) * 16);
  memset(solid_crc_list, 0, sizeof(igt_crc_t) * 16);
  memset(gamma_crc_list, 0, sizeof(igt_crc_t) * 16);

  callback->SetBroadcastRGB("Full");
  // Show solid colors
  for (int32_t &fence : test_frame.fences) {
    if (fence == -1)
      continue;

    sync_wait(fence, -1);
    close(fence);
    fence = -1;
  }

  for (auto &fence : test_frame.layers_fences[0]) {
    if (fence->get() != -1) {
      ret = sync_wait(fence->get(), 1000);
    }
  }
  test_frame.layers_fences[0].clear();
  std::vector<hwcomposer::HwcLayer *>().swap(layers);
  draw_colors(pBo, test_frame.bo_.height, test_frame.bo_.pitches[0], false);
  test_frame.layers[0]->acquire_fence.Reset(-1);
  layers.emplace_back(test_frame.layers[0].get());
  callback->PresentLayers(layers, test_frame.layers_fences, test_frame.fences);
  get_crc_list(displays, solid_crc_list);

  // set gamma to remap gradient to solid
  callback->SetGamma(0, 0, 0);
  for (int32_t &fence : test_frame.fences) {
    if (fence == -1)
      continue;

    sync_wait(fence, -1);
    close(fence);
    fence = -1;
  }

  for (auto &fence : test_frame.layers_fences[0]) {
    if (fence->get() != -1) {
      ret = sync_wait(fence->get(), 1000);
    }
  }
  test_frame.layers_fences[0].clear();
  std::vector<hwcomposer::HwcLayer *>().swap(layers);
  draw_colors(pBo, test_frame.bo_.height, test_frame.bo_.pitches[0], true);
  test_frame.layers[0]->acquire_fence.Reset(-1);
  layers.emplace_back(test_frame.layers[0].get());
  callback->PresentLayers(layers, test_frame.layers_fences, test_frame.fences);
  get_crc_list(displays, gamma_crc_list);

  buffer_handler->UnMap(test_frame.handle_, pOpaque);

  // restore default gamma and broadcast RGB
  callback->SetGamma(1, 1, 1);
  callback->SetBroadcastRGB("Automatic");
  close(fd);

  // comparing crc list
  ret = 0;
  for (int i = 0; i < 16; i++) {
    if (!igt_assert_crc_equal(solid_crc_list + i, gamma_crc_list + i)) {
      ret = 1;
      break;
    }
  }

  free(solid_crc_list);
  free(gamma_crc_list);

  if (!ret) {
    printf("\nPASSED\n");
  } else {
    printf("\nFAILED\n");
  }

  delete buffer_handler;
  exit(ret);
}
