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

#define LOG_TAG "hwcomposer-drm"

#include "drm_hwcomposer.h"
#include "drmresources.h"

#include <errno.h>
#include <fcntl.h>
#include <list>
#include <map>
#include <pthread.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <sw_sync.h>
#include <sync/sync.h>

#define ARRAY_SIZE(arr) (int)(sizeof(arr) / sizeof((arr)[0]))

#define UM_PER_INCH 25400

namespace android {

struct hwc_worker {
  pthread_t thread;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  bool exit;
};

typedef struct hwc_drm_display {
  struct hwc_context_t *ctx;
  int display;

  std::vector<uint32_t> config_ids;

  struct hwc_worker set_worker;

  std::list<struct hwc_drm_bo> buf_queue;
  struct hwc_drm_bo front;
  pthread_mutex_t flip_lock;
  pthread_cond_t flip_cond;

  int timeline_fd;
  unsigned timeline_next;

  bool enable_vsync_events;
  unsigned int vsync_sequence;
} hwc_drm_display_t;

struct hwc_context_t {
  // map of display:hwc_drm_display_t
  typedef std::map<int, hwc_drm_display_t> DisplayMap;
  typedef DisplayMap::iterator DisplayMapIter;

  hwc_composer_device_1_t device;
  hwc_procs_t const *procs;
  struct hwc_import_context *import_ctx;

  struct hwc_worker event_worker;

  DisplayMap displays;
  DrmResources drm;
};

static int hwc_prepare_layer(hwc_layer_1_t *layer) {
  /* TODO: We can't handle background right now, defer to sufaceFlinger */
  if (layer->compositionType == HWC_BACKGROUND) {
    layer->compositionType = HWC_FRAMEBUFFER;
    ALOGV("Can't handle background layers yet");

    /* TODO: Support sideband compositions */
  } else if (layer->compositionType == HWC_SIDEBAND) {
    layer->compositionType = HWC_FRAMEBUFFER;
    ALOGV("Can't handle sideband content yet");
  }

  layer->hints = 0;

  /* TODO: Handle cursor by setting compositionType=HWC_CURSOR_OVERLAY */
  if (layer->flags & HWC_IS_CURSOR_LAYER) {
    ALOGV("Can't handle async cursors yet");
  }

  /* TODO: Handle transformations */
  if (layer->transform) {
    ALOGV("Can't handle transformations yet");
  }

  /* TODO: Handle blending & plane alpha*/
  if (layer->blending == HWC_BLENDING_PREMULT ||
      layer->blending == HWC_BLENDING_COVERAGE) {
    ALOGV("Can't handle blending yet");
  }

  /* TODO: Handle cropping & scaling */

  return 0;
}

static int hwc_prepare(hwc_composer_device_1_t * /* dev */, size_t num_displays,
                       hwc_display_contents_1_t **display_contents) {
  /* TODO: Check flags for HWC_GEOMETRY_CHANGED */

  for (int i = 0; i < (int)num_displays; ++i) {
    if (!display_contents[i])
      continue;

    for (int j = 0; j < (int)display_contents[i]->numHwLayers; ++j) {
      int ret = hwc_prepare_layer(&display_contents[i]->hwLayers[j]);
      if (ret) {
        ALOGE("Failed to prepare layer %d:%d", j, i);
        return ret;
      }
    }
  }

  return 0;
}

static int hwc_queue_vblank_event(struct hwc_drm_display *hd) {
  DrmCrtc *crtc = hd->ctx->drm.GetCrtcForDisplay(hd->display);
  if (!crtc) {
    ALOGE("Failed to get crtc for display");
    return -ENODEV;
  }

  drmVBlank vblank;
  memset(&vblank, 0, sizeof(vblank));

  uint32_t high_crtc = (crtc->pipe() << DRM_VBLANK_HIGH_CRTC_SHIFT);
  vblank.request.type = (drmVBlankSeqType)(
      DRM_VBLANK_ABSOLUTE | DRM_VBLANK_NEXTONMISS | DRM_VBLANK_EVENT |
      (high_crtc & DRM_VBLANK_HIGH_CRTC_MASK));
  vblank.request.signal = (unsigned long)hd;
  vblank.request.sequence = hd->vsync_sequence + 1;

  int ret = drmWaitVBlank(hd->ctx->drm.fd(), &vblank);
  if (ret) {
    ALOGE("Failed to wait for vblank %d", ret);
    return ret;
  }

  return 0;
}

static void hwc_vblank_event_handler(int /* fd */, unsigned int sequence,
                                     unsigned int tv_sec, unsigned int tv_usec,
                                     void *user_data) {
  struct hwc_drm_display *hd = (struct hwc_drm_display *)user_data;

  if (!hd->enable_vsync_events || !hd->ctx->procs->vsync)
    return;

  /*
   * Discard duplicate vsync (can happen when enabling vsync events while
   * already processing vsyncs).
   */
  if (sequence <= hd->vsync_sequence)
    return;

  hd->vsync_sequence = sequence;
  int ret = hwc_queue_vblank_event(hd);
  if (ret)
    ALOGE("Failed to queue vblank event ret=%d", ret);

  int64_t timestamp =
      (int64_t)tv_sec * 1000 * 1000 * 1000 + (int64_t)tv_usec * 1000;
  hd->ctx->procs->vsync(hd->ctx->procs, hd->display, timestamp);
}

static void hwc_flip_event_handler(int /* fd */, unsigned int /* sequence */,
                                   unsigned int /* tv_sec */,
                                   unsigned int /* tv_usec */,
                                   void *user_data) {
  struct hwc_drm_display *hd = (struct hwc_drm_display *)user_data;

  int ret = pthread_mutex_lock(&hd->flip_lock);
  if (ret) {
    ALOGE("Failed to lock flip lock ret=%d", ret);
    return;
  }

  ret = pthread_cond_signal(&hd->flip_cond);
  if (ret)
    ALOGE("Failed to signal flip condition ret=%d", ret);

  ret = pthread_mutex_unlock(&hd->flip_lock);
  if (ret) {
    ALOGE("Failed to unlock flip lock ret=%d", ret);
    return;
  }
}

static void *hwc_event_worker(void *arg) {
  setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

  struct hwc_context_t *ctx = (struct hwc_context_t *)arg;
  do {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ctx->drm.fd(), &fds);

    drmEventContext event_context;
    event_context.version = DRM_EVENT_CONTEXT_VERSION;
    event_context.page_flip_handler = hwc_flip_event_handler;
    event_context.vblank_handler = hwc_vblank_event_handler;

    int ret;
    do {
      ret = select(ctx->drm.fd() + 1, &fds, NULL, NULL, NULL);
    } while (ret == -1 && errno == EINTR);

    if (ret != 1) {
      ALOGE("Failed waiting for drm event\n");
      continue;
    }

    drmHandleEvent(ctx->drm.fd(), &event_context);
  } while (true);

