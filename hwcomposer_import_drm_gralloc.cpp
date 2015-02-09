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

#include <stdlib.h>

#include <cutils/log.h>

#include <drm/drm_fourcc.h>

#include <gralloc_drm.h>
#include <gralloc_drm_priv.h>
#include <gralloc_drm_handle.h>

#include "drm_hwcomposer.h"

struct hwc_import_context {
	struct drm_module_t *gralloc_module;
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

static uint32_t hwc_convert_hal_format_to_drm_format(uint32_t hal_format)
{
	switch(hal_format) {
	case HAL_PIXEL_FORMAT_RGB_888:
		return DRM_FORMAT_BGR888;
	case HAL_PIXEL_FORMAT_BGRA_8888:
		return DRM_FORMAT_ARGB8888;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		return DRM_FORMAT_XBGR8888;
	case HAL_PIXEL_FORMAT_RGBA_8888:
		return DRM_FORMAT_ABGR8888;
	case HAL_PIXEL_FORMAT_RGB_565:
		return DRM_FORMAT_BGR565;
	case HAL_PIXEL_FORMAT_YV12:
		return DRM_FORMAT_YVU420;
	default:
		ALOGE("Cannot convert hal format to drm format %u", hal_format);
		return -EINVAL;
	}
}

int hwc_create_bo_from_import(int fd, hwc_import_context *ctx,
			buffer_handle_t handle, struct hwc_drm_bo *bo)
{
	gralloc_drm_handle_t *gr_handle = gralloc_drm_handle(handle);
	struct gralloc_drm_bo_t *gralloc_bo;
	uint32_t gem_handle;
	int ret;

	if (!gr_handle)
		return -EINVAL;

	gralloc_bo = gr_handle->data;
	if (!gralloc_bo) {
		ALOGE("Could not get drm bo from handle");
		return -EINVAL;
	}

	ret = drmPrimeFDToHandle(fd, gr_handle->prime_fd, &gem_handle);
	if (ret) {
		ALOGE("failed to import prime fd %d ret=%d",
			gr_handle->prime_fd, ret);
		return ret;
	}

	bo->width = gr_handle->width;
	bo->height = gr_handle->height;
	bo->format = hwc_convert_hal_format_to_drm_format(gr_handle->format);
	bo->pitches[0] = gr_handle->stride;
	bo->gem_handles[0] = gem_handle;
	bo->offsets[0] = 0;

	return 0;
}
