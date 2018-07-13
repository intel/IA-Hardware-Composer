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

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm.h>
#include <libudev.h>

#include <libsync.h>

#include <iahwc.h>

#include "compositor-iahwc.h"
#include "compositor.h"
#include "gl-renderer.h"
#include "launcher-util.h"
#include "libbacklight.h"
#include "libinput-seat.h"
#include "linux-dmabuf-unstable-v1-server-protocol.h"
#include "linux-dmabuf.h"
#include "pixel-formats.h"
#include "pixman-renderer.h"
#include "presentation-time-server-protocol.h"
#include "shared/helpers.h"
#include "shared/timespec-util.h"
#include "vaapi-recorder.h"
#include "weston-egl-ext.h"

#ifndef GBM_BO_USE_CURSOR
#define GBM_BO_USE_CURSOR GBM_BO_USE_CURSOR_64X64
#endif

#define MAX_CLONED_CONNECTORS 1

struct iahwc_head {
  struct weston_head base;
  struct iahwc_backend *backend;
  uint32_t *mode_configs;
  uint32_t num_configs;
};

// Spin Lock related functions.
struct iahwc_spinlock {
  int atomic_lock;
  bool locked;
};

static void lock(struct iahwc_spinlock *lock) {
  while (__sync_lock_test_and_set(&lock->atomic_lock, 1)) {
  }

  lock->locked = true;
}

static void unlock(struct iahwc_spinlock *lock) {
  lock->locked = false;
  __sync_lock_release(&lock->atomic_lock);
}

struct iahwc_backend {
  struct weston_backend base;
  struct weston_compositor *compositor;

  iahwc_module_t *iahwc_module;
  iahwc_device_t *iahwc_device;

  struct udev *udev;
  struct wl_event_source *iahwc_source;

  struct udev_monitor *udev_monitor;
  struct wl_event_source *udev_iahwc_source;

  struct {
    int id;
    int fd;
    char *filename;
  } iahwc;

  struct gbm_device *gbm;
  struct wl_listener session_listener;
  uint32_t gbm_format;

  IAHWC_PFN_GET_NUM_DISPLAYS iahwc_get_num_displays;
  IAHWC_PFN_REGISTER_CALLBACK iahwc_register_callback;
  IAHWC_PFN_DISPLAY_GET_CONNECTION_STATUS iahwc_display_get_connection_status;
  IAHWC_PFN_DISPLAY_GET_INFO iahwc_get_display_info;
  IAHWC_PFN_DISPLAY_GET_NAME iahwc_get_display_name;
  IAHWC_PFN_DISPLAY_GET_CONFIGS iahwc_get_display_configs;
  IAHWC_PFN_DISPLAY_SET_GAMMA iahwc_set_display_gamma;
  IAHWC_PFN_DISPLAY_SET_CONFIG iahwc_set_display_config;
  IAHWC_PFN_DISPLAY_GET_CONFIG iahwc_get_display_config;
  IAHWC_PFN_DISPLAY_SET_POWER_MODE iahwc_display_set_power_mode;
  IAHWC_PFN_DISPLAY_CLEAR_ALL_LAYERS iahwc_display_clear_all_layers;
  IAHWC_PFN_PRESENT_DISPLAY iahwc_present_display;
  IAHWC_PFN_DISABLE_OVERLAY_USAGE iahwc_disable_overlay_usage;
  IAHWC_PFN_ENABLE_OVERLAY_USAGE iahwc_enable_overlay_usage;
  IAHWC_PFN_CREATE_LAYER iahwc_create_layer;
  IAHWC_PFN_DESTROY_LAYER iahwc_destroy_layer;
  IAHWC_PFN_LAYER_SET_BO iahwc_layer_set_bo;
  IAHWC_PFN_LAYER_SET_RAW_PIXEL_DATA iahwc_layer_set_raw_pixel_data;
  IAHWC_PFN_LAYER_SET_SOURCE_CROP iahwc_layer_set_source_crop;
  IAHWC_PFN_LAYER_SET_DISPLAY_FRAME iahwc_layer_set_display_frame;
  IAHWC_PFN_LAYER_SET_SURFACE_DAMAGE iahwc_layer_set_surface_damage;
  IAHWC_PFN_LAYER_SET_PLANE_ALPHA iahwc_layer_set_plane_alpha;
  IAHWC_PFN_LAYER_SET_ACQUIRE_FENCE iahwc_layer_set_acquire_fence;
  IAHWC_PFN_LAYER_SET_USAGE iahwc_layer_set_usage;
  IAHWC_PFN_LAYER_SET_INDEX iahwc_layer_set_index;

  int sprites_are_broken;
  int sprites_hidden;

  void *repaint_data;

  struct udev_input input;

  int32_t cursor_width;
  int32_t cursor_height;
};

struct iahwc_mode {
  struct weston_mode base;
  uint32_t config_id;
};

struct iahwc_edid {
  char eisa_id[13];
  char monitor_name[13];
  char pnp_id[5];
  char serial_number[13];
};

/**
 * Pending state holds one or more iahwc_output_state structures, collected from
 * performing repaint. This pending state is transient, and only lives between
 * beginning a repaint group and flushing the results: after flush, each
 * output state will complete and be retired separately.
 */
struct iahwc_pending_state {
  struct iahwc_backend *backend;
};

struct iahwc_overlay {
  struct wl_list link;

  struct wl_shm_buffer *shm_memory;

  struct gbm_bo *overlay_bo;
  uint32_t overlay_layer_id;
  uint32_t layer_index;
  struct weston_surface *es;
};

struct iahwc_output {
  struct weston_output base;
  drmModeConnector *connector;

  uint32_t crtc_id; /* object ID to pass to IAHWC functions */
  int pipe;         /* index of CRTC in resource array / bitmasks */
  uint32_t connector_id;

  struct iahwc_edid edid;

  enum dpms_enum dpms;
  struct backlight *backlight;

  bool state_invalid;
  bool overlay_enabled;

  struct weston_plane overlay_plane;
  struct wl_list overlay_list;

  uint32_t gbm_format;

  pixman_region32_t previous_damage;

  struct vaapi_recorder *recorder;
  struct wl_listener recorder_frame_listener;

  int release_fence;
  struct wl_event_source *release_fence_source;
  struct iahwc_spinlock spin_lock;
  struct timespec last_vsync_ts;
  uint32_t total_layers;

  enum dpms_enum current_dpms;
};

static struct gl_renderer_interface *gl_renderer;

static const char default_seat[] = "seat0";

static inline struct iahwc_output *to_iahwc_output(struct weston_output *base) {
  return container_of(base, struct iahwc_output, base);
}

static inline struct iahwc_head *to_iahwc_head(struct weston_head *base) {
  return container_of(base, struct iahwc_head, base);
}

static inline struct iahwc_backend *to_iahwc_backend(
    struct weston_compositor *base) {
  return container_of(base->backend, struct iahwc_backend, base);
}

static void frame_done(struct iahwc_output *output) {
  struct timespec ts;

  uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
                   WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK |
                   WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
  weston_compositor_read_presentation_clock(output->base.compositor, &ts);
  weston_output_finish_frame(&output->base, &ts, flags);
}

static int frame_done_fd(int fd, unsigned int mask, void *data) {
  struct iahwc_output *output = data;
  frame_done(output);
  return 0;
}

