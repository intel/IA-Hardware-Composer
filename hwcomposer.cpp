/*
 * Copyright (C) 2015 The Android Open Source Project
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
#define LOG_TAG "hwcomposer-drm"

#include "drm_hwcomposer.h"
#include "drmresources.h"
#include "importer.h"
#include "vsyncworker.h"

#include <stdlib.h>

#include <map>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <utils/Trace.h>

#define UM_PER_INCH 25400
#define HWC_FB_BUFFERS 3

namespace android {

typedef struct hwc_drm_display {
  struct hwc_context_t *ctx;
  int display;

  std::vector<uint32_t> config_ids;

  VSyncWorker vsync_worker;
} hwc_drm_display_t;

struct hwc_context_t {
  // map of display:hwc_drm_display_t
  typedef std::map<int, hwc_drm_display_t> DisplayMap;
  typedef DisplayMap::iterator DisplayMapIter;

  hwc_context_t() : procs(NULL), importer(NULL) {
  }

  ~hwc_context_t() {
    delete importer;
  }

  hwc_composer_device_1_t device;
  hwc_procs_t const *procs;

  DisplayMap displays;
  DrmResources drm;
  Importer *importer;
};

static void hwc_dump(struct hwc_composer_device_1 *dev, char *buff,
                     int buff_len) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  std::ostringstream out;

  ctx->drm.compositor()->Dump(&out);
  std::string out_str = out.str();
  strncpy(buff, out_str.c_str(), std::min((size_t)buff_len, out_str.length()));
}

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t num_displays,
                       hwc_display_contents_1_t **display_contents) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  for (int i = 0; i < (int)num_displays; ++i) {
    if (!display_contents[i])
      continue;

    DrmCrtc *crtc = ctx->drm.GetCrtcForDisplay(i);
    if (!crtc) {
      ALOGE("No crtc for display %d", i);
      return -ENODEV;
    }

    int num_layers = display_contents[i]->numHwLayers;
    for (int j = 0; j < num_layers; j++) {
      hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

      if (layer->compositionType == HWC_FRAMEBUFFER)
        layer->compositionType = HWC_OVERLAY;
    }
  }

  return 0;
}

static void hwc_set_cleanup(size_t num_displays,
                            hwc_display_contents_1_t **display_contents) {
  for (int i = 0; i < (int)num_displays; ++i) {
    if (!display_contents[i])
      continue;

    hwc_display_contents_1_t *dc = display_contents[i];
    for (size_t j = 0; j < dc->numHwLayers; ++j) {
      hwc_layer_1_t *layer = &dc->hwLayers[j];
      if (layer->acquireFenceFd >= 0) {
        close(layer->acquireFenceFd);
        layer->acquireFenceFd = -1;
      }
    }
    if (dc->outbufAcquireFenceFd >= 0) {
      close(dc->outbufAcquireFenceFd);
      dc->outbufAcquireFenceFd = -1;
    }
  }
}

static void hwc_add_layer_to_retire_fence(
    hwc_layer_1_t *layer, hwc_display_contents_1_t *display_contents) {
  if (layer->releaseFenceFd < 0)
    return;

  if (display_contents->retireFenceFd >= 0) {
    int old_retire_fence = display_contents->retireFenceFd;
    display_contents->retireFenceFd =
        sync_merge("dc_retire", old_retire_fence, layer->releaseFenceFd);
    close(old_retire_fence);
  } else {
    display_contents->retireFenceFd = dup(layer->releaseFenceFd);
  }
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays,
                   hwc_display_contents_1_t **display_contents) {
  ATRACE_CALL();
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  int ret;
  std::unique_ptr<DrmComposition> composition(
      ctx->drm.compositor()->CreateComposition(ctx->importer));
  if (!composition) {
    ALOGE("Drm composition init failed");
    hwc_set_cleanup(num_displays, display_contents);
    return -EINVAL;
  }

  std::vector<DrmCompositionDisplayLayersMap> layers_map;
  std::vector<std::vector<size_t>> layers_indices;
  layers_map.reserve(num_displays);
  layers_indices.reserve(num_displays);

  for (int i = 0; i < (int)num_displays; ++i) {
    if (!display_contents[i])
      continue;
    hwc_display_contents_1_t *dc = display_contents[i];

    layers_map.emplace_back();
    DrmCompositionDisplayLayersMap &map = layers_map[i];
    map.display = i;
    map.layers = dc->hwLayers;

    std::vector<size_t> indices_to_composite;
    unsigned num_dc_layers = dc->numHwLayers;
    for (int j = 0; j < (int)num_dc_layers; ++j) {
      hwc_layer_1_t *layer = &dc->hwLayers[j];
      if (layer->flags & HWC_SKIP_LAYER)
        continue;
      if (layer->compositionType == HWC_OVERLAY)
        indices_to_composite.push_back(j);
    }

    map.num_layers = indices_to_composite.size();
    layers_indices.emplace_back(std::move(indices_to_composite));
    map.layer_indices = layers_indices.back().data();
  }

  ret = composition->SetLayers(layers_map.size(), layers_map.data());
  if (ret) {
    hwc_set_cleanup(num_displays, display_contents);
    return -EINVAL;
  }

  ret = ctx->drm.compositor()->QueueComposition(std::move(composition));
  if (ret) {
    hwc_set_cleanup(num_displays, display_contents);
    return -EINVAL;
  }

  composition.reset(NULL);

  for (int i = 0; i < (int)num_displays; ++i) {
    if (!display_contents[i])
      continue;
    hwc_display_contents_1_t *dc = display_contents[i];
    unsigned num_dc_layers = dc->numHwLayers;
    for (int j = 0; j < (int)num_dc_layers; ++j) {
      hwc_layer_1_t *layer = &dc->hwLayers[j];
      if (layer->flags & HWC_SKIP_LAYER)
        continue;
      if (layer->compositionType == HWC_OVERLAY)
        hwc_add_layer_to_retire_fence(layer, dc);
    }
  }

  if (ret) {
    ALOGE("Failed to queue the composition");
  }
  hwc_set_cleanup(num_displays, display_contents);
  return ret;
}

static int hwc_event_control(struct hwc_composer_device_1 *dev, int display,
                             int event, int enabled) {
  if (event != HWC_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  hwc_drm_display_t *hd = &ctx->displays[display];
  return hd->vsync_worker.VSyncControl(enabled);
}

static int hwc_set_power_mode(struct hwc_composer_device_1 *dev, int display,
                              int mode) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

  uint64_t dpmsValue = 0;
  switch (mode) {
    case HWC_POWER_MODE_OFF:
      dpmsValue = DRM_MODE_DPMS_OFF;
      break;

    /* We can't support dozing right now, so go full on */
    case HWC_POWER_MODE_DOZE:
    case HWC_POWER_MODE_DOZE_SUSPEND:
    case HWC_POWER_MODE_NORMAL:
      dpmsValue = DRM_MODE_DPMS_ON;
      break;
  };
  return ctx->drm.SetDpmsMode(display, dpmsValue);
}

