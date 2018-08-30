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

#include <stdint.h>
#include <memory>

#include "hwctrace.h"

namespace hwcomposer {

int HwfLayer::InitFromHwcLayer(hwf_layer_t *sf_layer) {
  bool surface_damage = true;
  if (!hwc_layer_)
    hwc_layer_ = new hwcomposer::HwcLayer();

  if (hwc_layer_->GetNativeHandle() &&
      (hwc_layer_->GetNativeHandle()->target_ == sf_layer->target))
    surface_damage = false;

  native_handle_.target_ = sf_layer->target;
  hwc_layer_->SetNativeHandle(&native_handle_);
  hwc_layer_->SetAlpha(sf_layer->globalAlpha);
  if (sf_layer->target->fds.num > 0)
    ITRACE("prime_fd (%d)", sf_layer->target->fds.data[0]);
#if 1
  /* temporary workround for alpha blending */
  if (sf_layer->globalAlpha == 255)
    hwc_layer_->SetAlpha(254);
#endif
  hwc_layer_->SetSourceCrop(hwcomposer::HwcRect<float>(
      sf_layer->srcRect.left, sf_layer->srcRect.top, sf_layer->srcRect.right,
      sf_layer->srcRect.bottom));

  hwc_layer_->SetDisplayFrame(
      hwcomposer::HwcRect<int>(sf_layer->destRect.left, sf_layer->destRect.top,
                               sf_layer->destRect.right,
                               sf_layer->destRect.bottom),
      0, 0);

  // Transform
  uint32_t transform = 0;
  if (sf_layer->transform == HWF_TRANSFORM_ROT_270)
    transform = hwcomposer::HWCTransform::kTransform270;
  else if (sf_layer->transform == HWF_TRANSFORM_ROT_180)
    transform = hwcomposer::HWCTransform::kTransform180;
  else {
    if (sf_layer->transform & HWF_TRANSFORM_FLIP_H)
      transform |= hwcomposer::HWCTransform::kReflectX;
    if (sf_layer->transform & HWF_TRANSFORM_FLIP_V)
      transform |= hwcomposer::HWCTransform::kReflectY;
    if (sf_layer->transform & HWF_TRANSFORM_ROT_90)
      transform |= hwcomposer::HWCTransform::kTransform90;
  }

  hwc_layer_->SetTransform(transform);
  hwc_layer_->SetAcquireFence(dup(sf_layer->acquireSyncFd));

  // Blending
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
      ETRACE("Invalid blendMode in hwc_layer_1_t %d", sf_layer->blendMode);
      return -EINVAL;
  }

  // Damage
  hwcomposer::HwcRegion hwc_region;
  if (!surface_damage)
    hwc_region.emplace_back(0, 0, 0, 0);
  hwc_layer_->SetSurfaceDamage(hwc_region);

  // Visible Region
  uint32_t num_rects = sf_layer->visibleRegion.num;
  hwcomposer::HwcRegion visible_region;

  for (size_t rect = 0; rect < num_rects; ++rect) {
    ITRACE("visible_region: (%d) %d %d %d %d", rect,
           sf_layer->visibleRegion.rects[rect].left,
           sf_layer->visibleRegion.rects[rect].top,
           sf_layer->visibleRegion.rects[rect].right,
           sf_layer->visibleRegion.rects[rect].bottom);
    visible_region.emplace_back(sf_layer->visibleRegion.rects[rect].left,
                                sf_layer->visibleRegion.rects[rect].top,
                                sf_layer->visibleRegion.rects[rect].right,
                                sf_layer->visibleRegion.rects[rect].bottom);
  }

  hwc_layer_->SetVisibleRegion(visible_region);

  ITRACE("%.f %.f %.f %.f -> %d %d %d %d blending(%x) alpha(%d)",
         sf_layer->srcRect.left, sf_layer->srcRect.top, sf_layer->srcRect.right,
         sf_layer->srcRect.bottom, sf_layer->destRect.left,
         sf_layer->destRect.top, sf_layer->destRect.right,
         sf_layer->destRect.bottom, sf_layer->blendMode, sf_layer->globalAlpha);
  return 0;
}