static void frame_done_idle(void *data) {
  struct iahwc_output *output = data;
  frame_done(output);
}

/**
 * Allocate a new iahwc_pending_state
 *
 * Allocate a new, empty, 'pending state' structure to be used across a
 * repaint cycle or similar.
 *
 * @param backend IAHWC backend
 * @returns Newly-allocated pending state structure
 */
static struct iahwc_pending_state *iahwc_pending_state_alloc(
    struct iahwc_backend *backend) {
  struct iahwc_pending_state *ret;

  ret = calloc(1, sizeof(*ret));
  if (!ret)
    return NULL;

  ret->backend = backend;

  return ret;
}

/**
 * Free a iahwc_pending_state structure
 *
 * Frees a pending_state structure.
 *
 * @param pending_state Pending state structure to free
 */
static void iahwc_pending_state_free(
    struct iahwc_pending_state *pending_state) {
  if (!pending_state)
    return;

  free(pending_state);
}

static void iahwc_output_set_gamma(struct weston_output *output_base,
                                   uint16_t size, uint16_t *r, uint16_t *g,
                                   uint16_t *b) {
  int rc;
  struct iahwc_output *output = to_iahwc_output(output_base);
  struct iahwc_backend *backend = to_iahwc_backend(output->base.compositor);

  float rs = *r, gs = *g, bs = *b;
  rc = backend->iahwc_set_display_gamma(backend->iahwc_device, 0, rs, gs, bs);
  if (rc)
    weston_log("set gamma failed: %m\n");
}

static int iahwc_output_repaint(struct weston_output *output_base,
                                pixman_region32_t *damage, void *repaint_data) {
  struct iahwc_output *output = to_iahwc_output(output_base);
  struct iahwc_backend *backend = to_iahwc_backend(output->base.compositor);
  struct wl_event_loop *loop;

  weston_log("release fence is %d\n", output->release_fence);
  if (output->release_fence > 0) {
    wl_event_source_remove(output->release_fence_source);
    close(output->release_fence);
    output->release_fence = -1;
    output->release_fence_source = NULL;
  }

  backend->iahwc_present_display(backend->iahwc_device, 0,
                                 &output->release_fence);

  loop = wl_display_get_event_loop(output->base.compositor->wl_display);

  if (output->release_fence > 0) {
    output->release_fence_source = wl_event_loop_add_fd(
        loop, output->release_fence, WL_EVENT_READABLE, frame_done_fd, output);
  } else {
    // when release fence is -1, immediately call frame_done
    wl_event_loop_add_idle(loop, frame_done_idle, output);
  }

  lock(&output->spin_lock);
  output->state_invalid = false;
  unlock(&output->spin_lock);
  return 0;
}

static void iahwc_output_start_repaint_loop(struct weston_output *output_base) {
  struct iahwc_output *output = to_iahwc_output(output_base);

  /* if we cannot page-flip, immediately finish frame */
  lock(&output->spin_lock);
  weston_output_finish_frame(output_base, NULL,
                             WP_PRESENTATION_FEEDBACK_INVALID);
  unlock(&output->spin_lock);
}

static void iahwc_output_destroy(struct weston_output *base);

/**
 * Begin a new repaint cycle
 *
 * Called by the core compositor at the beginning of a repaint cycle.
 */
static void *iahwc_repaint_begin(struct weston_compositor *compositor) {
  struct iahwc_backend *b = to_iahwc_backend(compositor);
  struct iahwc_pending_state *ret;

  ret = iahwc_pending_state_alloc(b);
  b->repaint_data = ret;

  return ret;
}

/**
 * Flush a repaint set
 *
 * Called by the core compositor when a repaint cycle has been completed
 * and should be flushed.
 */
static void iahwc_repaint_flush(struct weston_compositor *compositor,
                                void *repaint_data) {
  struct iahwc_backend *b = to_iahwc_backend(compositor);
  struct iahwc_pending_state *pending_state = repaint_data;

  iahwc_pending_state_free(pending_state);
  b->repaint_data = NULL;
}

/**
 * Cancel a repaint set
 *
 * Called by the core compositor when a repaint has finished, so the data
 * held across the repaint cycle should be discarded.
 */
static void iahwc_repaint_cancel(struct weston_compositor *compositor,
                                 void *repaint_data) {
  struct iahwc_backend *b = to_iahwc_backend(compositor);
  struct iahwc_pending_state *pending_state = repaint_data;

  iahwc_pending_state_free(pending_state);
  b->repaint_data = NULL;
}

/**
 * Find the closest-matching mode for a given target
 *
 * Given a target mode, find the most suitable mode amongst the output's
 * current mode list to use, preferring the current mode if possible, to
 * avoid an expensive mode switch.
 *
 * @param output IAHWC output
 * @param target_mode Mode to attempt to match
 * @returns Pointer to a mode from the output's mode list
 */
static struct iahwc_mode *choose_mode(struct iahwc_output *output,
                                      struct weston_mode *target_mode) {
  struct iahwc_mode *tmp_mode = NULL, *mode;

  if (output->base.current_mode->width == target_mode->width &&
      output->base.current_mode->height == target_mode->height &&
      (output->base.current_mode->refresh == target_mode->refresh ||
       target_mode->refresh == 0))
    return (struct iahwc_mode *)output->base.current_mode;

  wl_list_for_each(mode, &output->base.mode_list, base.link) {
    if (mode->base.width == target_mode->width &&
        mode->base.height == target_mode->height) {
      if (mode->base.refresh == target_mode->refresh ||
          target_mode->refresh == 0) {
        return mode;
      } else if (!tmp_mode)
        tmp_mode = mode;
    }
  }

  return tmp_mode;
}

static int iahwc_output_switch_mode(struct weston_output *output_base,
                                    struct weston_mode *mode) {
  struct iahwc_output *output;
  struct iahwc_mode *iahwc_mode;
  struct iahwc_backend *b;

  if (output_base == NULL) {
    weston_log("output is NULL.\n");
    return -1;
  }

  if (mode == NULL) {
    weston_log("mode is NULL.\n");
    return -1;
  }

  b = to_iahwc_backend(output_base->compositor);
  output = to_iahwc_output(output_base);
  iahwc_mode = choose_mode(output, mode);

  if (!iahwc_mode) {
    weston_log("%s, invalid resolution:%dx%d\n", __func__, mode->width,
               mode->height);
    return -1;
  }

  if (&iahwc_mode->base == output->base.current_mode)
    return 0;

  b->iahwc_set_display_config(b->iahwc_device, 0, iahwc_mode->config_id);

  output->base.current_mode->flags = 0;
  lock(&output->spin_lock);
  output->state_invalid = true;
  unlock(&output->spin_lock);

  output->base.current_mode = &iahwc_mode->base;
  output->base.current_mode->flags =
      WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
  return 0;
}

static struct gbm_device *create_gbm_device(int fd) {
  struct gbm_device *gbm;

  gl_renderer = weston_load_module("gl-renderer.so", "gl_renderer_interface");
  if (!gl_renderer)
    return NULL;

  /* GBM will load a dri driver, but even though they need symbols from
   * libglapi, in some version of Mesa they are not linked to it. Since
   * only the gl-renderer module links to it, the call above won't make
   * these symbols globally available, and loading the DRI driver fails.
   * Workaround this by dlopen()'ing libglapi with RTLD_GLOBAL. */
  dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL);

  gbm = gbm_create_device(fd);

  return gbm;
}