static int hwc_query(struct hwc_composer_device_1 * /* dev */, int what,
                     int *value) {
  switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
      *value = 0; /* TODO: We should do this */
      break;
    case HWC_VSYNC_PERIOD:
      ALOGW("Query for deprecated vsync value, returning 60Hz");
      *value = 1000 * 1000 * 1000 / 60;
      break;
    case HWC_DISPLAY_TYPES_SUPPORTED:
      *value = HWC_DISPLAY_PRIMARY | HWC_DISPLAY_EXTERNAL;
      break;
  }
  return 0;
}

static void hwc_register_procs(struct hwc_composer_device_1 *dev,
                               hwc_procs_t const *procs) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

  ctx->procs = procs;

  for (hwc_context_t::DisplayMapIter iter = ctx->displays.begin();
       iter != ctx->displays.end(); ++iter) {
    iter->second.vsync_worker.SetProcs(procs);
  }
}

static int hwc_get_display_configs(struct hwc_composer_device_1 *dev,
                                   int display, uint32_t *configs,
                                   size_t *num_configs) {
  if (!*num_configs)
    return 0;

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  hwc_drm_display_t *hd = &ctx->displays[display];
  hd->config_ids.clear();

  DrmConnector *connector = ctx->drm.GetConnectorForDisplay(display);
  if (!connector) {
    ALOGE("Failed to get connector for display %d", display);
    return -ENODEV;
  }

  int ret = connector->UpdateModes();
  if (ret) {
    ALOGE("Failed to update display modes %d", ret);
    return ret;
  }

  for (DrmConnector::ModeIter iter = connector->begin_modes();
       iter != connector->end_modes(); ++iter) {
    size_t idx = hd->config_ids.size();
    if (idx == *num_configs)
      break;
    hd->config_ids.push_back(iter->id());
    configs[idx] = iter->id();
  }
  *num_configs = hd->config_ids.size();
  return *num_configs == 0 ? -1 : 0;
}

