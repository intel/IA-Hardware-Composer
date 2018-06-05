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

#include "hwf_alioshal.h"

#include <cutils/hwflinger.h>
#include <cutils/hwflinger_defs.h>

#include <log/Log.h>
#include <stdint.h>
#include <memory>

#include "hwctrace.h"

#define LOG_TAG "IAHWF"

namespace hwcomposer {

int HwfLayer::InitFromHwcLayer(hwf_layer_t *sf_layer) {
  if (!hwc_layer_) {
    hwc_layer_ = new hwcomposer::HwcLayer();
  }

  bool surface_damage = true;

  if (hwc_layer_->GetNativeHandle() &&
      (hwc_layer_->GetNativeHandle()->target_ == sf_layer->target))
    surface_damage = false;

  native_handle_.target_ = sf_layer->target;
  hwc_layer_->SetNativeHandle(&native_handle_);
  hwc_layer_->SetAlpha(sf_layer->globalAlpha);
  hwc_layer_->SetSourceCrop(hwcomposer::HwcRect<float>(
      sf_layer->srcRect.left, sf_layer->srcRect.top, sf_layer->srcRect.right,
      sf_layer->srcRect.bottom));

  hwc_layer_->SetDisplayFrame(
      hwcomposer::HwcRect<int>(sf_layer->destRect.left, sf_layer->destRect.top,
                               sf_layer->destRect.right,
                               sf_layer->destRect.bottom),
      0, 0);

  uint32_t transform = 0;
  if (sf_layer->transform == HWF_TRANSFORM_ROT_270) {
    transform = hwcomposer::HWCTransform::kTransform270;
  } else if (sf_layer->transform == HWF_TRANSFORM_ROT_180) {
    transform = hwcomposer::HWCTransform::kTransform180;
  } else {
    if (sf_layer->transform & HWF_TRANSFORM_FLIP_H)
      transform |= hwcomposer::HWCTransform::kReflectX;
    if (sf_layer->transform & HWF_TRANSFORM_FLIP_V)
      transform |= hwcomposer::HWCTransform::kReflectY;
    if (sf_layer->transform & HWF_TRANSFORM_ROT_90)
      transform |= hwcomposer::HWCTransform::kTransform90;
  }

  hwc_layer_->SetTransform(transform);
  hwc_layer_->SetAcquireFence(dup(sf_layer->acquireSyncFd));

  switch (sf_layer->blendMode) {
    case HWF_BLENDING_NONE:
      hwc_layer_->SetBlending(hwcomposer::HWCBlending::kBlendingNone);
      break;

    case HWF_BLENDING_PREMULT:
      hwc_layer_->SetBlending(hwcomposer::HWCBlending::kBlendingPremult);
      break;

    case HWF_BLENDING_COVERAGE:
      hwc_layer_->SetBlending(hwcomposer::HWCBlending::kBlendingCoverage);
      break;

    default:
      LOG_E("Invalid blendMode in hwc_layer_1_t %d", sf_layer->blendMode);
      return -EINVAL;
  }

  // TODO: Add the damage detection.
  if (surface_damage) {
    hwcomposer::HwcRegion hwc_region;
#if 0
	uint32_t num_rects = sf_layer->surfaceDamage.numRects;
	for (size_t rect = 0; rect < num_rects; ++rect) {
	    hwc_region.emplace_back(sf_layer->surfaceDamage.rects[rect].left,
				  sf_layer->surfaceDamage.rects[rect].top,
				  sf_layer->surfaceDamage.rects[rect].right,
				  sf_layer->surfaceDamage.rects[rect].bottom);
	    }
#endif
    hwc_layer_->SetSurfaceDamage(hwc_region);
  } else {
    hwcomposer::HwcRegion hwc_region;
    hwc_region.emplace_back(0, 0, 0, 0);
    hwc_layer_->SetSurfaceDamage(hwc_region);
  }

  uint32_t num_rects = sf_layer->visibleRegion.num;
  hwcomposer::HwcRegion visible_region;

  for (size_t rect = 0; rect < num_rects; ++rect) {
    visible_region.emplace_back(sf_layer->visibleRegion.rects[rect].left,
                                sf_layer->visibleRegion.rects[rect].top,
                                sf_layer->visibleRegion.rects[rect].right,
                                sf_layer->visibleRegion.rects[rect].bottom);
  }

  hwc_layer_->SetVisibleRegion(visible_region);

  return 0;
}

/************************/

HwfDisplay *HwfDevice::GetDisplay(int display) {
  if (display == HWF_DISPLAY_PRIMARY) {
    return &primary_display_;
  }

  if (display == HWF_DISPLAY_VIRTUAL) {
    return &virtual_display_;
  }

  return &extended_displays_.at(0);
}

int DBG_DumpHwfLayerInfo(struct hwf_device_t *device, int dispCount,
                         hwf_display_t **displays) {
  LOG_I("DBG_DumpHwfLayerInfo --> Enter.\n");

  int total_displays = (int)dispCount;

  for (int i = 0; i < total_displays; ++i) {
    if (!displays[i])
      continue;

    LOG_I("\tDisplay Number: %d.\n", i);

    int num_layers = displays[i]->numLayers;

    for (int j = 0; j < num_layers; ++j) {
      hwf_layer_t *layer = &displays[i]->hwfLayers[j];

      LOG_I("\t\tLayer Number: %d.\n", j);

      // Dump Layer info:
      switch (layer->composeMode) {
        case HWF_FB:
          LOG_I("\t\t\tLayer->composeMode: %s.\n", "HWF_FB");
          break;

        case HWF_FB_TARGET:
          LOG_I("\t\t\tLayer->composeMode: %s.\n", "HWF_FB_TARGET");
          break;

        case HWF_OVERLAY:
          LOG_I("\t\t\tLayer->composeMode: %s.\n", "HWF_OVERLAY");
          break;

        default:
          LOG_I("\t\t\tLayer->composeMode: %s.\n", "Not Set.");
          break;
      }
    }
  }

  LOG_I("DBG_DumpHwfLayerInfo --> Exit.\n");
  return 0;
}

int HwfDevice::detect(struct hwf_device_t *device, int dispCount,
                      hwf_display_t **displays) {
  CTRACE();
  LOG_I("HwfDevice::detect --> dispCount: %d\n", dispCount);

  HwfDevice *hwf_device = (HwfDevice *)device;

  int total_displays = (int)dispCount;
  bool disable_overlays = hwf_device->disable_explicit_sync_;  // TODO: review

  for (int i = 0; i < total_displays; ++i) {
    if (!displays[i])
      continue;

    if (i == HWF_DISPLAY_VIRTUAL) {
      disable_overlays = true;
    } else {
      disable_overlays = hwf_device->disable_explicit_sync_;
    }

    int num_layers = displays[i]->numLayers;
    HwfDisplay *native_display = hwf_device->GetDisplay(i);
    native_display->gl_composition_ = disable_overlays;

    for (int j = 0; j < num_layers; ++j) {
      hwf_layer_t *layer = &displays[i]->hwfLayers[j];

      if (!disable_overlays) {
        switch (layer->composeMode) {
          /*
        //case HWC_BACKGROUND:  // TODO:
        //case HWC_SIDEBAND:
          layer->composeMode = HWF_FB;
          native_display->gl_composition_ = true;
          break;
          */
          case HWF_FB_TARGET:
            break;
          default:
            layer->composeMode = HWF_OVERLAY;
            break;
        }
      } else {
        layer->composeMode = HWF_FB;
      }
    }
  }

  DBG_DumpHwfLayerInfo(device, dispCount, displays);

  return 0;
}

int HwfDevice::flip(struct hwf_device_t *device, int dispCount,
                    hwf_display_t **displays) {
  CTRACE();
  LOG_I("HwfDevice::flip --> enter.\n");
  LOG_I("HwfDevice::flip --> dispCount: %d\n", dispCount);

  HwfDevice *hwf_device = (HwfDevice *)device;

  for (int i = 0; i < dispCount; ++i) {
    LOG_I("\tflip --> display[%d] -- begin.\n", i);
    hwf_display_t *dc = displays[i];
    if (!dc || i == HWF_DISPLAY_VIRTUAL)
      continue;

    size_t num_dc_layers = dc->numLayers;
    HwfDisplay *native_display = hwf_device->GetDisplay(i);
    dc->retireSyncFd = native_display->timeline_.IncrementTimeLine();
    hwcomposer::NativeDisplay *display = native_display->display_;
    std::vector<HwfLayer *> &old_layers = native_display->layers_;
    std::vector<HwfLayer *> new_layers;
    size_t size = old_layers.size();
    std::vector<hwcomposer::HwcLayer *> source_layers;
    for (size_t j = 0; j < num_dc_layers; ++j) {
      hwf_layer_t *sf_layer = &dc->hwfLayers[j];
      if (!sf_layer || !sf_layer->target ||
          (sf_layer->flags & HWF_LAYER_IGNORED))
        continue;

      if (!native_display->gl_composition_ &&
          (sf_layer->composeMode == HWF_FB_TARGET)) {
        continue;
      }

      HwfLayer *new_layer = new HwfLayer();
      if (size > j) {
        HwfLayer *old_layer = old_layers.at(j);
        new_layer->hwc_layer_ = old_layer->hwc_layer_;
        old_layer->hwc_layer_ = NULL;
      }

      new_layer->InitFromHwcLayer(sf_layer);
      source_layers.emplace_back(new_layer->hwc_layer_);
      new_layer->index_ = j;
      new_layers.emplace_back(new_layer);
      sf_layer->acquireSyncFd = -1;
      sf_layer->releaseSyncFd = -1;
    }

    if (source_layers.empty()) {
      return 0;
    }

    int32_t retire_fence = -1;
    old_layers.swap(new_layers);
    size = new_layers.size();
    for (size_t i = 0; i < size; i++) {
      HwfLayer *layer = new_layers.at(i);
      delete layer;
    }

    std::vector<HwfLayer *>().swap(new_layers);

    LOG_I("\tWill to present.\n");
    bool success = display->Present(source_layers, &retire_fence);
    if (!success) {
      LOG_E("Failed to set layers in the composition");
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

      hwf_layer_t *sf_layer = &dc->hwfLayers[old_layers.at(i)->index_];
      sf_layer->releaseSyncFd = release_fence;
    }

    std::vector<hwcomposer::HwcLayer *>().swap(source_layers);

    LOG_I("\tflip --> display[%d] -- end.\n", i);
  }

  LOG_I("HwfDevice::flip --> exit.\n");

  return 0;
}

int HwfDevice::setEventState(struct hwf_device_t *device, int disp, int event,
                             int enabled) {
  CTRACE();
  LOG_I("HwfDevice::setEventState --> disp:%d, event: %d, enabled: %d.\n", disp,
        event, enabled);

  if (event != HWF_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  HwfDevice *hwf_device = (HwfDevice *)device;

  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  hwcomposer::NativeDisplay *temp = native_display->display_;
  temp->VSyncControl(enabled);

  return 0;
}

int HwfDevice::setDisplayState(struct hwf_device_t *device, int disp,
                               int state) {
  CTRACE();
  LOG_I("HwfDevice::setDisplayState --> disp:%d, state: %d.\n", disp, state);

  HwfDevice *hwf_device = (HwfDevice *)device;

  uint32_t power_mode = hwcomposer::kOn;
  switch (state) {
    case HWF_DISPLAY_STATE_OFF:
      power_mode = hwcomposer::kOff;
      break;
    case HWF_DISPLAY_STATE_IDLE:
      power_mode = hwcomposer::kDoze;
      break;
    case HWF_DISPLAY_STATE_IDLE_SUSPEND:
      power_mode = hwcomposer::kDozeSuspend;
      break;
    case HWF_DISPLAY_STATE_NORMAL:
      power_mode = hwcomposer::kOn;
      break;
    default:
      LOG_I("Power mode %d is unsupported\n", state);
      return -1;
  };

  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  hwcomposer::NativeDisplay *temp = native_display->display_;
  temp->SetPowerMode(power_mode);

  return 0;
}

int HwfDevice::lookup(struct hwf_device_t *device, int what, int *value) {
  LOG_I("HwfDevice::setDisplayState --> called.\n");

  return 0;
}

/* Callback function */
class IAVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  IAVsyncCallback(hwf_callback const *procs) : m_pCB(procs) {
  }

  void Callback(uint32_t display, int64_t timestamp) {
    m_pCB->vsyncEvent(m_pCB,
                      display > 0 ? HWF_DISPLAY_EXTERNAL : HWF_DISPLAY_PRIMARY,
                      timestamp);
  }

 private:
  hwf_callback const *m_pCB;
};

class IAHotPlugEventCallback : public hwcomposer::HotPlugCallback {
 public:
  IAHotPlugEventCallback(hwf_callback const *procs) : m_pCB(procs) {
  }

