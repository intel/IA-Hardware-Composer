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

#include <fcntl.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <pthread.h>
#include <queue>

#include <cutils/log.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <sync/sync.h>

#include "drm_hwcomposer.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr) / sizeof((arr)[0]))

#define HWCOMPOSER_DRM_DEVICE "/dev/dri/card0"
#define MAX_NUM_DISPLAYS 3
#define UM_PER_INCH 25400

static const uint32_t panel_types[] = {
	DRM_MODE_CONNECTOR_LVDS,
	DRM_MODE_CONNECTOR_eDP,
	DRM_MODE_CONNECTOR_DSI,
};

struct hwc_worker {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	bool exit;
};

struct hwc_drm_display {
	struct hwc_context_t *ctx;
	int display;

	uint32_t connector_id;

	drmModeModeInfoPtr configs;
	uint32_t num_configs;

	int active_config;
	uint32_t active_crtc;

	struct hwc_worker set_worker;

	std::queue<struct hwc_drm_bo> buf_queue;
	struct hwc_drm_bo front;
};

struct hwc_context_t {
	hwc_composer_device_1_t device;

	int fd;

	hwc_procs_t const *procs;
	struct hwc_import_context *import_ctx;

	struct hwc_drm_display displays[MAX_NUM_DISPLAYS];
	int num_displays;
};

static int hwc_get_drm_display(struct hwc_context_t *ctx, int display,
			struct hwc_drm_display **hd)
{
	if (display >= MAX_NUM_DISPLAYS) {
		ALOGE("Requested display is out-of-bounds %d %d", display,
			MAX_NUM_DISPLAYS);
		return -EINVAL;
	}
	*hd = &ctx->displays[display];
	return 0;
}