static int hwc_get_display_attributes(struct hwc_composer_device_1 *dev,
                                      int display, uint32_t config,
                                      const uint32_t *attributes,
                                      int32_t *values) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  DrmConnector *c = ctx->drm.GetConnectorForDisplay(display);
  if (!c) {
    ALOGE("Failed to get DrmConnector for display %d", display);
    return -ENODEV;
  }
  DrmMode mode;
  for (DrmConnector::ModeIter iter = c->begin_modes(); iter != c->end_modes();
       ++iter) {
    if (iter->id() == config) {
      mode = *iter;
      break;
    }
  }
  if (mode.id() == 0) {
    ALOGE("Failed to find active mode for display %d", display);
    return -ENOENT;
  }

  uint32_t mm_width = c->mm_width();
  uint32_t mm_height = c->mm_height();
  for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; ++i) {
    switch (attributes[i]) {
      case HWC_DISPLAY_VSYNC_PERIOD:
        values[i] = 1000 * 1000 * 1000 / mode.v_refresh();
        break;
      case HWC_DISPLAY_WIDTH:
        values[i] = mode.h_display();
        break;
      case HWC_DISPLAY_HEIGHT:
        values[i] = mode.v_display();
        break;
      case HWC_DISPLAY_DPI_X:
        /* Dots per 1000 inches */
        values[i] = mm_width ? (mode.h_display() * UM_PER_INCH) / mm_width : 0;
        break;
      case HWC_DISPLAY_DPI_Y:
        /* Dots per 1000 inches */
        values[i] =
            mm_height ? (mode.v_display() * UM_PER_INCH) / mm_height : 0;
        break;
    }
  }
  return 0;
}

static int hwc_get_active_config(struct hwc_composer_device_1 *dev,
                                 int display) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  DrmConnector *c = ctx->drm.GetConnectorForDisplay(display);
  if (!c) {
    ALOGE("Failed to get DrmConnector for display %d", display);
    return -ENODEV;
  }

  DrmMode mode = c->active_mode();
  hwc_drm_display_t *hd = &ctx->displays[display];
  for (size_t i = 0; i < hd->config_ids.size(); ++i) {
    if (hd->config_ids[i] == mode.id())
      return i;
  }
  return -1;
}

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display,
                                 int index) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  hwc_drm_display_t *hd = &ctx->displays[display];
  if (index >= (int)hd->config_ids.size()) {
    ALOGE("Invalid config index %d passed in", index);
    return -EINVAL;
  }

  DrmConnector *c = ctx->drm.GetConnectorForDisplay(display);
  if (!c) {
    ALOGE("Failed to get connector for display %d", display);
    return -ENODEV;
  }
  DrmMode mode;
  for (DrmConnector::ModeIter iter = c->begin_modes(); iter != c->end_modes();
       ++iter) {
    if (iter->id() == hd->config_ids[index]) {
      mode = *iter;
      break;
    }
  }
  if (mode.id() != hd->config_ids[index]) {
    ALOGE("Could not find active mode for %d/%d", index, hd->config_ids[index]);
    return -ENOENT;
  }
  int ret = ctx->drm.SetDisplayActiveMode(display, mode);
  if (ret) {
    ALOGE("Failed to set active config %d", ret);
    return ret;
  }
  return ret;
}

