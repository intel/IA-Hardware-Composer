/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <inttypes.h>

#include <android/log.h>
#include <cutils/properties.h>
#include <sw_sync.h>
#include <sync/sync.h>

#include <gpudevice.h>
#include <hwcdefs.h>
#include <hwclayer.h>
#include <nativedisplay.h>
#include <platformdefines.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include "utils_android.h"

namespace android {

class DisplayTimeLine {
 public:
  int Init() {
    timeline_fd_ = open("/dev/sw_sync", O_RDWR);
    if (timeline_fd_ < 0)
      return -1;

    return 0;
  }

  ~DisplayTimeLine() {
    if (timeline_fd_ > 0) {
      close(timeline_fd_);
    }
  }

  int32_t IncrementTimeLine() {
    int ret =
        sw_sync_fence_create(timeline_fd_, "display fence", timeline_pt_ + 1);
    if (ret < 0) {
      ALOGE("Failed to create display fence %d %d", ret, timeline_fd_);
      return ret;
    }

    int32_t ret_fd(ret);

    ret = sw_sync_timeline_inc(timeline_fd_, 1);
    if (ret) {
      ALOGE("Failed to increment display sync timeline %d", ret);
      return ret;
    }

    ++timeline_pt_;
    return ret_fd;
  }

 private:
  int32_t timeline_fd_;
  int timeline_pt_ = 0;
};

struct IAHwc1Layer {
  ~IAHwc1Layer() {
    delete hwc_layer_;
    hwc_layer_ = NULL;
  }

  IAHwc1Layer() = default;

  IAHwc1Layer(const IAHwc1Layer &rhs) = delete;
  IAHwc1Layer &operator=(const IAHwc1Layer &rhs) = delete;

  struct gralloc_handle native_handle_;
  hwcomposer::HwcLayer *hwc_layer_ = NULL;
  uint32_t index_ = 0;

  int InitFromHwcLayer(hwc_layer_1_t *sf_layer);
};

typedef struct HwcDisplay {
  struct hwc_context_t *ctx;
  hwcomposer::NativeDisplay *display_ = NULL;
  uint32_t display_id_ = 0;
  int32_t fence_ = -1;
  int last_render_layers_size = -1;
  std::vector<IAHwc1Layer *> layers_;
  DisplayTimeLine timeline_;
  bool gl_composition_ = false;
} hwc_drm_display_t;

struct hwc_context_t {
  ~hwc_context_t() {
  }

  hwc_composer_device_1_t device;
  hwc_procs_t const *procs = NULL;

  hwcomposer::GpuDevice device_;
  std::vector<HwcDisplay> extended_displays_;
  HwcDisplay primary_display_;
  HwcDisplay virtual_display_;

  bool disable_explicit_sync_ = false;
};

class IAVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  IAVsyncCallback(hwc_procs_t const *procs) : procs_(procs) {
  }

  void Callback(uint32_t display, int64_t timestamp) {
    procs_->vsync(procs_, display > 0 ? HWC_DISPLAY_EXTERNAL_BIT
                                      : HWC_DISPLAY_PRIMARY_BIT,
                  timestamp);
  }

 private:
  hwc_procs_t const *procs_;
};

class IAHotPlugEventCallback : public hwcomposer::HotPlugCallback {
 public:
  IAHotPlugEventCallback(hwc_procs_t const *procs) : procs_(procs) {
  }

  void Callback(uint32_t /*display*/, bool connected) {
    if (ignore_) {
      ignore_ = false;
      return;
    }

    procs_->hotplug(procs_, HWC_DISPLAY_EXTERNAL_BIT, connected);
  }

 private:
  hwc_procs_t const *procs_;
  bool ignore_ = true;
};

class IARefreshCallback : public hwcomposer::RefreshCallback {
 public:
  IARefreshCallback(hwc_procs_t const *procs) : procs_(procs) {
  }

  void Callback(uint32_t /*display*/) {
    procs_->invalidate(procs_);
  }

 private:
  hwc_procs_t const *procs_;
};