/* When initializing EGL, if the preferred buffer format isn't available
 * we may be able to substitute an ARGB format for an XRGB one.
 *
 * This returns 0 if substitution isn't possible, but 0 might be a
 * legitimate format for other EGL platforms, so the caller is
 * responsible for checking for 0 before calling gl_renderer->create().
 *
 * This works around https://bugs.freedesktop.org/show_bug.cgi?id=89689
 * but it's entirely possible we'll see this again on other implementations.
 */
static int fallback_format_for(uint32_t format) {
  switch (format) {
    case GBM_FORMAT_XRGB8888:
      return GBM_FORMAT_ARGB8888;
    case GBM_FORMAT_XRGB2101010:
      return GBM_FORMAT_ARGB2101010;
    default:
      return 0;
  }
}

static int iahwc_backend_create_gl_renderer(struct iahwc_backend *b) {
  EGLint format[3] = {
      b->gbm_format, fallback_format_for(b->gbm_format), 0,
  };
  int n_formats = 2;

  if (format[1])
    n_formats = 3;
  if (gl_renderer->display_create(
          b->compositor, EGL_PLATFORM_GBM_KHR, (void *)b->gbm, NULL,
          gl_renderer->opaque_attribs, format, n_formats) < 0) {
    return -1;
  }

  return 0;
}

static int init_egl(struct iahwc_backend *b) {
  b->gbm = create_gbm_device(b->iahwc.fd);

  if (!b->gbm)
    return -1;

  if (iahwc_backend_create_gl_renderer(b) < 0) {
    gbm_device_destroy(b->gbm);
    return -1;
  }

  return 0;
}

/**
 * Return's overlay which is showing layer with index layer_index.
 */
static struct iahwc_overlay *iahwc_get_existing_plane(
    struct iahwc_output *output, uint32_t layer_index) {
  struct iahwc_overlay *ps;

  wl_list_for_each(ps, &output->overlay_list, link) {
    if (ps->layer_index == layer_index)
      return ps;
  }

  return NULL;
}

/**
 * Add Overlay information to the list managed by the output.
 */
static void iahwc_add_overlay_info(struct iahwc_overlay *plane,
                                   struct iahwc_output *output,
                                   struct wl_shm_buffer *shm_memory,
                                   struct gbm_bo *overlay_bo,
                                   uint32_t overlay_layer_id,
                                   uint32_t layer_index,
                                   struct weston_surface *es) {
  if (!plane) {
    plane = zalloc(sizeof *plane);
    if (!plane) {
      weston_log("%s: out of memory\n", __func__);
      return;
    }

    wl_list_insert(&output->overlay_list, &plane->link);
  }

  if (shm_memory) {
    plane->shm_memory = shm_memory;
    plane->overlay_bo = 0;
  } else {
    plane->overlay_bo = overlay_bo;
    plane->shm_memory = 0;
  }

  plane->overlay_layer_id = overlay_layer_id;
  plane->layer_index = layer_index;
  plane->es = es;
}

/**
 * Clean up output overlay lists.
 */
static void iahwc_overlay_destroy(struct iahwc_output *output,
                                  uint32_t starting_index) {
  struct iahwc_overlay *plane, *next;
  struct iahwc_backend *b = to_iahwc_backend(output->base.compositor);

  wl_list_for_each_safe(plane, next, &output->overlay_list, link) {
    if (plane->layer_index >= starting_index) {
      b->iahwc_destroy_layer(b->iahwc_device, 0, plane->overlay_layer_id);
      if (plane->overlay_bo)
        gbm_bo_destroy(plane->overlay_bo);

      wl_list_remove(&plane->link);
      free(plane);
    }
  }
}