static int hwc_prepare_layer(hwc_layer_1_t *layer)
{
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

static int hwc_prepare(hwc_composer_device_1_t */* dev */, size_t num_displays,
			hwc_display_contents_1_t** display_contents)
{
	int ret = 0, i, j;

	/* TODO: Check flags for HWC_GEOMETRY_CHANGED */

	for (i = 0; i < (int)num_displays && i < MAX_NUM_DISPLAYS; i++) {
		for (j = 0; j < (int)display_contents[i]->numHwLayers; j++) {
			ret = hwc_prepare_layer(
					&display_contents[i]->hwLayers[j]);
			if (ret) {
				ALOGE("Failed to prepare layer %d:%d", j, i);
				return ret;
			}
		}
	}

	return ret;
}

/*
 * TODO: This hack allows us to use the importer's fd to drm to add and remove
 * framebuffers. The reason it exists is because gralloc doesn't export its
 * bo's, so we have to use its file descriptor to drm for some operations. Once
 * gralloc behaves, we can remove this.
 */
static int hwc_get_fd_for_bo(struct hwc_context_t *ctx, struct hwc_drm_bo *bo)
{
	if (bo->importer_fd >= 0)
		return bo->importer_fd;

	return ctx->fd;
}

static bool hwc_mode_is_equal(drmModeModeInfoPtr a, drmModeModeInfoPtr b)
{
	return a->clock == b->clock &&
		a->hdisplay == b->hdisplay &&
		a->hsync_start == b->hsync_start &&
		a->hsync_end == b->hsync_end &&
		a->htotal == b->htotal &&
		a->hskew == b->hskew &&
		a->vdisplay == b->vdisplay &&
		a->vsync_start == b->vsync_start &&
		a->vsync_end == b->vsync_end &&
		a->vtotal == b->vtotal &&
		a->vscan == b->vscan &&
		a->vrefresh == b->vrefresh &&
		a->flags == b->flags &&
		a->type == b->type &&
		!strcmp(a->name, b->name);
}

static int hwc_modeset_required(struct hwc_drm_display *hd,
			bool *modeset_required)
{
	drmModeCrtcPtr crtc;
	drmModeModeInfoPtr m;

	crtc = drmModeGetCrtc(hd->ctx->fd, hd->active_crtc);
	if (!crtc) {
		ALOGE("Failed to get crtc for display %d", hd->display);
		return -ENODEV;
	}

	m = &hd->configs[hd->active_config];

	/* Do a modeset if we haven't done one, or the mode has changed */
	if (!crtc->mode_valid || !hwc_mode_is_equal(m, &crtc->mode))
		*modeset_required = true;
	else
		*modeset_required = false;

	drmModeFreeCrtc(crtc);

	return 0;
}

static void hwc_flip_handler(int /* fd */, unsigned int /* sequence */,
		unsigned int /* tv_sec */, unsigned int /* tv_usec */,
		void */* user_data */)
{
}

static int hwc_flip(struct hwc_drm_display *hd, struct hwc_drm_bo *buf)
{
	fd_set fds;
	drmEventContext event_context;
	int ret;
	bool modeset_required;

	ret = hwc_modeset_required(hd, &modeset_required);
	if (ret) {
		ALOGE("Failed to determine if modeset is required %d", ret);
		return ret;
	}
	if (modeset_required) {
		ret = drmModeSetCrtc(hd->ctx->fd, hd->active_crtc, buf->fb_id,
			0, 0, &hd->connector_id, 1,
			&hd->configs[hd->active_config]);
		if (ret) {
			ALOGE("Modeset failed for crtc %d",
				hd->active_crtc);
			return ret;
		}
		return 0;
	}

	FD_ZERO(&fds);
	FD_SET(hd->ctx->fd, &fds);

	event_context.version = DRM_EVENT_CONTEXT_VERSION;
	event_context.page_flip_handler = hwc_flip_handler;

	ret = drmModePageFlip(hd->ctx->fd, hd->active_crtc, buf->fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, hd);
	if (ret) {
		ALOGE("Failed to flip buffer for crtc %d",
			hd->active_crtc);
		return ret;
	}

	do {
		ret = select(hd->ctx->fd + 1, &fds, NULL, NULL, NULL);
	} while (ret == -1 && errno == EINTR);

	if (ret != 1) {
		ALOGE("Failed waiting for flip to complete\n");
		return -EINVAL;
	}
	drmHandleEvent(hd->ctx->fd, &event_context);

	return 0;
}

static int hwc_wait_and_set(struct hwc_drm_display *hd)
{
	int ret;
	struct hwc_drm_bo buf = hd->buf_queue.front();

	ret = drmModeAddFB2(hwc_get_fd_for_bo(hd->ctx, &buf), buf.width,
		buf.height, buf.format, buf.gem_handles, buf.pitches,
		buf.offsets, &buf.fb_id, 0);
	if (ret) {
		ALOGE("could not create drm fb %d", ret);
		return ret;
	}

	if (buf.acquire_fence_fd >= 0) {
		ret = sync_wait(buf.acquire_fence_fd, -1);
		if (ret) {
			ALOGE("Failed to wait for acquire %d", ret);
			return ret;
		}
	}

	ret = hwc_flip(hd, &buf);
	if (ret) {
		ALOGE("Failed to perform flip\n");
		return ret;
	}

	if (hd->front.fb_id) {
		ret = drmModeRmFB(hwc_get_fd_for_bo(hd->ctx, &hd->front),
				hd->front.fb_id);
		if (ret) {
			ALOGE("Failed to rm fb from front %d", ret);
			return ret;
		}
	}
	hd->front = buf;
	hd->buf_queue.pop();
	return ret;
}

static void *hwc_set_worker(void *arg)
{
	struct hwc_drm_display *hd = (struct hwc_drm_display *)arg;
	int ret;

	setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

	ret = pthread_mutex_lock(&hd->set_worker.lock);
	if (ret) {
		ALOGE("Failed to lock set lock %d", ret);
		return NULL;
	}

	do {
		ret = pthread_cond_wait(&hd->set_worker.cond,
				&hd->set_worker.lock);
		if (ret) {
			ALOGE("Failed to wait on set condition %d", ret);
			break;
		} else if (hd->set_worker.exit) {
			break;
		}

		ret = hwc_wait_and_set(hd);
		if (ret)
			ALOGE("Failed to wait and set %d", ret);
	} while (true);

	ret = pthread_mutex_unlock(&hd->set_worker.lock);
	if (ret) {
		ALOGE("Failed to unlock set lock %d", ret);
		return NULL;
	}

	return NULL;
}

static int hwc_set_display(hwc_context_t *ctx, int display,
			hwc_display_contents_1_t* display_contents)
{
	struct hwc_drm_display *hd = NULL;
	hwc_layer_1_t *layer = NULL;
	struct hwc_drm_bo buf;
	int ret, i;
	uint32_t fb_id;

	memset(&buf, 0, sizeof(buf));

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	if (!hd->active_crtc) {
		ALOGE("There is no active crtc for display %d", display);
		return -ENOENT;
	}

	/*
	 * TODO: We can only support one hw layer atm, so choose either the
	 * first one or the framebuffer target.
	 */
	if (!display_contents->numHwLayers) {
		return 0;
	} else if (display_contents->numHwLayers == 1) {
		layer = &display_contents->hwLayers[0];
	} else {
		for (i = 0; i < (int)display_contents->numHwLayers; i++) {
			layer = &display_contents->hwLayers[i];
			if (layer->compositionType == HWC_FRAMEBUFFER_TARGET)
				break;
		}
		if (i == (int)display_contents->numHwLayers) {
			ALOGE("Could not find a suitable layer for display %d",
				display);
		}
	}

	ret = pthread_mutex_lock(&hd->set_worker.lock);
	if (ret) {
		ALOGE("Failed to lock set lock in set() %d", ret);
		return ret;
	}

	ret = hwc_create_bo_from_import(ctx->fd, ctx->import_ctx, layer->handle,
				&buf);
	if (ret) {
		ALOGE("Failed to import handle to drm bo %d", ret);
		goto out;
	}
	buf.acquire_fence_fd = layer->acquireFenceFd;
	layer->releaseFenceFd = -1;

	hd->buf_queue.push(buf);

	ret = pthread_cond_signal(&hd->set_worker.cond);
	if (ret) {
		ALOGE("Failed to signal set worker %d", ret);
		goto out;
	}

	ret = pthread_mutex_unlock(&hd->set_worker.lock);
	if (ret) {
		ALOGE("Failed to unlock set lock in set() %d", ret);
		return ret;
	}

	return ret;

out:
	if (pthread_mutex_unlock(&hd->set_worker.lock))
		ALOGE("Failed to unlock set lock in set error handler");

	return ret;
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays,
			hwc_display_contents_1_t** display_contents)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	int ret = 0, i;

	/* TODO: Handle acquire & release fences */

	for (i = 0; i < (int)num_displays && i < MAX_NUM_DISPLAYS; i++) {
		display_contents[i]->retireFenceFd = -1; /* TODO: sync */

		ret = hwc_set_display(ctx, i, display_contents[i]);
	}

	return ret;
}