  return NULL;
}

static bool hwc_mode_is_equal(drmModeModeInfoPtr a, drmModeModeInfoPtr b) {
  return a->clock == b->clock && a->hdisplay == b->hdisplay &&
         a->hsync_start == b->hsync_start && a->hsync_end == b->hsync_end &&
         a->htotal == b->htotal && a->hskew == b->hskew &&
         a->vdisplay == b->vdisplay && a->vsync_start == b->vsync_start &&
         a->vsync_end == b->vsync_end && a->vtotal == b->vtotal &&
         a->vscan == b->vscan && a->vrefresh == b->vrefresh &&
         a->flags == b->flags && a->type == b->type &&
         !strcmp(a->name, b->name);
}

static int hwc_flip(struct hwc_drm_display *hd, struct hwc_drm_bo *buf) {
  DrmCrtc *crtc = hd->ctx->drm.GetCrtcForDisplay(hd->display);
  if (!crtc) {
    ALOGE("Failed to get crtc for display %d", hd->display);
    return -ENODEV;
  }

  DrmConnector *connector = hd->ctx->drm.GetConnectorForDisplay(hd->display);
  if (!connector) {
    ALOGE("Failed to get connector for display %d", hd->display);
    return -ENODEV;
  }

  int ret;
  if (crtc->requires_modeset()) {
    drmModeModeInfo drm_mode;
    connector->active_mode().ToModeModeInfo(&drm_mode);
    uint32_t connector_id = connector->id();
    ret = drmModeSetCrtc(hd->ctx->drm.fd(), crtc->id(), buf->fb_id, 0, 0,
                         &connector_id, 1, &drm_mode);
    if (ret) {
      ALOGE("Modeset failed for crtc %d", crtc->id());
      return ret;
    }
    return 0;
  }

  ret = drmModePageFlip(hd->ctx->drm.fd(), crtc->id(), buf->fb_id,
                        DRM_MODE_PAGE_FLIP_EVENT, hd);
  if (ret) {
    ALOGE("Failed to flip buffer for crtc %d", crtc->id());
    return ret;
  }

  ret = pthread_cond_wait(&hd->flip_cond, &hd->flip_lock);
  if (ret) {
    ALOGE("Failed to wait on condition %d", ret);
    return ret;
  }

  return 0;
}

