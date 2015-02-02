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

#include "drm_hwcomposer.h"

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

int hwc_create_bo_from_import(int fd, hwc_import_context *ctx,
			      buffer_handle_t handle, struct hwc_drm_bo *bo)
{
	const gralloc_module_t *g = ctx->gralloc_module;

	bo->importer_fd = -1;
	return g->perform(g, GRALLOC_MODULE_PERFORM_DRM_IMPORT, fd, handle, bo);
}