/************************/
HwfDisplay *HwfDevice::GetDisplay(int display) {
  int ext_count = extended_displays_.size();
  switch (display) {
    case HWF_DISPLAY_PRIMARY:
      return &primary_display_;

    case HWF_DISPLAY_VIRTUAL:
      return &virtual_display_;

    case HWF_DISPLAY_EXTERNAL:
      return ext_count > 0 ? &extended_displays_.at(0) : NULL;

    case HWF_DISPLAY_EXTERNAL_EXT_1:
      return ext_count > 1 ? &extended_displays_.at(1) : NULL;

    case HWF_DISPLAY_EXTERNAL_EXT_2:
      return ext_count > 2 ? &extended_displays_.at(2) : NULL;

    case HWF_DISPLAY_EXTERNAL_EXT_3:
      return ext_count > 3 ? &extended_displays_.at(3) : NULL;

    case HWF_DISPLAY_EXTERNAL_EXT_4:
      return ext_count > 4 ? &extended_displays_.at(4) : NULL;

    default:
      ETRACE("Error: invalid display %d", display);
      return NULL;
  }
}

int DBG_DumpHwfLayerInfo(struct hwf_device_t *device, int dispCount,
                         hwf_display_t **displays) {
  for (int i = 0; i < dispCount; ++i) {
    if (!displays[i])
      continue;

    ITRACE("\tDisplay No: %d", i);
    int num_layers = displays[i]->numLayers;

    for (int j = 0; j < num_layers; ++j) {
      hwf_layer_t *layer = &displays[i]->hwfLayers[j];

      ITRACE("\t\tLayer No: %d", j);
      // Dumper Layer info:
      switch (layer->composeMode) {
        case HWF_FB:
          ITRACE("\t\t\tLayer->composeMode: %s.\n", "HWF_FB");
          break;

        case HWF_FB_TARGET:
          ITRACE("\t\t\tLayer->composeMode: %s.\n", "HWF_FB_TARGET");
          break;

        case HWF_OVERLAY:
          ITRACE("\t\t\tLayer->composeMode: %s.\n", "HWF_OVERLAY");
          break;

        default:
          ITRACE("\t\t\tLayer->composeMode: %s.\n", "Not Set.");
          break;
      }
    }
  }
  return 0;
}

int HwfDevice::detect(struct hwf_device_t *device, int dispCount,
                      hwf_display_t **displays) {
  CTRACE();
  ITRACE("dispCount: %d\n", dispCount);

  HwfDevice *hwf_device = (HwfDevice *)device;

  int total_displays = (int)dispCount;
  bool disable_overlays = hwf_device->disable_explicit_sync_;

  for (int i = 0; i < total_displays; ++i) {
    if (!displays[i])
      continue;

    if (i == HWF_DISPLAY_VIRTUAL)
      disable_overlays = true;
    else
      disable_overlays = hwf_device->disable_explicit_sync_;

    int num_layers = displays[i]->numLayers;
    HwfDisplay *native_display = hwf_device->GetDisplay(i);
    if (native_display)
      native_display->gl_composition_ = disable_overlays;

    for (int j = 0; j < num_layers; ++j) {
      hwf_layer_t *layer = &displays[i]->hwfLayers[j];

      if (disable_overlays)
        layer->composeMode = HWF_FB;
      else {
        switch (layer->composeMode) {
          case HWF_FB:
            layer->composeMode = HWF_OVERLAY;
            break;
          case HWF_FB_TARGET:
            break;
          case HWF_CURSOR_OVERLAY:
            break;
          default:
            break;
        }
      }
    }
  }

  DBG_DumpHwfLayerInfo(device, dispCount, displays);
  return 0;
}