static int hwc_wait_and_set(struct hwc_drm_display *hd,
                            struct hwc_drm_bo *buf) {
  int ret;
  if (buf->acquire_fence_fd >= 0) {
    ret = sync_wait(buf->acquire_fence_fd, -1);
    close(buf->acquire_fence_fd);
    buf->acquire_fence_fd = -1;
    if (ret) {
      ALOGE("Failed to wait for acquire %d", ret);
      return ret;
    }
  }

  ret = hwc_flip(hd, buf);
  if (ret) {
    ALOGE("Failed to perform flip\n");
    return ret;
  }

  if (hwc_import_bo_release(hd->ctx->drm.fd(), hd->ctx->import_ctx,
                            &hd->front)) {
    struct drm_gem_close args;
    memset(&args, 0, sizeof(args));
    for (int i = 0; i < ARRAY_SIZE(hd->front.gem_handles); ++i) {
      if (!hd->front.gem_handles[i])
        continue;

      ret = pthread_mutex_lock(&hd->set_worker.lock);
      if (ret) {
        ALOGE("Failed to lock set lock in wait_and_set() %d", ret);
        continue;
      }

      /* check for duplicate handle in buf_queue */
      bool found = false;
      for (std::list<struct hwc_drm_bo>::iterator bi = hd->buf_queue.begin();
           bi != hd->buf_queue.end(); ++bi)
        for (int j = 0; j < ARRAY_SIZE(bi->gem_handles); ++j)
          if (hd->front.gem_handles[i] == bi->gem_handles[j])
            found = true;

      for (int j = 0; j < ARRAY_SIZE(buf->gem_handles); ++j)
        if (hd->front.gem_handles[i] == buf->gem_handles[j])
          found = true;

      if (!found) {
        args.handle = hd->front.gem_handles[i];
        drmIoctl(hd->ctx->drm.fd(), DRM_IOCTL_GEM_CLOSE, &args);
      }
      if (pthread_mutex_unlock(&hd->set_worker.lock))
        ALOGE("Failed to unlock set lock in wait_and_set() %d", ret);
    }
  }

  hd->front = *buf;

  return ret;
}

static void *hwc_set_worker(void *arg) {
  setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

  struct hwc_drm_display *hd = (struct hwc_drm_display *)arg;
  int ret = pthread_mutex_lock(&hd->flip_lock);
  if (ret) {
    ALOGE("Failed to lock flip lock ret=%d", ret);
    return NULL;
  }

  do {
    ret = pthread_mutex_lock(&hd->set_worker.lock);
    if (ret) {
      ALOGE("Failed to lock set lock %d", ret);
      return NULL;
    }

    if (hd->set_worker.exit)
      break;

    if (hd->buf_queue.empty()) {
      ret = pthread_cond_wait(&hd->set_worker.cond, &hd->set_worker.lock);
      if (ret) {
        ALOGE("Failed to wait on condition %d", ret);
        break;
      }
    }

    struct hwc_drm_bo buf;
    buf = hd->buf_queue.front();
    hd->buf_queue.pop_front();

    ret = pthread_mutex_unlock(&hd->set_worker.lock);
    if (ret) {
      ALOGE("Failed to unlock set lock %d", ret);
      return NULL;
    }

    ret = hwc_wait_and_set(hd, &buf);
    if (ret)
      ALOGE("Failed to wait and set %d", ret);

    ret = sw_sync_timeline_inc(hd->timeline_fd, 1);
    if (ret)
      ALOGE("Failed to increment sync timeline %d", ret);
  } while (true);

  ret = pthread_mutex_unlock(&hd->set_worker.lock);
  if (ret)
    ALOGE("Failed to unlock set lock while exiting %d", ret);

  ret = pthread_mutex_unlock(&hd->flip_lock);
  if (ret)
    ALOGE("Failed to unlock flip lock ret=%d", ret);

  return NULL;
}

static void hwc_close_fences(hwc_display_contents_1_t *display_contents) {
  for (int i = 0; i < (int)display_contents->numHwLayers; ++i) {
    hwc_layer_1_t *layer = &display_contents->hwLayers[i];
    if (layer->acquireFenceFd >= 0) {
      close(layer->acquireFenceFd);
      layer->acquireFenceFd = -1;
    }
  }
  if (display_contents->outbufAcquireFenceFd >= 0) {
    close(display_contents->outbufAcquireFenceFd);
    display_contents->outbufAcquireFenceFd = -1;
  }
}