int IAHwc1Layer::InitFromHwcLayer(hwc_layer_1_t *sf_layer) {
  if (!hwc_layer_) {
    hwc_layer_ = new hwcomposer::HwcLayer();
  }

  bool surface_damage = true;

  if (hwc_layer_->GetNativeHandle() &&
      (hwc_layer_->GetNativeHandle()->handle_ == sf_layer->handle))
    surface_damage = false;

  native_handle_.handle_ = sf_layer->handle;
  hwc_layer_->SetNativeHandle(&native_handle_);
  hwc_layer_->SetAlpha(sf_layer->planeAlpha);
  hwc_layer_->SetSourceCrop(hwcomposer::HwcRect<float>(
      sf_layer->sourceCropf.left, sf_layer->sourceCropf.top,
      sf_layer->sourceCropf.right, sf_layer->sourceCropf.bottom));
  hwc_layer_->SetDisplayFrame(hwcomposer::HwcRect<int>(
      sf_layer->displayFrame.left, sf_layer->displayFrame.top,
      sf_layer->displayFrame.right, sf_layer->displayFrame.bottom));

  uint32_t transform = 0;
  if (sf_layer->transform == HWC_TRANSFORM_ROT_270) {
    transform = hwcomposer::HWCTransform::kTransform270;
  } else if (sf_layer->transform == HWC_TRANSFORM_ROT_180) {
    transform = hwcomposer::HWCTransform::kTransform180;
  } else {
    if (sf_layer->transform & HWC_TRANSFORM_FLIP_H)
      transform |= hwcomposer::HWCTransform::kReflectX;
    if (sf_layer->transform & HWC_TRANSFORM_FLIP_V)
      transform |= hwcomposer::HWCTransform::kReflectY;
    if (sf_layer->transform & HWC_TRANSFORM_ROT_90)
      transform |= hwcomposer::HWCTransform::kTransform90;
  }

  hwc_layer_->SetTransform(transform);
  hwc_layer_->SetAcquireFence(dup(sf_layer->acquireFenceFd));

  switch (sf_layer->blending) {
    case HWC_BLENDING_NONE:
      hwc_layer_->SetBlending(hwcomposer::HWCBlending::kBlendingNone);
      break;
    case HWC_BLENDING_PREMULT:
      hwc_layer_->SetBlending(hwcomposer::HWCBlending::kBlendingPremult);
      break;
    case HWC_BLENDING_COVERAGE:
      hwc_layer_->SetBlending(hwcomposer::HWCBlending::kBlendingCoverage);
      break;
    default:
      ALOGE("Invalid blending in hwc_layer_1_t %d", sf_layer->blending);
      return -EINVAL;
  }

  if (surface_damage) {
    uint32_t num_rects = sf_layer->surfaceDamage.numRects;
    hwcomposer::HwcRegion hwc_region;

    for (size_t rect = 0; rect < num_rects; ++rect) {
      hwc_region.emplace_back(sf_layer->surfaceDamage.rects[rect].left,
                              sf_layer->surfaceDamage.rects[rect].top,
                              sf_layer->surfaceDamage.rects[rect].right,
                              sf_layer->surfaceDamage.rects[rect].bottom);
    }

    hwc_layer_->SetSurfaceDamage(hwc_region);
  } else {
    hwcomposer::HwcRegion hwc_region;
    hwc_region.emplace_back(0, 0, 0, 0);
    hwc_layer_->SetSurfaceDamage(hwc_region);
  }

  uint32_t num_rects = sf_layer->visibleRegionScreen.numRects;
  hwcomposer::HwcRegion visible_region;

  for (size_t rect = 0; rect < num_rects; ++rect) {
    visible_region.emplace_back(
        sf_layer->visibleRegionScreen.rects[rect].left,
        sf_layer->visibleRegionScreen.rects[rect].top,
        sf_layer->visibleRegionScreen.rects[rect].right,
        sf_layer->visibleRegionScreen.rects[rect].bottom);
  }

  hwc_layer_->SetVisibleRegion(visible_region);
  return 0;
}

static void hwc_dump(struct hwc_composer_device_1 * /*dev*/, char * /*buff*/,
                     int /*buff_len*/) {
}