  void Callback(uint32_t /*display*/, bool connected) {
    if (ignore_) {
      ignore_ = false;
      return;
    }

    LOG_I("IAHotPlugEventCallback --> called.\n");

    m_pCB->hotplugEvent(m_pCB, HWF_DISPLAY_EXTERNAL, connected);
  }

 private:
  hwf_callback const *m_pCB;
  bool ignore_ = true;
};

/* Callback function */

void HwfDevice::registerCallback(struct hwf_device_t *device,
                                 hwf_callback_t const *callback) {
  CTRACE();
  LOG_I("HwfDevice::registerCallback --> called.\n");

  HwfDevice *hwf_device = (HwfDevice *)device;

  hwf_device->m_phwf_callback = (hwf_callback *)callback;

  hwcomposer::NativeDisplay *display = hwf_device->primary_display_.display_;

  auto vsync_callback = std::make_shared<IAVsyncCallback>(callback);
  display->RegisterVsyncCallback(std::move(vsync_callback), 0);

  std::vector<HwfDisplay> &extended = hwf_device->extended_displays_;
  size_t size = extended.size();
  for (size_t i = 0; i < size; i++) {
    auto extended_callback = std::make_shared<IAVsyncCallback>(callback);
    extended.at(i)
        .display_->RegisterVsyncCallback(std::move(extended_callback), 1);

    auto hotplug_callback = std::make_shared<IAHotPlugEventCallback>(callback);
    extended.at(i)
        .display_->RegisterHotPlugCallback(std::move(hotplug_callback), 1);
  }

  return;
}

int HwfDevice::queryDispConfigs(struct hwf_device_t *device, int disp,
                                uint32_t *configs, int *numConfigs) {
  CTRACE();
  HwfDevice *hwf_device = (HwfDevice *)device;

  uint32_t size = *numConfigs;
  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  hwcomposer::NativeDisplay *temp = native_display->display_;

  if (!temp->GetDisplayConfigs(&size, configs)) {
    LOG_E("GetDisplayConfigs failed @ Display: %d, size: %d, configs: %u.",
          disp, size, *configs);
    return -1;
  }

  *numConfigs = size;

  LOG_I("HwfDevice::queryDispConfigs --> disp: %d, numConfigs: %d.\n", disp,
        *numConfigs);

  return *numConfigs == 0 ? -1 : 0;
}

int HwfDevice::queryDispAttribs(struct hwf_device_t *device, int disp,
                                uint32_t config, const uint32_t *attributes,
                                int32_t *values) {
  CTRACE();
  LOG_I("    HwfDevice::queryDispAttribs --> disp: %d.\n", disp);
  HwfDevice *hwf_device = (HwfDevice *)device;

  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  hwcomposer::NativeDisplay *temp = native_display->display_;
  for (int i = 0; attributes[i] != HWF_DISPLAY_NO_ATTRIBUTE; ++i) {
    switch (attributes[i]) {
      case HWF_DISPLAY_WIDTH:
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kWidth, &values[i]);
        break;
      case HWF_DISPLAY_HEIGHT:
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kHeight, &values[i]);
        break;
      case HWF_DISPLAY_VSYNC_PERIOD:
        // in nanoseconds
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kRefreshRate, &values[i]);
        break;
      case HWF_DISPLAY_DPI_X:
        // Dots per 1000 inches
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kDpiX, &values[i]);
        break;
      case HWF_DISPLAY_DPI_Y:
        // Dots per 1000 inches
        temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kDpiY, &values[i]);
        break;
      default:
        values[i] = -1;
        return -1;
    }

    LOG_I("    HwfDevice::queryDispAttribs --> attributes[%d]: %d.\n", i,
          values[i]);
  }

  return 0;
}