static int hwc_set_display(hwc_context_t *ctx, int display,
                           hwc_display_contents_1_t *display_contents) {
  struct hwc_drm_display *hd = &ctx->displays[display];
  DrmCrtc *crtc = hd->ctx->drm.GetCrtcForDisplay(display);
  if (!crtc) {
    ALOGE("There is no active crtc for display %d", display);
    hwc_close_fences(display_contents);
    return -ENOENT;
  }

  /*
   * TODO: We can only support one hw layer atm, so choose either the
   * first one or the framebuffer target.
   */
  hwc_layer_1_t *layer = NULL;
  if (!display_contents->numHwLayers) {
    return 0;
  } else if (display_contents->numHwLayers == 1) {
    layer = &display_contents->hwLayers[0];
  } else {
    int i;
    for (i = 0; i < (int)display_contents->numHwLayers; ++i) {
      layer = &display_contents->hwLayers[i];
      if (layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        break;
    }
    if (i == (int)display_contents->numHwLayers) {
      ALOGE("Could not find a suitable layer for display %d", display);
    }
  }

  int ret = pthread_mutex_lock(&hd->set_worker.lock);
  if (ret) {
    ALOGE("Failed to lock set lock in set() %d", ret);
    hwc_close_fences(display_contents);
    return ret;
  }

  struct hwc_drm_bo buf;
  memset(&buf, 0, sizeof(buf));
  ret =
      hwc_import_bo_create(ctx->drm.fd(), ctx->import_ctx, layer->handle, &buf);
  if (ret) {
    ALOGE("Failed to import handle to drm bo %d", ret);
    hwc_close_fences(display_contents);
    return ret;
  }
  buf.acquire_fence_fd = layer->acquireFenceFd;
  layer->acquireFenceFd = -1;

  /*
   * TODO: Retire and release can use the same sync point here b/c hwc is
   * restricted to one layer. Once that is no longer true, this will need
   * to change
   */
  ++hd->timeline_next;
  display_contents->retireFenceFd = sw_sync_fence_create(
      hd->timeline_fd, "drm_hwc_retire", hd->timeline_next);
  layer->releaseFenceFd = sw_sync_fence_create(
      hd->timeline_fd, "drm_hwc_release", hd->timeline_next);
  hd->buf_queue.push_back(buf);

  ret = pthread_cond_signal(&hd->set_worker.cond);
  if (ret)
    ALOGE("Failed to signal set worker %d", ret);

  if (pthread_mutex_unlock(&hd->set_worker.lock))
    ALOGE("Failed to unlock set lock in set()");

  hwc_close_fences(display_contents);
  return ret;
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays,
                   hwc_display_contents_1_t **display_contents) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

  int ret = 0;
  for (int i = 0; i < (int)num_displays; ++i) {
    if (display_contents[i])
      ret = hwc_set_display(ctx, i, display_contents[i]);
  }

  return ret;
}

static int hwc_event_control(struct hwc_composer_device_1 *dev, int display,
                             int event, int enabled) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  struct hwc_drm_display *hd = &ctx->displays[display];
  if (event != HWC_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  DrmCrtc *crtc = ctx->drm.GetCrtcForDisplay(display);
  if (!crtc) {
    ALOGD("Can't service events for display %d, no crtc", display);
    return -EINVAL;
  }

  hd->enable_vsync_events = !!enabled;

  if (!hd->enable_vsync_events)
    return 0;

  /*
   * Note that it's possible that the event worker is already waiting for
   * a vsync, and this will be a duplicate request. In that event, we'll
   * end up firing the event handler twice, and it will discard the second
   * event. Not ideal, but not worth introducing a bunch of additional
   * logic/locks/state for.
   */
  int ret = hwc_queue_vblank_event(hd);
  if (ret) {
    ALOGE("Failed to queue vblank event ret=%d", ret);
    return ret;
  }

  return 0;
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

  int ret =
      ctx->drm.SetDisplayActiveMode(display, hd->config_ids[index]);
  if (ret) {
    ALOGE("Failed to set config for display %d", display);
    return ret;
  }

  return ret;
}

static int hwc_destroy_worker(struct hwc_worker *worker) {
  int ret = pthread_mutex_lock(&worker->lock);
  if (ret) {
    ALOGE("Failed to lock in destroy() %d", ret);
    return ret;
  }

  worker->exit = true;

  ret |= pthread_cond_signal(&worker->cond);
  if (ret)
    ALOGE("Failed to signal cond in destroy() %d", ret);

  ret |= pthread_mutex_unlock(&worker->lock);
  if (ret)
    ALOGE("Failed to unlock in destroy() %d", ret);

  ret |= pthread_join(worker->thread, NULL);
  if (ret && ret != ESRCH)
    ALOGE("Failed to join thread in destroy() %d", ret);

  return ret;
}