static int hwc_event_control(struct hwc_composer_device_1 */* dev */,
			int /* display */, int /* event */, int /* enabled */)
{
	int ret;

	/* TODO */
	return 0;
}

static int hwc_set_power_mode(struct hwc_composer_device_1* dev, int display,
			int mode)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	struct hwc_drm_display *hd = NULL;
	drmModeConnectorPtr c;
	int ret, i;
	uint32_t dpms_prop = 0;
	uint64_t dpms_value = 0;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	c = drmModeGetConnector(ctx->fd, hd->connector_id);
	if (!c) {
		ALOGE("Failed to get connector %d", display);
		return -ENODEV;
	}

	for (i = 0; !dpms_prop && i < c->count_props; i++) {
		drmModePropertyPtr p;

		p = drmModeGetProperty(ctx->fd, c->props[i]);
		if (!p)
			continue;

		if (!strcmp(p->name, "DPMS"))
			dpms_prop = c->props[i];

		drmModeFreeProperty(p);
	}
	if (!dpms_prop) {
		ALOGE("Failed to get DPMS property from display %d", display);
		ret = -ENOENT;
		goto out;
	}

	switch(mode) {
	case HWC_POWER_MODE_OFF:
		dpms_value = DRM_MODE_DPMS_OFF;
		break;

	/* We can't support dozing right now, so go full on */
	case HWC_POWER_MODE_DOZE:
	case HWC_POWER_MODE_DOZE_SUSPEND:
	case HWC_POWER_MODE_NORMAL:
		dpms_value = DRM_MODE_DPMS_ON;
		break;
	};

	ret = drmModeConnectorSetProperty(ctx->fd, c->connector_id,
			dpms_prop, dpms_value);
	if (ret) {
		ALOGE("Failed to set DPMS property for display %d", display);
		goto out;
	}