static HwcDisplay *GetDisplay(struct hwc_context_t *ctx, int display) {
  if (display == 0) {
    return &ctx->primary_display_;
  }

  if (display == HWC_DISPLAY_VIRTUAL) {
    return &ctx->virtual_display_;
  }

  return &ctx->extended_displays_.at(0);
}

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t num_displays,
                       hwc_display_contents_1_t **display_contents) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  int total_displays = (int)num_displays;
  bool disable_overlays = ctx->disable_explicit_sync_;

  for (int i = 0; i < total_displays; ++i) {
    if (!display_contents[i])
      continue;

    if (i == HWC_DISPLAY_VIRTUAL) {
      disable_overlays = true;
    } else {
      disable_overlays = ctx->disable_explicit_sync_;
    }

    int num_layers = display_contents[i]->numHwLayers;
    HwcDisplay *native_display = GetDisplay(ctx, i);
    native_display->gl_composition_ = disable_overlays;

    for (int j = 0; j < num_layers; ++j) {
      hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

      if (!disable_overlays) {
        switch (layer->compositionType) {
          case HWC_BACKGROUND:
          case HWC_SIDEBAND:
            layer->compositionType = HWC_FRAMEBUFFER;
            native_display->gl_composition_ = true;
            break;
          case HWC_FRAMEBUFFER_TARGET:
            break;
          default:
            layer->compositionType = HWC_OVERLAY;
            break;
        }
      } else {
        layer->compositionType = HWC_FRAMEBUFFER;
      }
    }
  }

  return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays,
                   hwc_display_contents_1_t **sf_display_contents) {
  ATRACE_CALL();
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  for (size_t i = 0; i < num_displays; ++i) {
    hwc_display_contents_1_t *dc = sf_display_contents[i];
    if (!dc || i == HWC_DISPLAY_VIRTUAL)
      continue;

    size_t num_dc_layers = dc->numHwLayers;
    HwcDisplay *native_display = GetDisplay(ctx, i);
    dc->retireFenceFd = native_display->timeline_.IncrementTimeLine();
    hwcomposer::NativeDisplay *display = native_display->display_;
    std::vector<IAHwc1Layer *> &old_layers = native_display->layers_;
    std::vector<IAHwc1Layer *> new_layers;
    size_t size = old_layers.size();
    std::vector<hwcomposer::HwcLayer *> source_layers;
    for (size_t j = 0; j < num_dc_layers; ++j) {
      hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
      if (!sf_layer || !sf_layer->handle || (sf_layer->flags & HWC_SKIP_LAYER))
        continue;

      if (!native_display->gl_composition_ &&
          (sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET)) {
        continue;
      }

      IAHwc1Layer *new_layer = new IAHwc1Layer();
      if (size > j) {
        IAHwc1Layer *old_layer = old_layers.at(j);
        new_layer->hwc_layer_ = old_layer->hwc_layer_;
        old_layer->hwc_layer_ = NULL;
      }

      new_layer->InitFromHwcLayer(sf_layer);
      source_layers.emplace_back(new_layer->hwc_layer_);
      new_layer->index_ = j;
      new_layers.emplace_back(new_layer);
      sf_layer->acquireFenceFd = -1;
      sf_layer->releaseFenceFd = -1;
    }

    if (source_layers.empty()) {
      return 0;
    }

    int32_t retire_fence = -1;
    old_layers.swap(new_layers);
    size = new_layers.size();
    for (size_t i = 0; i < size; i++) {
      IAHwc1Layer *layer = new_layers.at(i);
      delete layer;
    }

    std::vector<IAHwc1Layer *>().swap(new_layers);

    bool success = display->Present(source_layers, &retire_fence);
    if (!success) {
      ALOGE("Failed to set layers in the composition");
      return -1;
    }

    if (retire_fence > 0)
      close(retire_fence);

    size = old_layers.size();
    for (size_t i = 0; i < size; i++) {
      hwcomposer::HwcLayer *layer = old_layers.at(i)->hwc_layer_;
      int32_t release_fence = layer->GetReleaseFence();
      if (release_fence <= 0)
        continue;

      hwc_layer_1_t *sf_layer = &dc->hwLayers[old_layers.at(i)->index_];
      sf_layer->releaseFenceFd = release_fence;
    }

    std::vector<hwcomposer::HwcLayer *>().swap(source_layers);
  }

  return 0;
}

