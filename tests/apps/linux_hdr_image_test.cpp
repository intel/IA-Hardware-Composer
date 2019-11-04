/*
// Copyright (c) 2019 Intel Corporation
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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>

#include <i915_drm.h>
#include <intel_bufmgr.h>

#include <gpudevice.h>
#include <hwcdefs.h>
#include <hwclayer.h>
#include <nativedisplay.h>
#include <platformdefines.h>
#include <spinlock.h>

#include <nativebufferhandler.h>
#include "commondrmutils.h"
#include "hdr_metadata_defs.h"
#include "platformcommondefines.h"

#define NUM_BUFFERS 1

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif

#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010      \
  fourcc_code('P', '0', '1', \
              '0') /* 2x2 subsampled Cb:Cr plane 10 bits per channel */
#endif

#ifndef DRM_FORMAT_P012
#define DRM_FORMAT_P012      \
  fourcc_code('P', '0', '1', \
              '2') /* 2x2 subsampled Cr:Cb plane, 12 bit per channel */
#endif

#ifndef DRM_FORMAT_P016
#define DRM_FORMAT_P016      \
  fourcc_code('P', '0', '1', \
              '6') /* 2x2 subsampled Cr:Cb plane, 16 bit per channel */
#endif

struct buffer;

struct drm_device {
  int fd;
  char *name;

  int (*alloc_bo)(struct buffer *buf);
  void (*free_bo)(struct buffer *buf);
  int (*export_bo_to_prime)(struct buffer *buf);
  int (*map_bo)(struct buffer *buf);
  void (*unmap_bo)(struct buffer *buf);
  void (*device_destroy)(struct buffer *buf);
};

struct buffer {
  int busy;

  struct drm_device *dev;
  int drm_fd;

  drm_intel_bufmgr *bufmgr;
  drm_intel_bo *intel_bo;

  uint32_t gem_handle;
  int dmabuf_fd;
  uint8_t *mmap;

  int width;
  int height;
  int bpp;
  unsigned long stride;
  int format;
};

struct image {
  int fd;
  FILE *fp;
  int size;
  struct buffer buffers[NUM_BUFFERS];
  struct buffer *prev_buffer;
};

static int create_dmabuf_buffer(struct buffer *buffer, int width, int height,
                                int format);

static void drm_shutdown(struct buffer *my_buf);

static void destroy_dmabuf_buffer(struct buffer *buffer) {
  close(buffer->dmabuf_fd);
  buffer->dev->free_bo(buffer);
  drm_shutdown(buffer);
}

static struct buffer *image_next_buffer(struct image *s) {
  int i;

  for (i = 0; i < NUM_BUFFERS; i++)
    if (!s->buffers[i].busy)
      return &s->buffers[i];

  return NULL;
}

static void fill_buffer(struct buffer *buffer, struct image *image) {
  int i = 0;

  int frame_size = 0, y_size = 0;

  unsigned char *y_src = NULL, *u_src = NULL;
  unsigned char *y_dst = NULL, *u_dst = NULL;
  unsigned char *src_buffer = NULL;

  int bytes_per_pixel = 2;
  assert(buffer->mmap);

  frame_size = buffer->width * buffer->height * bytes_per_pixel * 3 / 2;
  y_size = buffer->width * buffer->height * bytes_per_pixel;

  src_buffer = (unsigned char *)malloc(frame_size);
  fread(src_buffer, 1, frame_size, image->fp);
  y_src = src_buffer;
  u_src = src_buffer + y_size;  // UV offset for P010

  fprintf(stderr, "Test width %d height %d stride %ld\n", buffer->width,
          buffer->height, buffer->stride);

  y_dst = (unsigned char *)buffer->mmap + 0;  // Y plane
  u_dst = (unsigned char *)buffer->mmap +
          buffer->stride * buffer->height;  // U offset for P010

  for (i = 0; i < buffer->height; i++) {
    memcpy(y_dst, y_src, buffer->width * 2);
    y_dst += buffer->stride;
    y_src += buffer->width * 2;
  }

  for (i = 0; i<buffer->height>> 1; i++) {
    memcpy(u_dst, u_src, buffer->width * 2);
    u_dst += buffer->stride;
    u_src += buffer->width * 2;
  }
}