static struct weston_plane *iahwc_output_prepare_overlay_view(
    struct iahwc_output *output, struct weston_view *ev, uint32_t layer_index) {
  struct weston_compositor *ec = output->base.compositor;
  struct iahwc_backend *b = to_iahwc_backend(ec);
  struct weston_buffer_viewport *viewport = &ev->surface->buffer_viewport;
  struct wl_resource *buffer_resource;
  struct weston_plane *p = &output->overlay_plane;
  struct linux_dmabuf_buffer *dmabuf;
  struct gbm_bo *bo = NULL;
  struct wl_shm_buffer *shmbuf;
  pixman_region32_t dest_rect, src_rect;
  pixman_box32_t *box, tbox;
  wl_fixed_t sx1, sy1, sx2, sy2;
  int32_t src_x, src_y;
  uint32_t src_w, src_h;
  uint32_t dest_x, dest_y;
  uint32_t dest_w, dest_h;
  int is_cusor_layer = 0;
  float x, y;

  uint32_t overlay_layer_id;
  if (ev->surface->buffer_ref.buffer == NULL) {
    return NULL;
  }
  buffer_resource = ev->surface->buffer_ref.buffer->resource;
  shmbuf = wl_shm_buffer_get(buffer_resource);

  struct iahwc_overlay *plane = 0;
  plane = iahwc_get_existing_plane(output, layer_index);
  // Update Damage.
  bool layer_damaged = true;
  bool full_damage = false;
  iahwc_region_t damage_region;
  damage_region.numRects = 1;
  struct weston_surface *es = ev->surface;
  if (!plane) {
    b->iahwc_create_layer(b->iahwc_device, 0, &overlay_layer_id);
    full_damage = true;
  } else if (plane->es == es) {
    overlay_layer_id = plane->overlay_layer_id;
    if (!pixman_region32_not_empty(&es->pending.damage_buffer) &&
        !pixman_region32_not_empty(&es->pending.damage_surface) &&
        !pixman_region32_not_empty(&es->damage)) {
      iahwc_rect_t damage_rect = {0, 0, 0, 0};
      damage_region.rects = &damage_rect;
      b->iahwc_layer_set_surface_damage(b->iahwc_device, 0, overlay_layer_id,
                                        damage_region);
      layer_damaged = false;
    } else {
      pixman_region32_t damage;
      pixman_region32_init(&damage);
      pixman_region32_union(&damage, &es->pending.damage_surface, &es->damage);
      pixman_region32_union(&damage, &es->pending.damage_buffer, &damage);
      pixman_region32_fini(&damage);
      pixman_box32_t *damage_extents = pixman_region32_extents(&damage);
      iahwc_rect_t damage_rect = {damage_extents->x1, damage_extents->y1,
                                  damage_extents->x2, damage_extents->y2};
      damage_region.rects = &damage_rect;
      b->iahwc_layer_set_surface_damage(b->iahwc_device, 0, overlay_layer_id,
                                        damage_region);
    }
  } else {
    overlay_layer_id = plane->overlay_layer_id;
    // Layer might have changed z-order as surface has changed.
    // Mark full surface as damaged.
    full_damage = true;
  }

  pixman_region32_clear(&es->pending.damage_buffer);
  pixman_region32_clear(&es->pending.damage_surface);
  pixman_region32_clear(&es->damage);

  if (shmbuf) {
    if (ev->surface->width <= b->cursor_width &&
        ev->surface->height <= b->cursor_height) {
      is_cusor_layer = 1;
      b->iahwc_layer_set_usage(b->iahwc_device, 0, overlay_layer_id,
                               IAHWC_LAYER_USAGE_CURSOR);
    } else {
      b->iahwc_layer_set_usage(b->iahwc_device, 0, overlay_layer_id,
                               IAHWC_LAYER_USAGE_OVERLAY);
    }
  }

  if (is_cusor_layer) {
    weston_view_to_global_float(ev, 0, 0, &x, &y);
    int32_t surfwidth = ev->surface->width;
    int32_t surfheight = ev->surface->height;
    iahwc_rect_t source_crop = {0, 0, surfwidth, surfheight};

    int32_t disp_width = output->base.current_mode->width;
    int32_t disp_height = output->base.current_mode->height;

    if (x < 0)
      x = 0;
    if (x > disp_width - surfwidth)
      x = disp_width - surfwidth;

    if (y < 0)
      y = 0;
    if (y > disp_height - surfheight)
      y = disp_height - surfheight;

    iahwc_rect_t display_frame = {x, y, surfwidth + x, surfheight + y};

    b->iahwc_layer_set_source_crop(b->iahwc_device, 0, overlay_layer_id,
                                   source_crop);
    b->iahwc_layer_set_display_frame(b->iahwc_device, 0, overlay_layer_id,
                                     display_frame);
    if (full_damage) {
      damage_region.rects = &source_crop;
      b->iahwc_layer_set_surface_damage(b->iahwc_device, 0, overlay_layer_id,
                                        damage_region);
    }
  } else {
    box = pixman_region32_extents(&ev->transform.boundingbox);
    p->x = box->x1;
    p->y = box->y1;

    /*
     * Calculate the source & dest rects properly based on actual
     * position (note the caller has called weston_view_update_transform()
     * for us already).
     */
    pixman_region32_init(&dest_rect);
    pixman_region32_intersect(&dest_rect, &ev->transform.boundingbox,
                              &output->base.region);
    pixman_region32_translate(&dest_rect, -output->base.x, -output->base.y);
    box = pixman_region32_extents(&dest_rect);
    tbox = weston_transformed_rect(output->base.width, output->base.height,
                                   output->base.transform,
                                   output->base.current_scale, *box);
    dest_x = tbox.x1;
    dest_y = tbox.y1;
    dest_w = tbox.x2 - tbox.x1;
    dest_h = tbox.y2 - tbox.y1;
    pixman_region32_fini(&dest_rect);

    pixman_region32_init(&src_rect);
    pixman_region32_intersect(&src_rect, &ev->transform.boundingbox,
                              &output->base.region);
    box = pixman_region32_extents(&src_rect);

    weston_view_from_global_fixed(ev, wl_fixed_from_int(box->x1),
                                  wl_fixed_from_int(box->y1), &sx1, &sy1);
    weston_view_from_global_fixed(ev, wl_fixed_from_int(box->x2),
                                  wl_fixed_from_int(box->y2), &sx2, &sy2);

    if (sx1 < 0)
      sx1 = 0;
    if (sy1 < 0)
      sy1 = 0;
    if (sx2 > wl_fixed_from_int(ev->surface->width))
      sx2 = wl_fixed_from_int(ev->surface->width);
    if (sy2 > wl_fixed_from_int(ev->surface->height))
      sy2 = wl_fixed_from_int(ev->surface->height);

    tbox.x1 = sx1;
    tbox.y1 = sy1;
    tbox.x2 = sx2;
    tbox.y2 = sy2;

    tbox = weston_transformed_rect(wl_fixed_from_int(ev->surface->width),
                                   wl_fixed_from_int(ev->surface->height),
                                   viewport->buffer.transform,
                                   viewport->buffer.scale, tbox);

    src_x = tbox.x1 << 8;
    src_y = tbox.y1 << 8;
    src_w = (tbox.x2 - tbox.x1) << 8;
    src_h = (tbox.y2 - tbox.y1) << 8;
    pixman_region32_fini(&src_rect);

    src_w = src_w >> 16;
    src_h = src_h >> 16;

    iahwc_rect_t source_crop = {src_x, src_y, src_w + src_x, src_h + src_y};

    iahwc_rect_t display_frame = {dest_x, dest_y, dest_w + dest_x,
                                  dest_h + dest_y};

    b->iahwc_layer_set_source_crop(b->iahwc_device, 0, overlay_layer_id,
                                   source_crop);
    b->iahwc_layer_set_display_frame(b->iahwc_device, 0, overlay_layer_id,
                                     display_frame);
    if (full_damage) {
      damage_region.rects = &source_crop;
      b->iahwc_layer_set_surface_damage(b->iahwc_device, 0, overlay_layer_id,
                                        damage_region);
    }
  }

  if (layer_damaged) {
    if (shmbuf) {
      struct iahwc_raw_pixel_data dbo;
      dbo.width = ev->surface->width;
      dbo.height = ev->surface->height;
      dbo.format = wl_shm_buffer_get_format(shmbuf);
      dbo.buffer = wl_shm_buffer_get_data(shmbuf);
      dbo.stride = wl_shm_buffer_get_stride(shmbuf);

      switch (dbo.format) {
        case WL_SHM_FORMAT_XRGB8888:
          dbo.format = DRM_FORMAT_XRGB8888;
          break;
        case WL_SHM_FORMAT_ARGB8888:
          dbo.format = DRM_FORMAT_ARGB8888;
          break;
        case WL_SHM_FORMAT_RGB565:
          dbo.format = DRM_FORMAT_RGB565;
          break;
        case WL_SHM_FORMAT_YUV420:
          dbo.format = DRM_FORMAT_YUV420;
          break;
        case WL_SHM_FORMAT_NV12:
          dbo.format = DRM_FORMAT_NV12;
          break;
        case WL_SHM_FORMAT_YUYV:
          dbo.format = DRM_FORMAT_YUYV;
          break;
        default:
          weston_log("warning: unknown shm buffer format: %08x\n", dbo.format);
      }

      dbo.callback_data = shmbuf;
      int ret = b->iahwc_layer_set_raw_pixel_data(b->iahwc_device, 0,
                                                  overlay_layer_id, dbo);
      if (ret == -1) {
        // Destroy the layer in case it's not already mapped to a plane.
        if (!plane)
          b->iahwc_destroy_layer(b->iahwc_device, 0, overlay_layer_id);

        return NULL;
      }
    } else {
      if ((dmabuf = linux_dmabuf_buffer_get(buffer_resource))) {
        /* XXX: TODO:
         *
         * Use AddFB2 directly, do not go via GBM.
         * Add support for multiplanar formats.
         * Both require refactoring in the IAHWC-backend to
         * support a mix of gbm_bos and iahwcfbs.
         */
        struct gbm_import_fd_data gbm_dmabuf = {
            .fd = dmabuf->attributes.fd[0],
            .width = dmabuf->attributes.width,
            .height = dmabuf->attributes.height,
            .stride = dmabuf->attributes.stride[0],
            .format = dmabuf->attributes.format};

        /* XXX: TODO:
         *
         * Currently the buffer is rejected if any dmabuf attribute
         * flag is set.  This keeps us from passing an inverted /
         * interlaced / bottom-first buffer (or any other type that may
         * be added in the future) through to an overlay.  Ultimately,
         * these types of buffers should be handled through buffer
         * transforms and not as spot-checks requiring specific
         * knowledge. */
        if (dmabuf->attributes.n_planes != 1 ||
            dmabuf->attributes.offset[0] != 0 || dmabuf->attributes.flags) {
          return NULL;
        }

        bo = gbm_bo_import(b->gbm, GBM_BO_IMPORT_FD, &gbm_dmabuf,
                           GBM_BO_USE_SCANOUT);
      } else {
        bo = gbm_bo_import(b->gbm, GBM_BO_IMPORT_WL_BUFFER, buffer_resource,
                           GBM_BO_USE_SCANOUT);
      }

      if (!bo) {
        return NULL;
      }

      b->iahwc_layer_set_usage(b->iahwc_device, 0, overlay_layer_id,
                               IAHWC_LAYER_USAGE_OVERLAY);
      b->iahwc_layer_set_bo(b->iahwc_device, 0, overlay_layer_id, bo);
    }

    b->iahwc_layer_set_index(b->iahwc_device, 0, overlay_layer_id, layer_index);

    iahwc_add_overlay_info(plane, output, shmbuf, bo, overlay_layer_id,
                           layer_index, ev->surface);
  }
  es->keep_buffer = true;

  return p;
}