out:
	drmModeFreeConnector(c);
	return ret;
}

static int hwc_query(struct hwc_composer_device_1 */* dev */, int what,
			int *value)
{
	switch(what) {
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

static void hwc_register_procs(struct hwc_composer_device_1* dev,
			hwc_procs_t const* procs)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

	ctx->procs = procs;
}

static int hwc_get_display_configs(struct hwc_composer_device_1* dev,
			int display, uint32_t* configs, size_t* numConfigs)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	struct hwc_drm_display *hd = NULL;
	drmModeConnectorPtr c;
	int ret = 0, i;

	if (!*numConfigs)
		return 0;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	c = drmModeGetConnector(ctx->fd, hd->connector_id);
	if (!c) {
		ALOGE("Failed to get connector %d", display);
		return -ENODEV;
	}

	if (hd->configs)
		free(hd->configs);

	hd->active_config = -1;
	hd->configs = (drmModeModeInfoPtr)calloc(c->count_modes,
					sizeof(*hd->configs));
	if (!hd->configs) {
		ALOGE("Failed to allocate config list for display %d", display);
		ret = -ENOMEM;
		hd->num_configs = 0;
		goto out;
	}

	for (i = 0; i < c->count_modes; i++) {
		drmModeModeInfoPtr m = &hd->configs[i];

		memcpy(m, &c->modes[i], sizeof(*m));

		if (i < (int)*numConfigs)
			configs[i] = i;
	}

	hd->num_configs = c->count_modes;
	*numConfigs = MIN(c->count_modes, *numConfigs);

out:
	drmModeFreeConnector(c);
	return ret;
}

static int hwc_check_config_valid(struct hwc_context_t *ctx,
			drmModeConnectorPtr connector, int display,
			int config_idx)
{
	struct hwc_drm_display *hd = NULL;
	drmModeModeInfoPtr m = NULL;
	int ret = 0, i;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	/* Make sure the requested config is still valid for the display */
	for (i = 0; i < connector->count_modes; i++) {
		if (hwc_mode_is_equal(&connector->modes[i],
				&hd->configs[config_idx])) {
			m = &hd->configs[config_idx];
			break;
		}
	}
	if (!m)
		return -ENOENT;

	return 0;
}