static void image_close(struct image *s) {
  fclose(s->fp);
}

static bool image_open(struct image *image, const char *filename) {
  image->fp = fopen(filename, "r");
  return true;
}

static int intel_alloc_bo(struct buffer *my_buf) {
  /* XXX: try different tiling modes for testing FB modifiers. */
  uint32_t tiling = I915_TILING_NONE;

  assert(my_buf->bufmgr);

  my_buf->intel_bo = drm_intel_bo_alloc_tiled(
      my_buf->bufmgr, "test", my_buf->width, my_buf->height, (my_buf->bpp / 8),
      &tiling, &my_buf->stride, 0);

  if (!my_buf->intel_bo)
    return 0;

  if (tiling != I915_TILING_NONE)
    return 0;

  return 1;
}

static void intel_free_bo(struct buffer *my_buf) {
  drm_intel_bo_unreference(my_buf->intel_bo);
}

static int intel_map_bo(struct buffer *my_buf) {
  if (drm_intel_gem_bo_map_gtt(my_buf->intel_bo) != 0)
    return 0;

  my_buf->mmap = (uint8_t *)my_buf->intel_bo->virt;

  return 1;
}

static int intel_bo_export_to_prime(struct buffer *buffer) {
  return drm_intel_bo_gem_export_to_prime(buffer->intel_bo, &buffer->dmabuf_fd);
}

static void intel_unmap_bo(struct buffer *my_buf) {
  drm_intel_gem_bo_unmap_gtt(my_buf->intel_bo);
}

static void intel_device_destroy(struct buffer *my_buf) {
  drm_intel_bufmgr_destroy(my_buf->bufmgr);
}

static void drm_device_destroy(struct buffer *buf) {
  buf->dev->device_destroy(buf);
  close(buf->drm_fd);
}

static int drm_device_init(struct buffer *buf) {
  struct drm_device *dev = (drm_device *)calloc(1, sizeof(struct drm_device));

  drmVersionPtr version = drmGetVersion(buf->drm_fd);

  dev->fd = buf->drm_fd;
  dev->name = strdup(version->name);
  if (0) {
    /* nothing */
  } else if (!strcmp(dev->name, "i915")) {
    buf->bufmgr = drm_intel_bufmgr_gem_init(buf->drm_fd, 32);
    if (!buf->bufmgr) {
      free(dev->name);
      free(dev);
      return 0;
    }
    dev->alloc_bo = intel_alloc_bo;
    dev->free_bo = intel_free_bo;
    dev->export_bo_to_prime = intel_bo_export_to_prime;
    dev->map_bo = intel_map_bo;
    dev->unmap_bo = intel_unmap_bo;
    dev->device_destroy = intel_device_destroy;
  } else {
    fprintf(stderr, "Error: drm device %s unsupported.\n", dev->name);
    free(dev->name);
    free(dev);
    return 0;
  }
  buf->dev = dev;
  return 1;
}

static int drm_connect(struct buffer *my_buf) {
  /* This won't work with card0 as we need to be authenticated; instead,
   * boot with drm.rnodes=1 and use that. */
  my_buf->drm_fd = open("/dev/dri/renderD128", O_RDWR);
  if (my_buf->drm_fd < 0)
    return 0;

  return drm_device_init(my_buf);
}

static void drm_shutdown(struct buffer *my_buf) {
  drm_device_destroy(my_buf);
}

class DisplayVSyncCallback : public hwcomposer::VsyncCallback {
 public:
  DisplayVSyncCallback() {
  }

  void Callback(uint32_t /*display*/, int64_t /*timestamp*/) {
  }
};

class HotPlugEventCallback : public hwcomposer::DisplayHotPlugEventCallback {
 public:
  HotPlugEventCallback() {
  }

  void Callback(std::vector<hwcomposer::NativeDisplay *> connected_displays) {
    spin_lock_.lock();
    connected_displays_.swap(connected_displays);
    if (connected_displays_.empty()) {
      spin_lock_.unlock();
      return;
    }

    hwcomposer::NativeDisplay *primary = connected_displays_.at(0);
    for (size_t i = 1; i < connected_displays_.size(); i++) {
      hwcomposer::NativeDisplay *cloned = connected_displays_.at(i);
      cloned->CloneDisplay(primary);
    }

    spin_lock_.unlock();
  }