/**
 * Add a mode to output's mode list
 *
 * Copy the supplied IAHWC mode into a Weston mode structure, and add it to the
 * output's mode list.
 *
 * @param output IAHWC output to add mode to
 * @param info IAHWC mode structure to add
 * @returns Newly-allocated Weston/IAHWC mode structure
 */
static int iahwc_output_add_mode(struct iahwc_backend *b,
                                 struct iahwc_output *output, int config_id) {
  struct iahwc_mode *mode;
  int refresh;

  mode = malloc(sizeof *mode);
  if (mode == NULL)
    return -1;

  mode->base.flags = 0;
  b->iahwc_get_display_info(b->iahwc_device, 0, config_id, IAHWC_CONFIG_WIDTH,
                            &mode->base.width);
  b->iahwc_get_display_info(b->iahwc_device, 0, config_id, IAHWC_CONFIG_HEIGHT,
                            &mode->base.height);
  b->iahwc_get_display_info(b->iahwc_device, 0, config_id,
                            IAHWC_CONFIG_REFRESHRATE, &refresh);
  mode->base.refresh = refresh;
  mode->config_id = config_id;

  wl_list_insert(output->base.mode_list.prev, &mode->base.link);

  return 0;
}

static int iahwc_subpixel_to_wayland(int iahwc_value) {
  switch (iahwc_value) {
    default:
    case DRM_MODE_SUBPIXEL_UNKNOWN:
      return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case DRM_MODE_SUBPIXEL_NONE:
      return WL_OUTPUT_SUBPIXEL_NONE;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
      return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
      return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
      return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
      return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
  }
}

/* returns a value between 0-255 range, where higher is brighter */
static uint32_t iahwc_get_backlight(struct iahwc_output *output) {
  long brightness, max_brightness, norm;

  brightness = backlight_get_brightness(output->backlight);
  max_brightness = backlight_get_max_brightness(output->backlight);

  /* convert it on a scale of 0 to 255 */
  norm = (brightness * 255) / (max_brightness);

  return (uint32_t)norm;
}

/* values accepted are between 0-255 range */
static void iahwc_set_backlight(struct weston_output *output_base,
                                uint32_t value) {
  struct iahwc_output *output = to_iahwc_output(output_base);
  long max_brightness, new_brightness;

  if (!output->backlight)
    return;

  if (value > 255)
    return;

  max_brightness = backlight_get_max_brightness(output->backlight);

  /* get denormalized value */
  new_brightness = (value * max_brightness) / 255;

  backlight_set_brightness(output->backlight, new_brightness);
}

static void iahwc_assign_planes(struct weston_output *output_base,
                                void *repaint_data) {
  struct iahwc_backend *b = to_iahwc_backend(output_base->compositor);
  struct iahwc_output *output = to_iahwc_output(output_base);
  struct weston_view *ev, *next;
  struct weston_plane *next_plane;
  uint32_t layer_index = 0;

  if (b->sprites_are_broken) {
    if (output->overlay_enabled) {
      weston_log("Disabling overlay usage \n");
      b->iahwc_disable_overlay_usage(b->iahwc_device, 0);
      output->overlay_enabled = false;
    }
  } else if (!output->overlay_enabled) {
    weston_log("Enabling overlay usage. \n");
    b->iahwc_enable_overlay_usage(b->iahwc_device, 0);
    output->overlay_enabled = true;
  }

  wl_list_for_each_safe(ev, next, &output_base->compositor->view_list, link) {
    next_plane = iahwc_output_prepare_overlay_view(output, ev, layer_index);

    if (next_plane != NULL) {
      weston_view_move_to_plane(ev, next_plane);
      layer_index++;
    } else {
      struct weston_surface *es = ev->surface;
      es->keep_buffer = false;
    }

    ev->psf_flags = WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
  }

  // Clean up our bookkeeping for unused overlays.
  if (output->total_layers > 0 && (output->total_layers > layer_index))
    iahwc_overlay_destroy(output, layer_index++);

  output->total_layers = layer_index;
  pixman_region32_clear(&output->overlay_plane.damage);
  pixman_region32_clear(&output->overlay_plane.clip);
  struct weston_compositor *c = output_base->compositor;
  pixman_region32_clear(&c->primary_plane.damage);
  pixman_region32_clear(&c->primary_plane.clip);
}

static void setup_output_seat_constraint(struct iahwc_backend *b,
                                         struct weston_output *output,
                                         const char *s) {
  if (strcmp(s, "") != 0) {
    struct weston_pointer *pointer;
    struct udev_seat *seat;

    seat = udev_seat_get_named(&b->input, s);
    if (!seat)
      return;

    seat->base.output = output;

    pointer = weston_seat_get_pointer(&seat->base);
    if (pointer)
      weston_pointer_clamp(pointer, &pointer->x, &pointer->y);
  }
}

static int parse_gbm_format(const char *s, uint32_t default_value,
                            uint32_t *gbm_format) {
  int ret = 0;

  if (s == NULL)
    *gbm_format = default_value;
  else if (strcmp(s, "xrgb8888") == 0)
    *gbm_format = GBM_FORMAT_XRGB8888;
  else if (strcmp(s, "rgb565") == 0)
    *gbm_format = GBM_FORMAT_RGB565;
  else if (strcmp(s, "xrgb2101010") == 0)
    *gbm_format = GBM_FORMAT_XRGB2101010;
  else {
    weston_log("fatal: unrecognized pixel format: %s\n", s);
    ret = -1;
  }

  return ret;
}

static void iahwc_set_dpms(struct weston_output *output_base,
                           enum dpms_enum level) {
  struct iahwc_output *output = to_iahwc_output(output_base);
  struct iahwc_backend *b = to_iahwc_backend(output_base->compositor);
  uint32_t power_level = 0;

  if (output->current_dpms == level)
    return;

  if (level == WESTON_DPMS_ON)
    weston_output_schedule_repaint(output_base);

  switch (level) {
    case WESTON_DPMS_ON:
      power_level = kOn;
      break;
    case WESTON_DPMS_STANDBY:
      power_level = kDoze;
      break;
    case WESTON_DPMS_SUSPEND:
      power_level = kDozeSuspend;
      break;
    case WESTON_DPMS_OFF:
      power_level = kOff;
      break;
  }

  b->iahwc_display_set_power_mode(b->iahwc_device, 0, power_level);
  output->current_dpms = level;
}