static int hwc_get_display_attributes(struct hwc_composer_device_1* dev,
		int display, uint32_t config, const uint32_t* attributes,
		int32_t* values)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	struct hwc_drm_display *hd = NULL;
	drmModeConnectorPtr c;
	drmModeModeInfoPtr m;
	int ret, i;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	if (config >= hd->num_configs) {
		ALOGE("Requested config is out-of-bounds %d %d", config,
			hd->num_configs);
		return -EINVAL;
	}

	c = drmModeGetConnector(ctx->fd, hd->connector_id);
	if (!c) {
		ALOGE("Failed to get connector %d", display);
		return -ENODEV;
	}

	ret = hwc_check_config_valid(ctx, c, display, (int)config);
	if (ret) {
		ALOGE("Provided config is no longer valid %u", config);
		goto out;
	}

	m = &hd->configs[config];
	for (i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
		switch(attributes[i]) {
		case HWC_DISPLAY_VSYNC_PERIOD:
			values[i] = 1000 * 1000 * 1000 / m->vrefresh;
			break;
		case HWC_DISPLAY_WIDTH:
			values[i] = m->hdisplay;
			break;
		case HWC_DISPLAY_HEIGHT:
			values[i] = m->vdisplay;
			break;
		case HWC_DISPLAY_DPI_X:
			/* Dots per 1000 inches */
			values[i] = c->mmWidth ?
				(m->hdisplay * UM_PER_INCH) / c->mmWidth : 0;
			break;
		case HWC_DISPLAY_DPI_Y:
			/* Dots per 1000 inches */
			values[i] = c->mmHeight ?
				(m->vdisplay * UM_PER_INCH) / c->mmHeight : 0;
			break;
		}
	}

out:
	drmModeFreeConnector(c);
	return ret;
}

static int hwc_get_active_config(struct hwc_composer_device_1* dev, int display)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	struct hwc_drm_display *hd = NULL;
	drmModeConnectorPtr c;
	int ret;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	if (hd->active_config < 0)
		return -1;

	c = drmModeGetConnector(ctx->fd, hd->connector_id);
	if (!c) {
		ALOGE("Failed to get connector %d", display);
		return -ENODEV;
	}

	ret = hwc_check_config_valid(ctx, c, display, hd->active_config);
	if (ret) {
		ALOGE("Config is no longer valid %d", hd->active_config);
		ret = -1;
		goto out;
	}

	ret = hd->active_config;

out:
	drmModeFreeConnector(c);
	return ret;
}

static bool hwc_crtc_is_bound(struct hwc_context_t *ctx, uint32_t crtc_id)
{
	int i;

	for (i = 0; i < MAX_NUM_DISPLAYS; i++) {
		if (ctx->displays[i].active_crtc == crtc_id)
			return true;
	}
	return false;
}

static int hwc_try_encoder(struct hwc_context_t *ctx, drmModeResPtr r,
			uint32_t encoder_id, uint32_t *crtc_id)
{
	drmModeEncoderPtr e;
	int ret, i;

	e = drmModeGetEncoder(ctx->fd, encoder_id);
	if (!e) {
		ALOGE("Failed to get encoder for connector %d", encoder_id);
		return -ENODEV;
	}

	/* First try to use the currently-bound crtc */
	if (e->crtc_id) {
		if (!hwc_crtc_is_bound(ctx, e->crtc_id)) {
			*crtc_id = e->crtc_id;
			ret = 0;
			goto out;
		}
	}

	/* Try to find a possible crtc which will work */
	for (i = 0; i < r->count_crtcs; i++) {
		if (!(e->possible_crtcs & (1 << i)))
			continue;

		/* We've already tried this earlier */
		if (e->crtc_id == r->crtcs[i])
			continue;

		if (!hwc_crtc_is_bound(ctx, r->crtcs[i])) {
			*crtc_id = r->crtcs[i];
			ret = 0;
			goto out;
		}
	}

	/* We can't use the encoder, but nothing went wrong, try another one */
	ret = -EAGAIN;

out:
	drmModeFreeEncoder(e);
	return ret;
}