  const std::vector<hwcomposer::NativeDisplay *> &GetConnectedDisplays() {
    spin_lock_.lock();
    PopulateConnectedDisplays();
    spin_lock_.unlock();
    return connected_displays_;
  }

  void PresentLayers(std::vector<hwcomposer::HwcLayer *> &layers,
                     std::vector<std::vector<uint32_t>> &layers_fences,
                     std::vector<int32_t> &fences) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    // We only support cloned mode for now.
    hwcomposer::NativeDisplay *primary = connected_displays_.at(0);
    int32_t retire_fence = -1;
    primary->Present(layers, &retire_fence);
    fences.emplace_back(retire_fence);
    // store fences for each display for each layer
    unsigned int fence_index = 0;
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

  void SetCanvasColor(uint64_t color, uint16_t bpc) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    /**
     * We are assuming that the color provided the user is in hex and in
     * ABGR format with R in LSB. For example, 0x000000ff would be Red.
     */
  }

  void SetActiveConfig(uint32_t config) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->SetActiveConfig(config);
  }

  void GetDisplayAttribute(uint32_t config,
                           hwcomposer::HWCDisplayAttribute attribute,
                           int32_t *value) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;
    int32_t tempValue;
    for (auto &display : connected_displays_)
      display->GetDisplayAttribute(config, attribute, &tempValue);
    *value = tempValue;
  }

  void GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    uint32_t numConfigs, configIndex;

    if (connected_displays_.empty())
      return;
    for (auto &display : connected_displays_)
      display->GetDisplayConfigs(&numConfigs, NULL);
    *num_configs = numConfigs;
  }

  void PopulateConnectedDisplays() {
    if (connected_displays_.empty()) {
      device_.GetConnectedPhysicalDisplays(connected_displays_);

      for (auto &display : connected_displays_) {
        auto callback = std::make_shared<DisplayVSyncCallback>();
        display->RegisterVsyncCallback(callback, 0);
        display->VSyncControl(true);
      }
    }
  }

 private:
  std::vector<hwcomposer::NativeDisplay *> connected_displays_;
  hwcomposer::GpuDevice &device_ = hwcomposer::GpuDevice::getInstance();
  hwcomposer::SpinLock spin_lock_;
};

static int create_dmabuf_buffer(struct buffer *buffer, int width, int height,
                                int format) {
  uint64_t modifier = 0;
  uint32_t flags = 0;
  struct drm_device *drm_dev;

  if (!drm_connect(buffer)) {
    fprintf(stderr, "drm_connect failed\n");
    goto error;
  }

  drm_dev = buffer->dev;

  buffer->width = width;
  switch (format) {
    case DRM_FORMAT_NV12:
      /* adjust height for allocation of NV12 Y and UV planes */
      buffer->height = height * 3 / 2;
      buffer->bpp = 8;
      break;
    case DRM_FORMAT_YUV420:
      buffer->height = height * 2;
      buffer->bpp = 8;
      break;
    case DRM_FORMAT_P010:
      buffer->height = height * 3 / 2;
      buffer->bpp = 16;
      break;
    default:
      buffer->height = height;
      buffer->bpp = 32;
  }
  buffer->format = format;

  if (!drm_dev->alloc_bo(buffer)) {
    fprintf(stderr, "alloc_bo failed\n");
    goto error1;
  }

  if (drm_dev->export_bo_to_prime(buffer) != 0) {
    fprintf(stderr, "gem_export_to_prime failed\n");
    goto error2;
  }
  if (buffer->dmabuf_fd < 0) {
    fprintf(stderr, "error: dmabuf_fd < 0\n");
    goto error2;
  }

  buffer->height = height;

  return 0;

error2:
  drm_dev->free_bo(buffer);
error1:
  drm_shutdown(buffer);
error:
  return -1;
}

static struct image *image_create(const char *filename) {
  uint32_t i, width, height, format;
  int ret;
  struct buffer *buffer;
  struct image *image;

  image = new struct image;
  memset(image, 0, sizeof(image));

  if (!image_open(image, filename))
    goto err;

  width = 1920;
  height = 1080;
  format = DRM_FORMAT_P010;

