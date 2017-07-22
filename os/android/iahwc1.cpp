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

#include <map>
#include <utility>

#include <cutils/log.h>
#include <cutils/properties.h>

#include <gpudevice.h>
#include <hwclayer.h>
#include <platformdefines.h>
#include <hwcdefs.h>
#include <nativedisplay.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include "utils_android.h"

namespace android {

struct IAHwc1Layer {
    ~IAHwc1Layer() {
       delete hwc_layer_;
        hwc_layer_ = NULL;
                ALOGE("KAL: Layer destroyed %p \n", native_handle_->handle_);
        delete native_handle_;
                native_handle_ = NULL;
    }

  struct gralloc_handle* native_handle_;
  hwcomposer::HwcLayer* hwc_layer_ = NULL;

  int InitFromHwcLayer(hwc_layer_1_t *sf_layer);
};

typedef struct HwcDisplay {
  struct hwc_context_t *ctx;
  hwcomposer::NativeDisplay *display_ = NULL;
  uint32_t display_id_ = 0;
  int32_t fence_ = -1;
  int last_render_layers_size = -1;
  std::vector<IAHwc1Layer> layers_;

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
    procs_->vsync(procs_, display > 0 ? HWC_DISPLAY_EXTERNAL_BIT : HWC_DISPLAY_PRIMARY_BIT, timestamp);
  }

 private:
  hwc_procs_t const *procs_;
};

class IAHotPlugEventCallback : public hwcomposer::HotPlugCallback {
public:
 IAHotPlugEventCallback(hwc_procs_t const *procs) : procs_(procs) {
 }

 void Callback(uint32_t display, bool connected) {
   procs_->hotplug(
       procs_, display > 0 ? HWC_DISPLAY_EXTERNAL_BIT : HWC_DISPLAY_PRIMARY_BIT,
       connected);
 }

private:
 hwc_procs_t const *procs_;
};