int HwfDevice::flip(struct hwf_device_t *device, int dispCount,
                    hwf_display_t **displays) {
  CTRACE();
  ITRACE("dispCount: %d\n", dispCount);

  HwfDevice *hwf_device = (HwfDevice *)device;

  for (int i = 0; i < dispCount; ++i) {
    ITRACE("begin flip display[%d]", i);
    hwf_display_t *dc = displays[i];
    if (i == HWF_DISPLAY_VIRTUAL) {
      ITRACE("skip virtual display");
      continue;
    }

    if (!dc) {
      ITRACE("skip empty display");
      continue;
    }

    size_t num_dc_layers = dc->numLayers;
    HwfDisplay *native_display = hwf_device->GetDisplay(i);
    if (!native_display)
      continue;
    dc->retireSyncFd = -1;
    hwcomposer::NativeDisplay *display = native_display->display_;
    std::vector<HwfLayer *> new_layers;
    std::vector<hwcomposer::HwcLayer *> source_layers;
    for (size_t j = 0; j < num_dc_layers; ++j) {
      hwf_layer_t *sf_layer = &dc->hwfLayers[j];
      if (!sf_layer || !sf_layer->target ||
          (sf_layer->flags & HWF_LAYER_IGNORED)) {
        ITRACE("Skip layer: %p %p %x", sf_layer, sf_layer->target,
               sf_layer->flags);
        ITRACE("(%f %f %f %f => %d %d %d %d)", sf_layer->srcRect.left,
               sf_layer->srcRect.top, sf_layer->srcRect.right,
               sf_layer->srcRect.bottom, sf_layer->destRect.left,
               sf_layer->destRect.top, sf_layer->destRect.right,
               sf_layer->destRect.bottom);
        continue;
      }

      if (!native_display->gl_composition_ &&
          (sf_layer->composeMode == HWF_FB_TARGET)) {
        ITRACE("Skip layer: %d %d", native_display->gl_composition_,
               sf_layer->composeMode);
        continue;
      }

      HwfLayer *new_layer = new HwfLayer();

      new_layer->InitFromHwcLayer(sf_layer);
      source_layers.emplace_back(new_layer->hwc_layer_);
      new_layer->index_ = j;
      new_layers.emplace_back(new_layer);
      sf_layer->acquireSyncFd = -1;
      sf_layer->releaseSyncFd = -1;
    }

    int32_t retire_fence = -1;

    ITRACE("Layers to present: %d", source_layers.size());
    bool success = display->Present(source_layers, &retire_fence);
    if (!success) {
      ETRACE("Failed to set layers in the composition");
      return -1;
    }

    ITRACE("retire_fence: %d", retire_fence);
    dc->retireSyncFd = retire_fence;

    size_t size = new_layers.size();
    for (size_t i = 0; i < size; i++) {
      hwcomposer::HwcLayer *layer = new_layers.at(i)->hwc_layer_;
      int32_t release_fence = layer->GetReleaseFence();

      ITRACE("release_fence: %d", release_fence);
      if (release_fence <= 0)
        continue;

      hwf_layer_t *sf_layer = &dc->hwfLayers[new_layers.at(i)->index_];
      sf_layer->releaseSyncFd = release_fence;
    }

    std::vector<hwcomposer::HwcLayer *>().swap(source_layers);

    for (size_t i = 0; i < size; i++) {
      HwfLayer *layer = new_layers.at(i);
      delete layer;
    }
    std::vector<HwfLayer *>().swap(new_layers);

    ITRACE("flip display[%d] end", i);
  }

  return 0;
}

int HwfDevice::setEventState(struct hwf_device_t *device, int disp, int event,
                             int enabled) {
  CTRACE();
  ITRACE("disp:%d, event: %d, enabled: %d", disp, event, enabled);

  if (event != HWF_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  HwfDevice *hwf_device = (HwfDevice *)device;
  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  if (!native_display)
    return -EINVAL;

  hwcomposer::NativeDisplay *nd = native_display->display_;
  nd->VSyncControl(enabled);

  return 0;
}

int HwfDevice::setDisplayState(struct hwf_device_t *device, int disp,
                               int state) {
  CTRACE();
  ITRACE("disp:%d, state: %d", disp, state);

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
      WTRACE("Power mode %d is unsupported", state);
      return -1;
  };

  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  if (!native_display)
    return -1;

  hwcomposer::NativeDisplay *temp = native_display->display_;
  temp->SetPowerMode(power_mode);

  return 0;
}