static int hwc_set_active_config(struct hwc_composer_device_1* dev, int display,
			int index)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	struct hwc_drm_display *hd = NULL;
	drmModeResPtr r = NULL;
	drmModeConnectorPtr c;
	uint32_t crtc_id = 0;
	int ret, i;
	bool new_crtc, new_encoder;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	c = drmModeGetConnector(ctx->fd, hd->connector_id);
	if (!c) {
		ALOGE("Failed to get connector %d", display);
		return -ENODEV;
	}

	if (c->connection == DRM_MODE_DISCONNECTED) {
		ALOGE("Tried to configure a disconnected display %d", display);
		ret = -ENODEV;
		goto out;
	}

	ret = hwc_check_config_valid(ctx, c, display, index);
	if (ret) {
		ALOGE("Provided config is no longer valid %u", index);
		ret = -ENOENT;
		goto out;
	}

	r = drmModeGetResources(ctx->fd);
	if (!r) {
		ALOGE("Failed to get drm resources");
		goto out;
	}

	/* We no longer have an active_crtc */
	hd->active_crtc = 0;

	/* First, try to use the currently-connected encoder */
	if (c->encoder_id) {
		ret = hwc_try_encoder(ctx, r, c->encoder_id, &crtc_id);
		if (ret && ret != -EAGAIN) {
			ALOGE("Encoder try failed %d", ret);
			goto out;
		}
	}

	/* We couldn't find a crtc with the attached encoder, try the others */
	if (!crtc_id) {
		for (i = 0; i < c->count_encoders; i++) {
			ret = hwc_try_encoder(ctx, r, c->encoders[i], &crtc_id);
			if (!ret) {
				break;
			} else if (ret != -EAGAIN) {
				ALOGE("Encoder try failed %d", ret);
				goto out;
			}
		}
		if (!crtc_id) {
			ALOGE("Couldn't find valid crtc to modeset");
			ret = -EINVAL;
			goto out;
		}
	}

	hd->active_crtc = crtc_id;
	hd->active_config = index;

	/* TODO: Once we have atomic, set the crtc timing info here */

out:
	if (r)
		drmModeFreeResources(r);

	drmModeFreeConnector(c);
	return ret;
}

static int hwc_destroy_worker(struct hwc_worker *worker)
{
	int ret;

	ret = pthread_mutex_lock(&worker->lock);
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

static void hwc_destroy_display(struct hwc_drm_display *hd)
{
	int ret;

	if (hwc_destroy_worker(&hd->set_worker))
		ALOGE("Destroy set worker failed");
}

static int hwc_device_close(struct hw_device_t *dev)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
	int ret, i;

	for (i = 0; i < MAX_NUM_DISPLAYS; i++)
		hwc_destroy_display(&ctx->displays[i]);

	drmClose(ctx->fd);

	ret = hwc_import_destroy(ctx->import_ctx);
	if (ret)
		ALOGE("Could not destroy import %d", ret);

	free(ctx);

	return 0;
}

static int hwc_initialize_worker(struct hwc_drm_display *hd,
			struct hwc_worker *worker, void *(*routine)(void*))
{
	int ret;

	ret = pthread_cond_init(&worker->cond, NULL);
	if (ret) {
		ALOGE("Failed to create worker condition %d", ret);
		return ret;
	}

	ret = pthread_mutex_init(&worker->lock, NULL);
	if (ret) {
		ALOGE("Failed to initialize worker lock %d", ret);
		goto err_cond;
	}

	worker->exit = false;

	ret = pthread_create(&worker->thread, NULL, routine, hd);
	if (ret) {
		ALOGE("Could not create worker thread %d", ret);
		goto err_lock;
	}
	return 0;

err_lock:
	pthread_mutex_destroy(&worker->lock);
err_cond:
	pthread_cond_destroy(&worker->cond);
	return ret;
}

static int hwc_initialize_display(struct hwc_context_t *ctx, int display,
			uint32_t connector_id)
{
	struct hwc_drm_display *hd = NULL;
	int ret;

	ret = hwc_get_drm_display(ctx, display, &hd);
	if (ret)
		return ret;

	hd->ctx = ctx;
	hd->display = display;
	hd->active_config = -1;
	hd->connector_id = connector_id;

	ret = hwc_initialize_worker(hd, &hd->set_worker, hwc_set_worker);
	if (ret) {
		ALOGE("Failed to create set worker %d\n", ret);
		return ret;
	}

	return 0;
}