static int hwc_event_control(struct hwc_composer_device_1 *dev, int display,
                             int event, int enabled) {
  if (event != HWC_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  HwcDisplay *native_display = GetDisplay(ctx, display);
  hwcomposer::NativeDisplay *temp = native_display->display_;
  temp->VSyncControl(enabled);
  return 0;
}

static int hwc_set_power_mode(struct hwc_composer_device_1 *dev, int display,
                              int mode) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  uint32_t power_mode = hwcomposer::kOn;
  switch (mode) {
    case HWC_POWER_MODE_OFF:
      power_mode = hwcomposer::kOff;
      break;
    case HWC_POWER_MODE_DOZE:
      power_mode = hwcomposer::kDoze;
      break;
    case HWC_POWER_MODE_DOZE_SUSPEND:
      power_mode = hwcomposer::kDozeSuspend;
      break;
    case HWC_POWER_MODE_NORMAL:
      power_mode = hwcomposer::kOn;
      break;
    default:
      ALOGI("Power mode %d is unsupported\n", mode);
      return -1;
  };

  HwcDisplay *native_display = GetDisplay(ctx, display);
  hwcomposer::NativeDisplay *temp = native_display->display_;
  temp->SetPowerMode(power_mode);
  return 0;
}

static int hwc_query(struct hwc_composer_device_1 * /* dev */, int what,
                     int *value) {
  switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
      *value = 0;
      break;
    case HWC_VSYNC_PERIOD:
      *value = 1000 * 1000 * 1000 / 60;
      break;
    case HWC_DISPLAY_TYPES_SUPPORTED:
      *value = HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_EXTERNAL_BIT |
               HWC_DISPLAY_VIRTUAL_BIT;
      break;
  }
  return 0;
}

static void hwc_register_procs(struct hwc_composer_device_1 *dev,
                               hwc_procs_t const *procs) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  hwcomposer::NativeDisplay *display = ctx->primary_display_.display_;

  ctx->procs = procs;

  auto callback = std::make_shared<IAVsyncCallback>(procs);
  display->RegisterVsyncCallback(std::move(callback), 0);

  auto refresh_callback = std::make_shared<IARefreshCallback>(procs);
  display->RegisterRefreshCallback(std::move(refresh_callback),
                                   static_cast<int>(0));

  std::vector<HwcDisplay> &extended = ctx->extended_displays_;
  size_t size = extended.size();
  for (size_t i = 0; i < size; i++) {
    auto extended_callback = std::make_shared<IAVsyncCallback>(procs);
    extended.at(i)
        .display_->RegisterVsyncCallback(std::move(extended_callback), 1);

    /* XXX/TODO Add hot plug registration for external displays */
    auto extended_refresh_callback = std::make_shared<IARefreshCallback>(procs);
    extended.at(i).display_->RegisterRefreshCallback(
        std::move(extended_refresh_callback), static_cast<int>(1));
  }
}

static int hwc_get_display_configs(struct hwc_composer_device_1 *dev,
                                   int display, uint32_t *configs,
                                   size_t *num_configs) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  uint32_t size = 0;
  HwcDisplay *native_display = GetDisplay(ctx, display);
  hwcomposer::NativeDisplay *temp = native_display->display_;

  if (!temp->GetDisplayConfigs(&size, configs))
    return -1;

  *num_configs = size;

  return *num_configs == 0 ? -1 : 0;
}

static int hwc_get_display_attributes(struct hwc_composer_device_1 *dev,
                                      int display, uint32_t config,
                                      const uint32_t *attributes,
                                      int32_t *values) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  HwcDisplay *native_display = GetDisplay(ctx, display);
  hwcomposer::NativeDisplay *temp = native_display->display_;
  for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; ++i) {
    switch (attributes[i]) {
      case HWC_DISPLAY_WIDTH:
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kWidth, &values[i]);
        break;
      case HWC_DISPLAY_HEIGHT:
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kHeight, &values[i]);
        break;
      case HWC_DISPLAY_VSYNC_PERIOD:
        // in nanoseconds
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kRefreshRate, &values[i]);
        break;
      case HWC_DISPLAY_DPI_X:
        // Dots per 1000 inches
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kDpiX, &values[i]);
        break;
      case HWC_DISPLAY_DPI_Y:
        // Dots per 1000 inches
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kDpiY, &values[i]);
        break;
      default:
        values[i] = -1;
        return -1;
    }
  }

  return 0;
}

static int hwc_get_active_config(struct hwc_composer_device_1 *dev,
                                 int display) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  uint32_t config;
  HwcDisplay *native_display = GetDisplay(ctx, display);
  hwcomposer::NativeDisplay *temp = native_display->display_;

  if (!temp->GetActiveConfig(&config))
    return -1;

  return config;
}

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display,
                                 int index) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  HwcDisplay *native_display = GetDisplay(ctx, display);
  hwcomposer::NativeDisplay *temp = native_display->display_;

  temp->SetActiveConfig(index);
  return 0;
}