/**
 * Choose suitable mode for an output
 *
 * Find the most suitable mode to use for initial setup (or reconfiguration on
 * hotplug etc) for a IAHWC output.
 *
 * @param output IAHWC output to choose mode for
 * @param kind Strategy and preference to use when choosing mode
 * @param width Desired width for this output
 * @param height Desired height for this output
 * @param current_mode Mode currently being displayed on this output
 * @param modeline Manually-entered mode (may be NULL)
 * @returns A mode from the output's mode list, or NULL if none available
 */
static struct iahwc_mode *iahwc_output_choose_initial_mode(
    struct iahwc_backend *backend, struct iahwc_output *output,
    enum weston_iahwc_backend_output_mode mode, const char *modeline) {
  struct iahwc_mode *iahwc_mode;
  uint32_t active_config;

  backend->iahwc_get_display_config(backend->iahwc_device, 0, &active_config);

  wl_list_for_each_reverse(iahwc_mode, &output->base.mode_list, base.link) {
    if (iahwc_mode->config_id == active_config)
      return iahwc_mode;
  }

  weston_log("no available modes for %s\n", output->base.name);
  return NULL;
}

static int iahwc_output_set_mode(struct weston_output *base,
                                 enum weston_iahwc_backend_output_mode mode,
                                 const char *modeline) {
  struct iahwc_output *output = to_iahwc_output(base);
  struct iahwc_backend *b = to_iahwc_backend(base->compositor);
  struct weston_head *head_base;
  struct iahwc_mode *current;
  struct iahwc_head *head;
  uint32_t i;
  int ret;

  wl_list_for_each(head_base, &output->base.head_list, output_link) {
    head = to_iahwc_head(head_base);
    for (i = 0; i < head->num_configs; i++) {
      ret = iahwc_output_add_mode(b, output, head->mode_configs[i]);
      if (ret < 0)
        return -1;
    }
  }

  current = iahwc_output_choose_initial_mode(b, output, mode, modeline);
  if (!current)
    return -1;

  output->base.current_mode = &current->base;
  output->base.current_mode->flags |= WL_OUTPUT_MODE_CURRENT;

  /* Set native_ fields, so weston_output_mode_switch_to_native() works */
  output->base.native_mode = output->base.current_mode;
  output->base.native_scale = output->base.current_scale;

  return 0;
}

static void iahwc_output_set_gbm_format(struct weston_output *base,
                                        const char *gbm_format) {
  struct iahwc_output *output = to_iahwc_output(base);
  struct iahwc_backend *b = to_iahwc_backend(base->compositor);

  if (parse_gbm_format(gbm_format, b->gbm_format, &output->gbm_format) == -1)
    output->gbm_format = b->gbm_format;
}

static void iahwc_output_set_seat(struct weston_output *base,
                                  const char *seat) {
  struct iahwc_output *output = to_iahwc_output(base);
  struct iahwc_backend *b = to_iahwc_backend(base->compositor);

  setup_output_seat_constraint(b, &output->base, seat ? seat : "");
}

static int iahwc_output_enable(struct weston_output *base) {
  struct iahwc_output *output = to_iahwc_output(base);
  struct iahwc_backend *b = to_iahwc_backend(base->compositor);
  struct weston_mode *m;

  if (output->backlight) {
    weston_log("Initialized backlight, device %s\n", output->backlight->path);
    output->base.set_backlight = iahwc_set_backlight;
    output->base.backlight_current = iahwc_get_backlight(output);
  } else {
    weston_log("Failed to initialize backlight\n");
  }

  output->base.start_repaint_loop = iahwc_output_start_repaint_loop;
  output->base.repaint = iahwc_output_repaint;
  output->base.assign_planes = iahwc_assign_planes;

  output->base.set_dpms = iahwc_set_dpms;
  output->base.switch_mode = iahwc_output_switch_mode;

  output->base.set_gamma = iahwc_output_set_gamma;

  weston_plane_init(&output->overlay_plane, b->compositor, INT32_MIN,
                    INT32_MIN);

  weston_compositor_stack_plane(b->compositor, &output->overlay_plane,
                                &b->compositor->primary_plane);

  weston_log("Output %s, (connector %d, crtc %d)\n", output->base.name,
             output->connector_id, output->crtc_id);
  wl_list_for_each(m, &output->base.mode_list, link) weston_log_continue(
      STAMP_SPACE "mode %dx%d@%.1d\n", m->width, m->height, m->refresh);

  output->release_fence = -1;
  output->release_fence_source = NULL;
  lock(&output->spin_lock);
  output->state_invalid = true;
  output->last_vsync_ts.tv_nsec = 0;
  output->last_vsync_ts.tv_sec = 0;
  output->total_layers = 0;
  output->overlay_enabled = true;
  base->disable_planes = 0;
  unlock(&output->spin_lock);

  output->current_dpms = WESTON_DPMS_ON;

  return 0;
}

static void iahwc_output_deinit(struct weston_output *base) {
  struct iahwc_output *output = to_iahwc_output(base);
  weston_plane_release(&output->overlay_plane);
  lock(&output->spin_lock);
  /* Force programming unused connectors and crtcs. */
  output->state_invalid = true;
  unlock(&output->spin_lock);
}

static void iahwc_output_destroy(struct weston_output *base) {
  struct iahwc_output *output = to_iahwc_output(base);
  struct iahwc_mode *mode, *next;

  wl_list_for_each_safe(mode, next, &output->base.mode_list, base.link) {
    wl_list_remove(&mode->base.link);
    free(mode);
  }

  iahwc_overlay_destroy(output, 0);
  weston_output_release(&output->base);

  if (output->backlight)
    backlight_destroy(output->backlight);

  free(output);
}

static int iahwc_output_disable(struct weston_output *base) {
  struct iahwc_output *output = to_iahwc_output(base);

  if (output->base.enabled) {
    iahwc_output_deinit(&output->base);
  }

  weston_log("Disabling output %s\n", output->base.name);

  return 0;
}

static int pixel_uploader_callback(iahwc_callback_data_t data,
                                   iahwc_display_t display,
                                   uint32_t start_access,
                                   void *call_back_data) {
  if (start_access) {
    wl_shm_buffer_begin_access((struct wl_shm_buffer *)call_back_data);
  } else {
    wl_shm_buffer_end_access((struct wl_shm_buffer *)call_back_data);
  }

  return 0;
}

static int iahwc_output_attach_head(struct weston_output *output_base,
                                    struct weston_head *head_base) {
  if (wl_list_length(&output_base->head_list) >= MAX_CLONED_CONNECTORS)
    return -1;

  if (!output_base->enabled)
    return 0;

  weston_output_schedule_repaint(output_base);

  return 0;
}

static void iahwc_output_detach_head(struct weston_output *output_base,
                                     struct weston_head *head_base) {
  if (!output_base->enabled)
    return;

  weston_output_schedule_repaint(output_base);
}