  for (i = 0; i < NUM_BUFFERS; i++) {
    buffer = &image->buffers[i];
    ret = create_dmabuf_buffer(buffer, width, height, format);

    if (ret < 0)
      goto err;
  }

  return image;

err:
  free(image);
  return NULL;
}

static void image_destroy(struct image *image) {
  image_close(image);
  free(image);
}

void copy_buffer_to_handle(struct gbm_handle *handle, buffer *buffer) {
  memset(&handle->import_data, 0, sizeof(handle->import_data));
  handle->import_data.fd_modifier_data.width = buffer->width;
  handle->import_data.fd_modifier_data.height = buffer->height;
  handle->import_data.fd_modifier_data.format = buffer->format;
  handle->import_data.fd_modifier_data.num_fds = 2;
  handle->import_data.fd_modifier_data.fds[0] = buffer->dmabuf_fd;
  handle->import_data.fd_modifier_data.strides[0] = buffer->stride;
  handle->import_data.fd_modifier_data.offsets[0] = 0;
  handle->import_data.fd_modifier_data.fds[1] = buffer->dmabuf_fd;
  handle->import_data.fd_modifier_data.strides[1] = buffer->stride;
  handle->import_data.fd_modifier_data.offsets[1] =
      buffer->stride * buffer->height;

  handle->meta_data_.num_planes_ = drm_bo_get_num_planes(buffer->format);
  handle->bo = nullptr;
  handle->hwc_buffer_ = true;
  handle->gbm_flags = 0;
}

int main(int argc, char *argv[]) {
  int ret, fd, primary_width, primary_height;
  struct image *image;
  std::vector<hwcomposer::HwcLayer *> layers;
  std::vector<std::vector<uint32_t>> layers_fences;
  std::vector<int32_t> fences;

  hwcomposer::HwcLayer layer;
  struct gbm_handle native_handle;
  buffer *buffer = NULL;

  image = image_create(argv[1]);
  if (!image) {
    fprintf(stderr, "Failed to initialize!");
    exit(EXIT_FAILURE);
  }

  hwcomposer::GpuDevice &device = hwcomposer::GpuDevice::getInstance();
  device.Initialize();
  auto callback = std::make_shared<HotPlugEventCallback>();
  device.RegisterHotPlugEventCallback(callback);
  const std::vector<hwcomposer::NativeDisplay *> &displays =
      device.GetAllDisplays();
  if (displays.empty())
    return 0;

  hwcomposer::NativeDisplay *primary = displays.at(0);

  primary->SetActiveConfig(0);
  primary->SetPowerMode(hwcomposer::kOn);
  primary_width = primary->Width();
  primary_height = primary->Height();

  layer.SetSourceCrop(hwcomposer::HwcRect<float>(0, 0, 1920, 1080));

  // layer.SetDisplayFrame(hwcomposer::HwcRect<int>(0, 0,
  // 					       primary_width,
  // 					       primary_height), 0, 0);

  layer.SetDisplayFrame(hwcomposer::HwcRect<int>(0, 0, 1920, 1080), 0, 0);
  // redraw loop
  // Draw here
  buffer = image_next_buffer(image);
  if (!buffer) {
    fprintf(stderr, "no free buffer\n");
  }

  if (!buffer->dev->map_bo(buffer)) {
    fprintf(stderr, "map_bo failed\n");
    return 1;
  }

  fill_buffer(buffer, image);

  buffer->dev->unmap_bo(buffer);

  copy_buffer_to_handle(&native_handle, buffer);

  layer.SetColorSpace(CS_BT2020);
  layer.SetHdrMetadata(6550, 2300, 8500, 39850, 35400, 14600, 15635, 16450,
                       1000, 100, 4000, 100);
  layer.SetHdrEotf(EOTF_ST2084);

  layer.SetAcquireFence(-1);
  std::vector<hwcomposer::HwcRect<int>> damage_region;
  damage_region.emplace_back(layer.GetDisplayFrame());
  layer.SetSurfaceDamage(damage_region);
  layer.SetNativeHandle(&native_handle);
  layers.emplace_back(&layer);

  callback->PresentLayers(layers, layers_fences, fences);

  while (1) {
    sleep(1);
  }

  image_destroy(image);

  return 0;
}