static int hwc_device_close(struct hw_device_t *dev) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
  delete ctx;
  return 0;
}

/*
 * TODO: This function sets the active config to the first one in the list. This
 * should be fixed such that it selects the preferred mode for the display, or
 * some other, saner, method of choosing the config.
 */
static int hwc_set_initial_config(hwc_drm_display_t *hd) {
  uint32_t config;
  size_t num_configs = 1;
  int ret = hwc_get_display_configs(&hd->ctx->device, hd->display, &config,
                                    &num_configs);
  if (ret || !num_configs)
    return 0;

  ret = hwc_set_active_config(&hd->ctx->device, hd->display, 0);
  if (ret) {
    ALOGE("Failed to set active config d=%d ret=%d", hd->display, ret);
    return ret;
  }

  return ret;
}

static int hwc_initialize_display(struct hwc_context_t *ctx, int display) {
  hwc_drm_display_t *hd = &ctx->displays[display];
  hd->ctx = ctx;
  hd->display = display;

  int ret = hwc_set_initial_config(hd);
  if (ret) {
    ALOGE("Failed to set initial config for d=%d ret=%d", display, ret);
    return ret;
  }

  ret = hd->vsync_worker.Init(&ctx->drm, display);
  if (ret) {
    ALOGE("Failed to create event worker for display %d %d\n", display, ret);
    return ret;
  }

  return 0;
}

static int hwc_enumerate_displays(struct hwc_context_t *ctx) {
  int ret;
  for (DrmResources::ConnectorIter c = ctx->drm.begin_connectors();
       c != ctx->drm.end_connectors(); ++c) {
    ret = hwc_initialize_display(ctx, (*c)->display());
    if (ret) {
      ALOGE("Failed to initialize display %d", (*c)->display());
      return ret;
    }
  }

  return 0;
}

static int hwc_device_open(const struct hw_module_t *module, const char *name,
                           struct hw_device_t **dev) {
  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("Invalid module name- %s", name);
    return -EINVAL;
  }

  struct hwc_context_t *ctx = new hwc_context_t();
  if (!ctx) {
    ALOGE("Failed to allocate hwc context");
    return -ENOMEM;
  }

  int ret = ctx->drm.Init();
  if (ret) {
    ALOGE("Can't initialize Drm object %d", ret);
    delete ctx;
    return ret;
  }

  ctx->importer = Importer::CreateInstance(&ctx->drm);
  if (!ctx->importer) {
    ALOGE("Failed to create importer instance");
    delete ctx;
    return ret;
  }

  ret = hwc_enumerate_displays(ctx);
  if (ret) {
    ALOGE("Failed to enumerate displays: %s", strerror(ret));
    delete ctx;
    return ret;
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
  ctx->device.setCursorPositionAsync = NULL; /* TODO: Add cursor */

  *dev = &ctx->device.common;

  return 0;
}
}

static struct hw_module_methods_t hwc_module_methods = {
  open : android::hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
  common : {
    tag : HARDWARE_MODULE_TAG,
    version_major : 1,
    version_minor : 0,
    id : HWC_HARDWARE_MODULE_ID,
    name : "DRM hwcomposer module",
    author : "The Android Open Source Project",
    methods : &hwc_module_methods,
    dso : NULL,
    reserved : {0},
  }
};