int HwfDevice::lookup(struct hwf_device_t *device, int what, int *value) {
  CTRACE();
  switch (what) {
    /* TODO: update Display associated mask bits in hwflinger_defs.h and here */
    case HWF_SUITABLE_DISPLAY_TYPES:
      if (value)
        *value = HWF_DISPLAY_PRIMARY_BIT | HWF_DISPLAY_EXTERNAL_BIT |
                 HWF_DISPLAY_VIRTUAL_BIT;
      break;

    default:
      WTRACE("Warning: lookup %d isn't supported", what);
      return -1;
  }

  return 0;
}

/* Callback function */
class IAVsyncCallback : public hwcomposer::VsyncCallback {
 public:
  IAVsyncCallback(hwf_callback const *procs) : m_pCB(procs) {
  }
  void Callback(uint32_t display, int64_t timestamp) {
    m_pCB->vsyncEvent(m_pCB, display, timestamp);
  }

 private:
  hwf_callback const *m_pCB;
};

class IAHotPlugEventCallback : public hwcomposer::HotPlugCallback {
 public:
  IAHotPlugEventCallback(hwf_callback const *procs) : m_pCB(procs) {
  }
  void Callback(uint32_t display, bool connected) {
    if (ignore_) {
      ignore_ = false;
      return;
    }
    ITRACE("IAHotPlugEventCallback is called: %d %d", display, connected);

    m_pCB->hotplugEvent(m_pCB, display, connected);
  }

 private:
  hwf_callback const *m_pCB;
  bool ignore_ = true;
};

/* Callback function */
void HwfDevice::registerCallback(struct hwf_device_t *device,
                                 hwf_callback_t const *callback) {
  CTRACE();

  HwfDevice *hwf_device = (HwfDevice *)device;
  hwcomposer::NativeDisplay *display = hwf_device->primary_display_.display_;

  auto vsync_callback = std::make_shared<IAVsyncCallback>(callback);
  display->RegisterVsyncCallback(std::move(vsync_callback),
                                 HWF_DISPLAY_PRIMARY);

  std::vector<HwfDisplay> &extended = hwf_device->extended_displays_;
  size_t size = extended.size();
  int disp_id;
  for (size_t i = 0; i < size; i++) {
    if (i == 0)
      disp_id = HWF_DISPLAY_EXTERNAL;
    else
      disp_id = HWF_DISPLAY_EXTERNAL_EXT_1 + (i - 1);

    auto extended_callback = std::make_shared<IAVsyncCallback>(callback);
    extended.at(i).display_->RegisterVsyncCallback(std::move(extended_callback),
                                                   disp_id);

    auto hotplug_callback = std::make_shared<IAHotPlugEventCallback>(callback);
    extended.at(i).display_->RegisterHotPlugCallback(
        std::move(hotplug_callback), disp_id);
  }

  return;
}

int HwfDevice::queryDispConfigs(struct hwf_device_t *device, int disp,
                                uint32_t *configs, int *numConfigs) {
  CTRACE();
  HwfDevice *hwf_device = (HwfDevice *)device;

  uint32_t size = *numConfigs;
  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  if (!native_display)
    return -1;

  hwcomposer::NativeDisplay *temp = native_display->display_;

  if (!temp->GetDisplayConfigs(&size, configs)) {
    ETRACE("GetDisplayConfigs failed @ Display: %d", disp);
    return -1;
  }

  *numConfigs = size;
  ITRACE("disp: %d, numConfigs: %d", disp, *numConfigs);

  return *numConfigs <= 0 ? -1 : 0;
}