int IAHwc1Layer::InitFromHwcLayer(hwc_layer_1_t *sf_layer) {
  if (!hwc_layer_) {
      hwc_layer_ = new hwcomposer::HwcLayer();
      native_handle_ = new struct gralloc_handle();
  }
  native_handle_->handle_ = sf_layer->handle;
  ETRACE("KAL: InitFromHwcLayer %p %p \n",  native_handle_->handle_, sf_layer->handle);
  hwc_layer_->SetNativeHandle(native_handle_);
  hwc_layer_->SetAlpha(sf_layer->planeAlpha);
  hwc_layer_->SetSourceCrop(hwcomposer::HwcRect<float>(
      sf_layer->sourceCropf.left, sf_layer->sourceCropf.top,
      sf_layer->sourceCropf.right, sf_layer->sourceCropf.bottom));
  hwc_layer_->SetDisplayFrame(hwcomposer::HwcRect<int>(
      sf_layer->displayFrame.left, sf_layer->displayFrame.top,
      sf_layer->displayFrame.right, sf_layer->displayFrame.bottom));

  uint32_t transform = 0;
  // 270* and 180* cannot be combined with flips. More specifically, they
  // already contain both horizontal and vertical flips, so those fields are
  // redundant in this case. 90* rotation can be combined with either horizontal
  // flip or vertical flip, so treat it differently
  if (sf_layer->transform == HWC_TRANSFORM_ROT_270) {
    transform = hwcomposer::HWCTransform::kRotate270;
  } else if (sf_layer->transform == HWC_TRANSFORM_ROT_180) {
    transform = hwcomposer::HWCTransform::kRotate180;
  } else {
    if (sf_layer->transform & HWC_TRANSFORM_FLIP_H)
      transform |= hwcomposer::HWCTransform::kReflectX;
    if (sf_layer->transform & HWC_TRANSFORM_FLIP_V)
      transform |= hwcomposer::HWCTransform::kReflectY;
    if (sf_layer->transform & HWC_TRANSFORM_ROT_90)
      transform |= hwcomposer::HWCTransform::kRotate90;
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
  ETRACE("KAL: InitFromHwcLayer Done %p %p \n",  native_handle_->handle_, sf_layer->handle);
  return 0;
}

static void hwc_dump(struct hwc_composer_device_1 * /*dev*/, char * /*buff*/,
                     int /*buff_len*/) {
}

static bool hwc_skip_layer(const std::pair<int, int> &indices, int i) {
  return indices.first >= 0 && i >= indices.first && i <= indices.second;
}

static HwcDisplay *GetDisplay(struct hwc_context_t *ctx, int display) {
    if (display == 0) {
      return &ctx->primary_display_;
    }

  if (display == HWC_DISPLAY_VIRTUAL) {
    return &ctx->virtual_display_;
  }

  return &ctx->extended_displays_.at(display - 1);
}

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t num_displays,
                       hwc_display_contents_1_t **display_contents) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  int total_displays = (int)num_displays;
  for (int i = 0; i < total_displays; ++i) {
    if (!display_contents[i])
      continue;
    bool use_framebuffer_target = false;
#ifdef USE_DISABLE_OVERLAY_USAGE
    use_framebuffer_target = true;
#endif
    if (i == HWC_DISPLAY_VIRTUAL) {
      use_framebuffer_target = true;
    }

    std::pair<int, int> skip_layer_indices(-1, -1);
    int num_layers = display_contents[i]->numHwLayers;
    for (int j = 0; !use_framebuffer_target && j < num_layers; ++j) {
      hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

      if (!(layer->flags & HWC_SKIP_LAYER))
        continue;

      if (skip_layer_indices.first == -1)
        skip_layer_indices.first = j;
      skip_layer_indices.second = j;
    }

    for (int j = 0; j < num_layers; ++j) {
      hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

      if (!use_framebuffer_target && !hwc_skip_layer(skip_layer_indices, j)) {
        HwcDisplay *native_display = GetDisplay(ctx, i);
        hwcomposer::NativeDisplay *display = native_display->display_;

        // If the layer is off the screen, don't earmark it for an overlay.
        // We'll leave it as-is, which effectively just drops it from the frame
        const hwc_rect_t *frame = &layer->displayFrame;
        if ((frame->right - frame->left) <= 0 ||
            (frame->bottom - frame->top) <= 0 || frame->right <= 0 ||
            frame->bottom <= 0 || frame->left >= (int)display->Width() ||
            frame->top >= (int)display->Height())
          continue;

        if (layer->compositionType == HWC_FRAMEBUFFER)
          layer->compositionType = HWC_OVERLAY;
      } else {
        switch (layer->compositionType) {
          case HWC_OVERLAY:
          case HWC_BACKGROUND:
          case HWC_SIDEBAND:
          case HWC_CURSOR_OVERLAY:
            layer->compositionType = HWC_FRAMEBUFFER;
            break;
        }
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
    if (!sf_display_contents[i] || i == HWC_DISPLAY_VIRTUAL)
      continue;

    size_t num_dc_layers = dc->numHwLayers;
    HwcDisplay *native_display = GetDisplay(ctx, i);
    hwcomposer::NativeDisplay *display = native_display->display_;
    std::vector<IAHwc1Layer> &old_layers = native_display->layers_;
    std::vector<IAHwc1Layer> new_layers;
    size_t size = old_layers.size();
    std::vector<hwcomposer::HwcLayer *> source_layers;
    for (size_t j = 0; j < num_dc_layers; ++j) {
      hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
      if (!sf_layer || !sf_layer->handle || (sf_layer->flags & HWC_SKIP_LAYER))
        continue;

      ALOGE("sf_layer->handle2_ %p \n", sf_layer->handle);

      new_layers.emplace_back();
      IAHwc1Layer *layer =  &(new_layers.back());
      if (size > j) {
          IAHwc1Layer &old_layer = old_layers.at(j);
          layer->hwc_layer_ = old_layer.hwc_layer_;
          old_layer.hwc_layer_ = NULL;
      }

      layer->InitFromHwcLayer(sf_layer);
      layer->hwc_layer_->SetReleaseFence(-1);
      source_layers.emplace_back(layer->hwc_layer_);
      sf_layer->acquireFenceFd = -1;
    }

    if (source_layers.empty())
      return 0;

    bool success = display->Present(source_layers, &dc->retireFenceFd);
    if (!success) {
      ALOGE("Failed to set layers in the composition");
      return -1;
    }

    std::vector<hwcomposer::HwcLayer *>().swap(source_layers);
    old_layers.swap(new_layers);
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
      *value = 0; /* TODO: We should do this */
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

  auto hot_plug_callback = std::make_shared<IAHotPlugEventCallback>(procs);
  display->RegisterHotPlugCallback(std::move(hot_plug_callback), 0);

  std::vector<HwcDisplay> &extended = ctx->extended_displays_;
  size_t size = extended.size();
  for (size_t i = 0; i < size; i++) {
    auto extended_callback = std::make_shared<IAVsyncCallback>(procs);
    extended.at(i)
        .display_->RegisterVsyncCallback(std::move(extended_callback), 1);

    auto extended_hot_plug_callback =
        std::make_shared<IAHotPlugEventCallback>(procs);
    display->RegisterHotPlugCallback(std::move(extended_hot_plug_callback), 1);
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

  std::vector<hwcomposer::NativeDisplay *> displays =
      ctx->device_.GetAllDisplays();
  ctx->virtual_display_.display_ = ctx->device_.GetVirtualDisplay();
  ctx->virtual_display_.display_->SetExplicitSyncSupport(
      ctx->disable_explicit_sync_);

  size_t size = displays.size();
  hwcomposer::NativeDisplay *primary_display = displays.at(0);
  ctx->primary_display_.display_ = primary_display;
  ctx->primary_display_.display_id_ = 0;
  ctx->primary_display_.display_->SetExplicitSyncSupport(
      ctx->disable_explicit_sync_);
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
    temp.display_->SetExplicitSyncSupport(ctx->disable_explicit_sync_);
  }

  ctx->device.common.tag = HARDWARE_DEVICE_TAG;
  ctx->device.common.version = HWC_DEVICE_API_VERSION_1_4;
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
  ctx->device.setCursorPositionAsync = NULL;

  *dev = &ctx->device.common;
  ctx.release();

  return 0;
}
}

static struct hw_module_methods_t hwc_module_methods = {
    .open = android::hwc_device_open};

hwc_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "IA-Hardware-Composer",
        .author = "The Android Open Source Project",
        .methods = &hwc_module_methods,
        .dso = NULL,
        .reserved = {0},
    }};