static void hwc_destroy_display(struct hwc_drm_display *hd) {
  if (hwc_destroy_worker(&hd->set_worker))
    ALOGE("Destroy set worker failed");
}

static int hwc_device_close(struct hw_device_t *dev) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

  for (hwc_context_t::DisplayMapIter iter = ctx->displays.begin();
       iter != ctx->displays.end(); ++iter)
    hwc_destroy_display(&iter->second);

  if (hwc_destroy_worker(&ctx->event_worker))
    ALOGE("Destroy event worker failed");

  int ret = hwc_import_destroy(ctx->import_ctx);
  if (ret)
    ALOGE("Could not destroy import %d", ret);

  delete ctx;

  return 0;
}

static int hwc_initialize_worker(struct hwc_worker *worker,
                                 void *(*routine)(void *), void *arg) {
  int ret = pthread_cond_init(&worker->cond, NULL);
  if (ret) {
    ALOGE("Failed to create worker condition %d", ret);
    return ret;
  }

  ret = pthread_mutex_init(&worker->lock, NULL);
  if (ret) {
    ALOGE("Failed to initialize worker lock %d", ret);
    pthread_cond_destroy(&worker->cond);
    return ret;
  }

  worker->exit = false;

  ret = pthread_create(&worker->thread, NULL, routine, arg);
  if (ret) {
    ALOGE("Could not create worker thread %d", ret);
    pthread_mutex_destroy(&worker->lock);
    pthread_cond_destroy(&worker->cond);
    return ret;
  }
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
  hd->enable_vsync_events = false;
  hd->vsync_sequence = 0;

  int ret = pthread_mutex_init(&hd->flip_lock, NULL);
  if (ret) {
    ALOGE("Failed to initialize flip lock %d", ret);
    return ret;
  }

  ret = pthread_cond_init(&hd->flip_cond, NULL);
  if (ret) {
    ALOGE("Failed to intiialize flip condition %d", ret);
    pthread_mutex_destroy(&hd->flip_lock);
    return ret;
  }

  ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    pthread_cond_destroy(&hd->flip_cond);
    pthread_mutex_destroy(&hd->flip_lock);
    return ret;
  }
  hd->timeline_fd = ret;

  /*
   * Initialize timeline_next to 1, because point 0 will be the very first
   * set operation. Since we increment every time set() is called,
   * initializing to 0 would cause an off-by-one error where
   * surfaceflinger would composite on the front buffer.
   */
  hd->timeline_next = 1;

  ret = hwc_set_initial_config(hd);
  if (ret) {
    ALOGE("Failed to set initial config for d=%d ret=%d", display, ret);
    close(hd->timeline_fd);
    pthread_cond_destroy(&hd->flip_cond);
    pthread_mutex_destroy(&hd->flip_lock);
    return ret;
  }

  ret = hwc_initialize_worker(&hd->set_worker, hwc_set_worker, hd);
  if (ret) {
    ALOGE("Failed to create set worker %d\n", ret);
    close(hd->timeline_fd);
    pthread_cond_destroy(&hd->flip_cond);
    pthread_mutex_destroy(&hd->flip_lock);
    return ret;
  }

  return 0;
}

static void hwc_free_conn_list(drmModeConnectorPtr *conn_list, int num_conn) {
  for (int i = 0; i < num_conn; ++i) {
    if (conn_list[i])
      drmModeFreeConnector(conn_list[i]);
  }
  free(conn_list);
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

  ret = hwc_import_init(&ctx->import_ctx);
  if (ret) {
    ALOGE("Failed to initialize import context");
    delete ctx;
    return ret;
  }

  ret = hwc_enumerate_displays(ctx);
  if (ret) {
    ALOGE("Failed to enumerate displays: %s", strerror(ret));
    delete ctx;
    return ret;
  }

  ret = hwc_initialize_worker(&ctx->event_worker, hwc_event_worker, ctx);
  if (ret) {
    ALOGE("Failed to create event worker %d\n", ret);
    delete ctx;
    return ret;
  }

  ctx->device.common.tag = HARDWARE_DEVICE_TAG;
  ctx->device.common.version = HWC_DEVICE_API_VERSION_1_4;
  ctx->device.common.module = const_cast<hw_module_t *>(module);
  ctx->device.common.close = hwc_device_close;

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