int HwfDevice::queryDispAttribs(struct hwf_device_t *device, int disp,
                                uint32_t config, const uint32_t *attributes,
                                int32_t *values) {
  CTRACE();
  ITRACE("disp: %d %d", disp, config);
  HwfDevice *hwf_device = (HwfDevice *)device;
  HwfDisplay *native_display = hwf_device->GetDisplay(disp);
  if (!native_display)
    return -1;

  hwcomposer::NativeDisplay *temp = native_display->display_;
  bool ret = false;
  for (int i = 0; attributes[i] != HWF_DISPLAY_NO_ATTRIBUTE; ++i) {
    switch (attributes[i]) {
      case HWF_DISPLAY_WIDTH:
        ret = temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kWidth, &values[i]);
        break;
      case HWF_DISPLAY_HEIGHT:
        ret = temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kHeight, &values[i]);
        break;
      case HWF_DISPLAY_VSYNC_PERIOD:  // in nanoseconds
        ret = temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kRefreshRate, &values[i]);
        break;
      case HWF_DISPLAY_DPI_X:  // Dots per 1000 inches
        ret = temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kDpiX, &values[i]);
        break;
      case HWF_DISPLAY_DPI_Y:  // Dots per 1000 inches
        ret = temp->GetDisplayAttribute(
            config, hwcomposer::HWCDisplayAttribute::kDpiY, &values[i]);
        break;
      default:
        values[i] = -1;
        return -1;
    }

    ITRACE("    attributes[%d]: %d", i, values[i]);
  }

  return ret ? 0 : -1;
}

void HwfDevice::dump(struct hwf_device_t *device, char *buff, int buff_len) {
  CTRACE();
}

/*************************/
int32_t hwf_close(VendorDevice *device) {
  CTRACE();

  HwfDevice *hwf_device = (HwfDevice *)device;
  delete hwf_device;
  return 0;
}

int32_t hwf_open(struct hwf_device_t **device, const VendorModule *module) {
  CTRACE();

  HwfDevice *hwf_device = new HwfDevice();
  if (!hwf_device) {
    ETRACE("Failed to allocate hwc context");
    return -ENOMEM;
  }

  hwcomposer::GpuDevice *p_gpu_device = &(hwf_device->device_);
  if (!p_gpu_device->Initialize()) {
    ETRACE("Can't initialize drm object.");
    return -1;
  }

  std::vector<hwcomposer::NativeDisplay *> displays =
      p_gpu_device->GetAllDisplays();

  // virtual display
  hwf_device->virtual_display_.display_ =
      p_gpu_device->CreateVirtualDisplay(HWF_DISPLAY_VIRTUAL);
  hwf_device->virtual_display_.display_->SetExplicitSyncSupport(
      hwf_device->disable_explicit_sync_);

  // primary display
  size_t size = displays.size();
  hwcomposer::NativeDisplay *primary_display = displays.at(0);
  hwf_device->primary_display_.display_ = primary_display;
  hwf_device->primary_display_.display_->SetExplicitSyncSupport(
      hwf_device->disable_explicit_sync_);

  // Grab the first mode, we'll choose this as the active mode
  uint32_t num_configs = 1;
  uint32_t default_config;
  if (!primary_display->GetDisplayConfigs(&num_configs, &default_config))
    ETRACE("Currently no display is connected");
  else {
    if (!primary_display->SetActiveConfig(default_config))
      ETRACE("Could not find active mode for %d", default_config);
  }

  // extended display
  for (size_t i = 1; i < size; ++i) {
    hwf_device->extended_displays_.emplace_back();
    HwfDisplay &temp = hwf_device->extended_displays_.back();
    temp.display_ = displays.at(i);
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
  ITRACE("open hwf module, id: %s", id);

  struct hwf_device_t **dev = (struct hwf_device_t **)device;
  return hwf_open(dev, module);
}

}  // namespace hwcomposer

hwf_module_t hwf_module_entry = {
    .common =
        {
            .version = 1,
            .id = "Hwf",
            .name = "Hwf",
            .author = "intel",
            .createDevice = &hwcomposer::hwf_device_open,
        },
};

VENDOR_MODULE_ENTRY(hwf_module_entry)