static struct weston_output *iahwc_output_create(
    struct weston_compositor *compositor, const char *name) {
  struct iahwc_backend *b = to_iahwc_backend(compositor);
  struct iahwc_output *output;
  int ret;

  output = zalloc(sizeof *output);
  if (output == NULL)
    return NULL;

  weston_output_init(&output->base, compositor, name);

  output->base.enable = iahwc_output_enable;
  output->base.destroy = iahwc_output_destroy;
  output->base.disable = iahwc_output_disable;
  output->base.attach_head = iahwc_output_attach_head;
  output->base.detach_head = iahwc_output_detach_head;

  ret = b->iahwc_register_callback(
      b->iahwc_device, IAHWC_CALLBACK_PIXEL_UPLOADER, 0, output,
      (iahwc_function_ptr_t)pixel_uploader_callback);
  if (ret != IAHWC_ERROR_NONE) {
    weston_log("unable to register pixel uploader callback\n");
  }

  lock(&output->spin_lock);
  output->state_invalid = true;
  unlock(&output->spin_lock);

  wl_list_init(&output->overlay_list);

  weston_compositor_add_pending_output(&output->base, b->compositor);

  return &output->base;
}

static void iahwc_head_destroy(struct iahwc_head *head) {
  weston_head_release(&head->base);
  free(head->mode_configs);
  free(head);
}

static struct iahwc_head *iahwc_head_create(struct iahwc_backend *backend) {
  struct iahwc_head *head;
  char *name;
  const char *make = "unknown";
  const char *model = "unknown";
  const char *serial_number = "unknown";
  uint32_t num_configs, size;
  int mm_width;
  int mm_height;
  int connection_status;

  head = zalloc(sizeof *head);
  if (!head)
    return NULL;

  backend->iahwc_get_display_name(backend->iahwc_device, 0, &size, NULL);
  name = (char *)calloc(size + 1, sizeof(char));
  backend->iahwc_get_display_name(backend->iahwc_device, 0, &size, name);
  name[size] = '\0';

  weston_log("Name of the display is %s\n", name);

  weston_head_init(&head->base, name);
  free(name);

  head->backend = backend;
  head->mode_configs = NULL;
  head->num_configs = 0;

  backend->iahwc_get_display_configs(backend->iahwc_device, 0, &num_configs,
                                     NULL);
  head->mode_configs = (uint32_t *)calloc(num_configs, sizeof(uint32_t));
  head->num_configs = num_configs;
  backend->iahwc_get_display_configs(backend->iahwc_device, 0, &num_configs,
                                     head->mode_configs);

  backend->iahwc_get_display_info(backend->iahwc_device, 0,
                                  head->mode_configs[0], IAHWC_CONFIG_DPIX,
                                  &mm_width);
  backend->iahwc_get_display_info(backend->iahwc_device, 0,
                                  head->mode_configs[0], IAHWC_CONFIG_DPIY,
                                  &mm_height);

  // XXX:TODO: get these details from iahwc
  weston_head_set_monitor_strings(&head->base, make, model, serial_number);
  weston_head_set_subpixel(&head->base, WL_OUTPUT_SUBPIXEL_UNKNOWN);

  weston_head_set_physical_size(&head->base, mm_width, mm_height);

  backend->iahwc_display_get_connection_status(backend->iahwc_device, 0,
                                               &connection_status);
  weston_head_set_connection_status(&head->base, connection_status);

  // XXX:TODO: check if the connector is internal or external?

  weston_compositor_add_head(backend->compositor, &head->base);
  /* iahwc_head_log_info(head, "found"); */

  return head;
}

static int iahwc_create_heads(struct iahwc_backend *b) {
  struct iahwc_head *head;
  int num_displays;
  int i;

  b->iahwc_get_num_displays(b->iahwc_device, &num_displays);

  if (num_displays < 1) {
    weston_log("Unable to find any connected displays");
    return -1;
  }

  for (i = 0; i < num_displays; i++) {
    head = iahwc_head_create(b);
    if (!head) {
      weston_log("IAHWC: failed to create head for display %d.\n", i);
    }
  }

  return 0;
}

static void iahwc_destroy(struct weston_compositor *ec) {
  struct iahwc_backend *b = to_iahwc_backend(ec);
  struct weston_head *base, *next;

  udev_input_destroy(&b->input);

  wl_event_source_remove(b->udev_iahwc_source);
  wl_event_source_remove(b->iahwc_source);

  wl_list_for_each_safe(base, next, &ec->head_list, compositor_link)
      iahwc_head_destroy(to_iahwc_head(base));

  weston_compositor_shutdown(ec);

  if (b->gbm)
    gbm_device_destroy(b->gbm);

  udev_unref(b->udev);

  weston_launcher_destroy(ec->launcher);

  b->iahwc_device->close(b->iahwc_device);

  free(b);
}

static void session_notify(struct wl_listener *listener, void *data) {
  struct weston_compositor *compositor = data;
  struct iahwc_backend *b = to_iahwc_backend(compositor);
  struct iahwc_output *output;

  if (compositor->session_active) {
    weston_log("activating session\n");
    weston_compositor_wake(compositor);
    weston_compositor_damage_all(compositor);

    wl_list_for_each(output, &compositor->output_list, base.link) {
      lock(&output->spin_lock);
      output->state_invalid = true;
      unlock(&output->spin_lock);
    }

    udev_input_enable(&b->input);
  } else {
    weston_log("deactivating session\n");
    udev_input_disable(&b->input);

    weston_compositor_offscreen(compositor);
  }
}

static void planes_binding(struct weston_keyboard *keyboard,
                           const struct timespec *time, uint32_t key,
                           void *data) {
  struct iahwc_backend *b = data;

  switch (key) {
    case KEY_V:
    case KEY_C:
      b->sprites_are_broken = 1;
      break;
    case KEY_O:
      // FIXME: Drmdisplay should not commit overlays in this case.
      b->sprites_hidden = 1;
      break;
    default:
      break;
  }
}

static const struct weston_iahwc_output_api api = {
    iahwc_output_set_mode, iahwc_output_set_gbm_format, iahwc_output_set_seat,
};