static int hwc_enumerate_displays(struct hwc_context_t *ctx)
{
	struct hwc_drm_display *panel_hd;
	drmModeResPtr res;
	drmModeConnectorPtr *conn_list;
	int ret = 0, i, j;

	res = drmModeGetResources(ctx->fd);
	if (!res) {
		ALOGE("Failed to get drm resources");
		return -ENODEV;
	}

	conn_list = (drmModeConnector **)calloc(res->count_connectors,
			sizeof(*conn_list));
	if (!conn_list) {
		ALOGE("Failed to allocate connector list");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < res->count_connectors; i++) {
		conn_list[i] = drmModeGetConnector(ctx->fd, res->connectors[i]);
		if (!conn_list[i]) {
			ALOGE("Failed to get connector %d", res->connectors[i]);
			ret = -ENODEV;
			goto out;
		}
	}

	ctx->num_displays = 0;

	/* Find a connected, panel type connector for display 0 */
	for (i = 0; i < res->count_connectors; i++) {
		drmModeConnectorPtr c = conn_list[i];

		for (j = 0; j < ARRAY_SIZE(panel_types); j++) {
			if (c->connector_type == panel_types[j] &&
			    c->connection == DRM_MODE_CONNECTED)
				break;
		}
		if (j == ARRAY_SIZE(panel_types))
			continue;

		hwc_initialize_display(ctx, ctx->num_displays, c->connector_id);
		ctx->num_displays++;
		break;
	}

	ret = hwc_get_drm_display(ctx, 0, &panel_hd);
	if (ret)
		goto out;

	/* Fill in the other displays */
	for (i = 0; i < res->count_connectors; i++) {
		drmModeConnectorPtr c = conn_list[i];

		if (panel_hd->connector_id == c->connector_id)
			continue;

		hwc_initialize_display(ctx, ctx->num_displays, c->connector_id);
		ctx->num_displays++;
	}

out:
	for (i = 0; i < res->count_connectors; i++) {
		if (conn_list[i])
			drmModeFreeConnector(conn_list[i]);
	}
	free(conn_list);

	if (res)
		drmModeFreeResources(res);

	return ret;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
			struct hw_device_t** dev)
{
	int ret = 0;
	struct hwc_context_t *ctx;

	if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
		ALOGE("Invalid module name- %s", name);
		return -EINVAL;
	}

	ctx = new hwc_context_t();
	if (!ctx) {
		ALOGE("Failed to allocate hwc context");
		return -ENOMEM;
	}

	ret = hwc_import_init(&ctx->import_ctx);
	if (ret) {
		ALOGE("Failed to initialize import context");
		goto out;
	}

	/* TODO: Use drmOpenControl here instead */
	ctx->fd = open(HWCOMPOSER_DRM_DEVICE, O_RDWR);
	if (ctx->fd < 0) {
		ALOGE("Failed to open dri- %s", strerror(-errno));
		goto out;
	}

	ret = drmSetMaster(ctx->fd);
	if (ret) {
		ALOGE("Failed to set hwcomposer as drm master %d", ret);
		goto out;
	}

	ret = hwc_enumerate_displays(ctx);
	if (ret) {
		ALOGE("Failed to enumerate displays: %s", strerror(ret));
		goto out;
	}

	ctx->device.common.tag = HARDWARE_DEVICE_TAG;
	ctx->device.common.version = HWC_DEVICE_API_VERSION_1_4;
	ctx->device.common.module = const_cast<hw_module_t*>(module);
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
out:
	if (ctx->fd >= 0)
		close(ctx->fd);

	free(ctx);
	return ret;
}

static struct hw_module_methods_t hwc_module_methods = {
	open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
	common: {
		tag: HARDWARE_MODULE_TAG,
		version_major: 1,
		version_minor: 0,
		id: HWC_HARDWARE_MODULE_ID,
		name: "DRM hwcomposer module",
		author: "The Android Open Source Project",
		methods: &hwc_module_methods,
		dso: NULL,
		reserved: { 0 },
	}
};
