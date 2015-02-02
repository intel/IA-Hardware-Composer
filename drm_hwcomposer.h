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

struct hwc_import_context;

enum {
       GRALLOC_MODULE_PERFORM_DRM_IMPORT = 0xffeeff00
};

struct hwc_drm_bo {
	uint32_t width;
	uint32_t height;
	uint32_t format;	/* DRM_FORMAT_* from drm_fourcc.h */
	uint32_t pitches[4];
	uint32_t offsets[4];
	uint32_t gem_handles[4];
	uint32_t fb_id;
	int acquire_fence_fd;

	/* TODO: This is bad voodoo, remove it once drm_gralloc uses dma_buf */
	int importer_fd;
};

int hwc_import_init(struct hwc_import_context **ctx);
int hwc_import_destroy(struct hwc_import_context *ctx);

int hwc_create_bo_from_import(int fd, struct hwc_import_context *ctx,
			buffer_handle_t buf, struct hwc_drm_bo *bo);