static int hwc_set_cursor_position_async(struct hwc_composer_device_1 * /*dev*/,
                                         int /*display*/, int /*x_pos*/,
                                         int /*y_pos*/) {
  return 0;
}

static int hwc_device_close(struct hw_device_t *dev) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
  delete ctx;
  return 0;
}

static int hwc_device_open(const struct hw_module_t *module, const char *name,
                           struct hw_device_t **dev) {
  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("Invalid module name- %s", name);
    return -EINVAL;
  }

  std::unique_ptr<hwc_context_t> ctx(new hwc_context_t());
  if (!ctx) {
    ALOGE("Failed to allocate hwc context");
    return -ENOMEM;
  }

  char value[PROPERTY_VALUE_MAX];
  property_get("board.disable.explicit.sync", value, "0");
  ctx->disable_explicit_sync_ = atoi(value);
  if (ctx->disable_explicit_sync_)
    ALOGI("EXPLICIT SYNC support is disabled");
  else
    ALOGI("EXPLICIT SYNC support is enabled");

  if (!ctx->device_.Initialize()) {
    ALOGE("Can't initialize drm object.");
    return -1;
  }

  const std::vector<hwcomposer::NativeDisplay *> &displays =
      ctx->device_.GetAllDisplays();
  ctx->virtual_display_.display_ = ctx->device_.GetVirtualDisplay();
  ctx->virtual_display_.display_->SetExplicitSyncSupport(
      ctx->disable_explicit_sync_);
  ctx->virtual_display_.timeline_.Init();

  size_t size = displays.size();
  hwcomposer::NativeDisplay *primary_display = displays.at(0);
  ctx->primary_display_.display_ = primary_display;
  ctx->primary_display_.display_id_ = 0;
  ctx->primary_display_.display_->SetExplicitSyncSupport(
      ctx->disable_explicit_sync_);
  ctx->primary_display_.timeline_.Init();
  // Fetch the number of modes from the display
  uint32_t num_configs;
  uint32_t default_config;
  if (!primary_display->GetDisplayConfigs(&num_configs, NULL))
    return -1;

  // Grab the first mode, we'll choose this as the active mode
  num_configs = 1;
  if (!primary_display->GetDisplayConfigs(&num_configs, &default_config))
    return -1;

  if (!primary_display->SetActiveConfig(default_config)) {
    ALOGE("Could not find active mode for %d", default_config);
    return -1;
  }

  for (size_t i = 1; i < size; ++i) {
    ctx->extended_displays_.emplace_back();
    HwcDisplay &temp = ctx->extended_displays_.back();
    temp.display_ = displays.at(i);
    temp.display_id_ = i;
    temp.timeline_.Init();
    temp.display_->SetExplicitSyncSupport(ctx->disable_explicit_sync_);
  }

  ctx->device.common.tag = HARDWARE_DEVICE_TAG;
  ctx->device.common.version = HWC_DEVICE_API_VERSION_1_5;
  ctx->device.common.module = const_cast<hw_module_t *>(module);
  ctx->device.common.close = hwc_device_close;

  ctx->device.dump = hwc_dump;
  ctx->device.prepare = hwc_prepare;
  ctx->device.set = hwc_set;
  ctx->device.eventControl = hwc_event_control;
  ctx->device.setPowerMode = hwc_set_power_mode;
  ctx->device.query = hwc_query;
  ctx->device.registerProcs = hwc_register_procs;
  ctx->device.getDisplayConfigs = hwc_get_display_configs;
  ctx->device.getDisplayAttributes = hwc_get_display_attributes;
  ctx->device.getActiveConfig = hwc_get_active_config;
  ctx->device.setActiveConfig = hwc_set_active_config;
  ctx->device.setCursorPositionAsync = hwc_set_cursor_position_async;

  *dev = &ctx->device.common;
  ctx.release();

  return 0;
}
}  // namespace android

static struct hw_module_methods_t hwc1_module_methods = {
    .open = android::hwc_device_open};

hwc_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "IA-Hardware-Composer",
        .author = "The Android Open Source Project",
        .methods = &hwc1_module_methods,
        .dso = NULL,
        .reserved = {0},
    }};
