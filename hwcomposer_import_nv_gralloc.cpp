/*
 * Copyright (C) 2015 The Android Open Source Project
 * Copyright (C) 2015 NVIDIA Corporation.
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


#include <cutils/log.h>
#include <hardware/gralloc.h>

#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "drm_hwcomposer.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr) / sizeof((arr)[0]))

struct hwc_import_context {
	const gralloc_module_t *gralloc_module;
};

int hwc_import_init(struct hwc_import_context **ctx)
{
	int ret;
	struct hwc_import_context *import_ctx;

	import_ctx = (struct hwc_import_context *)calloc(1,
				sizeof(*import_ctx));
	if (!ctx) {
		ALOGE("Failed to allocate gralloc import context");
		return -ENOMEM;
	}

	ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
			(const hw_module_t **)&import_ctx->gralloc_module);
	if (ret) {
		ALOGE("Failed to open gralloc module");
		goto err;
	}

	if (!strcasecmp(import_ctx->gralloc_module->common.author, "NVIDIA"))
		ALOGW("Using non-NVIDIA gralloc module: %s\n",
			import_ctx->gralloc_module->common.name);

	*ctx = import_ctx;

	return 0;

err:
	free(import_ctx);
	return ret;
}

int hwc_import_destroy(struct hwc_import_context *ctx)
{
	free(ctx);
	return 0;
}

struct importer_priv
{
	int drm_fd;
	struct hwc_drm_bo bo;
};

static void free_priv(void *p)
{
	struct importer_priv *priv = (struct importer_priv *)p;
	struct drm_gem_close gem_close;
	int i, ret;

	if (priv->bo.fb_id) {
		ret = drmModeRmFB(priv->drm_fd, priv->bo.fb_id);
		if (ret)
			ALOGE("Failed to rm fb %d", ret);
	}

	memset(&gem_close, 0, sizeof(gem_close));
	for (i = 0; i < ARRAY_SIZE(priv->bo.gem_handles); i++) {
		if (!priv->bo.gem_handles[i])
			continue;
		gem_close.handle = priv->bo.gem_handles[i];
		ret = drmIoctl(priv->drm_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
		if (ret)
			ALOGE("Failed to close gem handle %d", ret);
	}

	free(priv);
}

static int
hwc_import_set_priv(hwc_import_context *ctx, buffer_handle_t handle, struct importer_priv *priv)
{
	int ret;
	const gralloc_module_t *g = ctx->gralloc_module;

	return g->perform(g, GRALLOC_MODULE_PERFORM_SET_IMPORTER_PRIVATE, handle, free_priv, priv);
}

static struct importer_priv *
hwc_import_get_priv(hwc_import_context *ctx, buffer_handle_t handle)
{
	int ret;
	void *priv = NULL;
	const gralloc_module_t *g = ctx->gralloc_module;

	ret = g->perform(g, GRALLOC_MODULE_PERFORM_GET_IMPORTER_PRIVATE, handle, free_priv, &priv);
	return ret ? NULL : (struct importer_priv *)priv;
}

int hwc_import_bo_create(int fd, hwc_import_context *ctx,
			buffer_handle_t handle, struct hwc_drm_bo *bo)
{
	int ret = 0;
	const gralloc_module_t *g = ctx->gralloc_module;

	/* Get imported bo that is cached in gralloc buffer, or create a new one. */
	struct importer_priv *priv = hwc_import_get_priv(ctx, handle);
	if (!priv) {
		priv = (struct importer_priv *)calloc(1, sizeof(*priv));
		if (!priv)
			return -ENOMEM;
		priv->drm_fd = fd;

		ret = g->perform(g, GRALLOC_MODULE_PERFORM_DRM_IMPORT, fd, handle, &priv->bo);
		if (ret) {
			ALOGE("GRALLOC_MODULE_PERFORM_DRM_IMPORT failed %d", ret);
			free_priv(priv);
			return ret;
		}

		ret = drmModeAddFB2(fd, priv->bo.width, priv->bo.height,
				    priv->bo.format, priv->bo.gem_handles,
				    priv->bo.pitches, priv->bo.offsets,
				    &priv->bo.fb_id, 0);
		if (ret) {
			ALOGE("Failed to add fb %d", ret);
			free_priv(priv);
			return ret;
		}

		ret = hwc_import_set_priv(ctx, handle, priv);
		if (ret) {
			/* This will happen is persist.tegra.gpu_mapping_cache is 0/off,
			 * or if NV gralloc runs out of "priv slots" (currently 3 per buffer,
			 * only one of which should be used by drm_hwcomposer). */
			ALOGE("Failed to register free callback for imported buffer %d", ret);
			free_priv(priv);
			return ret;
		}
	}
	*bo = priv->bo;
	return ret;
}

bool hwc_import_bo_release(int fd, hwc_import_context *ctx,
			struct hwc_drm_bo *bo)
{
	/* hwc may not close the gem handles, we own them */
	return false;
}