void HwfDevice::dump(struct hwf_device_t *device, char *buff, int buff_len) {
  CTRACE();
  LOG_I("HwfDevice::dump --> called.\n");
}

/*************************/
int32_t hwf_close(VendorDevice *device) {
  LOG_I("HwfDevice::hwf_close --> called.\n");

  HwfDevice *hwf_device = (HwfDevice *)device;
  delete hwf_device;

  return 0;
}

int32_t hwf_open(struct hwf_device_t **device, const VendorModule *module) {
  CTRACE();
  LOG_I("HwfDevice::hwf_open --> called.\n");

  HwfDevice *hwf_device = new HwfDevice();
  if (!hwf_device) {
    LOG_E("Failed to allocate hwc context");
    return -ENOMEM;
  }

  hwcomposer::GpuDevice *p_gpu_device = &(hwf_device->device_);
  if (!p_gpu_device->Initialize()) {
    LOG_E("Can't initialize drm object.");
    return -1;
  }

  const std::vector<hwcomposer::NativeDisplay *> &displays =
      p_gpu_device->GetAllDisplays();

  hwf_device->virtual_display_.display_ = p_gpu_device->GetVirtualDisplay();
  // TODO: SetExplicitSyncSupport
  hwf_device->virtual_display_.display_->SetExplicitSyncSupport(
      hwf_device->disable_explicit_sync_);
  hwf_device->virtual_display_.timeline_.Init();

  size_t size = displays.size();
  hwcomposer::NativeDisplay *primary_display = displays.at(0);
  hwf_device->primary_display_.display_ = primary_display;
  hwf_device->primary_display_.display_id_ = 0;
  // TODO: SetExplicitSyncSupport
  hwf_device->primary_display_.display_->SetExplicitSyncSupport(
      hwf_device->disable_explicit_sync_);
  hwf_device->primary_display_.timeline_.Init();

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
    LOG_E("Could not find active mode for %d", default_config);
    return -1;
  }

  for (size_t i = 1; i < size; ++i) {
    hwf_device->extended_displays_.emplace_back();
    HwfDisplay &temp = hwf_device->extended_displays_.back();
    temp.display_ = displays.at(i);
    temp.display_id_ = i;
    temp.timeline_.Init();
    temp.display_->SetExplicitSyncSupport(hwf_device->disable_explicit_sync_);
  }

  hwf_device->base.common.module = module;
  hwf_device->base.common.destroy = hwcomposer::hwf_close;
  hwf_device->base.detect = HwfDevice::detect;
  hwf_device->base.flip = HwfDevice::flip;
  hwf_device->base.setEventState = HwfDevice::setEventState;
  hwf_device->base.setDisplayState = HwfDevice::setDisplayState;
  hwf_device->base.lookup = HwfDevice::lookup;
  hwf_device->base.registerCallback = HwfDevice::registerCallback;
  hwf_device->base.queryDispConfigs = HwfDevice::queryDispConfigs;
  hwf_device->base.queryDispAttribs = HwfDevice::queryDispAttribs;
  hwf_device->base.dump = HwfDevice::dump;

  *device = &hwf_device->base;

  return 0;
}

static int32_t hwf_device_open(const VendorModule *module, const char *id,
                               VendorDevice **device) {
  CTRACE();
  LOG_I("open hwf module, id:%s", id);
  struct hwf_device_t **dev = (struct hwf_device_t **)device;
  int err = hwf_open(dev, module);

  return err;
}

}  // namespace hwcomposer

hwf_module_t hwf_module_entry = {
    .common = {
        .version = 1,
        .id = "Hwf",
        .name = "Hwf",
        .author = "intel",
        .createDevice = &hwcomposer::hwf_device_open,
    },
};

VENDOR_MODULE_ENTRY(hwf_module_entry)