static struct iahwc_backend *iahwc_backend_create(
    struct weston_compositor *compositor,
    struct weston_iahwc_backend_config *config) {
  struct iahwc_backend *b;
  void *iahwc_dl_handle;
  iahwc_module_t *iahwc_module;
  iahwc_device_t *iahwc_device;

  const char *device = "/dev/dri/renderD128";
  const char *seat_id = default_seat;

  weston_log("Initializing iahwc backend\n");

  b = zalloc(sizeof *b);
  if (b == NULL)
    return NULL;

  b->compositor = compositor;
  compositor->backend = &b->base;
  compositor->capabilities |= WESTON_CAP_CURSOR_PLANE;

  iahwc_dl_handle = dlopen("libhwcomposer.so", RTLD_NOW);
  if (!iahwc_dl_handle) {
    weston_log("Unable to open libhwcomposer.so: %s\n", dlerror());
    weston_log("aborting...\n");
    abort();
  }

  iahwc_module = (iahwc_module_t *)dlsym(iahwc_dl_handle, IAHWC_MODULE_STR);
  iahwc_module->open(iahwc_module, &iahwc_device);

  b->iahwc_module = iahwc_module;
  b->iahwc_device = iahwc_device;

  b->iahwc_get_num_displays =
      (IAHWC_PFN_GET_NUM_DISPLAYS)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_GET_NUM_DISPLAYS);
  b->iahwc_create_layer = (IAHWC_PFN_CREATE_LAYER)iahwc_device->getFunctionPtr(
      iahwc_device, IAHWC_FUNC_CREATE_LAYER);
  b->iahwc_destroy_layer =
      (IAHWC_PFN_DESTROY_LAYER)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DESTROY_LAYER);
  b->iahwc_display_get_connection_status =
      (IAHWC_PFN_DISPLAY_GET_CONNECTION_STATUS)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_CONNECTION_STATUS);
  b->iahwc_get_display_info =
      (IAHWC_PFN_DISPLAY_GET_INFO)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_INFO);
  b->iahwc_get_display_configs =
      (IAHWC_PFN_DISPLAY_GET_CONFIGS)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_CONFIGS);
  b->iahwc_get_display_name =
      (IAHWC_PFN_DISPLAY_GET_NAME)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_NAME);
  b->iahwc_set_display_gamma =
      (IAHWC_PFN_DISPLAY_SET_GAMMA)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_SET_GAMMA);
  b->iahwc_set_display_config =
      (IAHWC_PFN_DISPLAY_SET_CONFIG)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_SET_CONFIG);
  b->iahwc_get_display_config =
      (IAHWC_PFN_DISPLAY_GET_CONFIG)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_CONFIG);
  b->iahwc_display_set_power_mode =
      (IAHWC_PFN_DISPLAY_SET_POWER_MODE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_SET_POWER_MODE);
  b->iahwc_display_clear_all_layers =
      (IAHWC_PFN_DISPLAY_CLEAR_ALL_LAYERS)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_CLEAR_ALL_LAYERS);
  b->iahwc_present_display =
      (IAHWC_PFN_PRESENT_DISPLAY)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_PRESENT_DISPLAY);
  b->iahwc_disable_overlay_usage =
      (IAHWC_PFN_DISABLE_OVERLAY_USAGE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISABLE_OVERLAY_USAGE);
  b->iahwc_enable_overlay_usage =
      (IAHWC_PFN_ENABLE_OVERLAY_USAGE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_ENABLE_OVERLAY_USAGE);
  b->iahwc_layer_set_bo = (IAHWC_PFN_LAYER_SET_BO)iahwc_device->getFunctionPtr(
      iahwc_device, IAHWC_FUNC_LAYER_SET_BO);
  b->iahwc_layer_set_raw_pixel_data =
      (IAHWC_PFN_LAYER_SET_RAW_PIXEL_DATA)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_RAW_PIXEL_DATA);
  b->iahwc_layer_set_acquire_fence =
      (IAHWC_PFN_LAYER_SET_ACQUIRE_FENCE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_ACQUIRE_FENCE);
  b->iahwc_layer_set_source_crop =
      (IAHWC_PFN_LAYER_SET_SOURCE_CROP)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_SOURCE_CROP);
  b->iahwc_layer_set_display_frame =
      (IAHWC_PFN_LAYER_SET_DISPLAY_FRAME)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_DISPLAY_FRAME);
  b->iahwc_layer_set_surface_damage =
      (IAHWC_PFN_LAYER_SET_SURFACE_DAMAGE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_SURFACE_DAMAGE);
  b->iahwc_layer_set_plane_alpha =
      (IAHWC_PFN_LAYER_SET_PLANE_ALPHA)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_PLANE_ALPHA);
  b->iahwc_layer_set_usage =
      (IAHWC_PFN_LAYER_SET_USAGE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_USAGE);
  b->iahwc_layer_set_index =
      (IAHWC_PFN_LAYER_SET_INDEX)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_INDEX);
  b->iahwc_register_callback =
      (IAHWC_PFN_REGISTER_CALLBACK)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_REGISTER_CALLBACK);

  if (parse_gbm_format(config->gbm_format, GBM_FORMAT_XRGB8888,
                       &b->gbm_format) < 0)
    goto err_compositor;

  // Check if we are connected with weston-launch
  compositor->launcher =
      weston_launcher_connect(compositor, config->tty, seat_id, true);
  if (compositor->launcher == NULL) {
    weston_log(
        "fatal: drm backend should be run "
        "using weston-launch binary or as root\n");
    goto err_compositor;
  }

  b->iahwc.fd = open(device, O_RDWR);
  if (b->iahwc.fd < 0) {
    weston_log("unable to open gpu file\n");
    goto err_compositor;
  }

  b->udev = udev_new();
  if (b->udev == NULL) {
    weston_log("failed to initialize udev context\n");
    goto err_compositor;
  }

  if (config->seat_id)
    seat_id = config->seat_id;

  // session_notification XXX?TODO: make necessary changes
  b->session_listener.notify = session_notify;
  wl_signal_add(&compositor->session_signal, &b->session_listener);

  if (init_egl(b) < 0) {
    weston_log("failed to initialize egl\n");
    goto err_compositor;
  }

  b->cursor_width = 256;
  b->cursor_height = 256;
  b->sprites_are_broken = 0;
  b->sprites_hidden = 0;

  b->base.destroy = iahwc_destroy;
  b->base.repaint_begin = iahwc_repaint_begin;
  b->base.repaint_flush = iahwc_repaint_flush;
  b->base.repaint_cancel = iahwc_repaint_cancel;
  b->base.create_output = iahwc_output_create;

  if (udev_input_init(&b->input, compositor, b->udev, seat_id,
                      config->configure_device) < 0) {
    weston_log("failed to create input devices\n");
    goto err_compositor;
  }

  if (iahwc_create_heads(b) < 0) {
    weston_log("Failed to create heads. No devices connected?");
    goto err_compositor;
  }

  // XXX/TODO: setup hotplugging support from IAHWC
  // Nothing for now, registering the callback enables the pixel upload support
  b->iahwc_register_callback(b->iahwc_device, IAHWC_CALLBACK_HOTPLUG, NULL,
                             NULL, NULL);

  weston_setup_vt_switch_bindings(compositor);

  weston_compositor_add_debug_binding(compositor, KEY_O, planes_binding, b);
  weston_compositor_add_debug_binding(compositor, KEY_C, planes_binding, b);
  weston_compositor_add_debug_binding(compositor, KEY_V, planes_binding, b);
  /* weston_compositor_add_debug_binding(compositor, KEY_Q, */
  /*                                     recorder_binding, b); */

  if (linux_dmabuf_setup(compositor) < 0)
    weston_log(
        "Error: initializing dmabuf "
        "support failed.\n");

  int ret = weston_plugin_api_register(compositor, WESTON_IAHWC_OUTPUT_API_NAME,
                                       &api, sizeof(api));

  if (ret)
    goto err_compositor;

  return b;

err_compositor:
  weston_compositor_shutdown(compositor);
  free(b);
  return NULL;
}

static void config_init_to_defaults(
    struct weston_iahwc_backend_config *config) {
}

WL_EXPORT int weston_backend_init(struct weston_compositor *compositor,
                                  struct weston_backend_config *config_base) {
  struct iahwc_backend *b;
  struct weston_iahwc_backend_config config = {{
      0,
  }};

  if (config_base == NULL ||
      config_base->struct_version != WESTON_IAHWC_BACKEND_CONFIG_VERSION ||
      config_base->struct_size > sizeof(struct weston_iahwc_backend_config)) {
    weston_log("iahwc backend config structure is invalid\n");
    return -1;
  }

  config_init_to_defaults(&config);
  memcpy(&config, config_base, config_base->struct_size);
  b = iahwc_backend_create(compositor, &config);
  if (b == NULL)
    return -1;

  return 0;
}
