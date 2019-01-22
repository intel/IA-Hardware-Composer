/*
 * Copyright (c) 2017 Rob Clark <rclark@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "common.h"
#include "drm-common.h"

static struct drm drm;

static void page_flip_handler(int fd, unsigned int frame, unsigned int sec,
                              unsigned int usec, void *data) {
  /* suppress 'unused parameter' warnings */
  (void)fd, (void)frame, (void)sec, (void)usec;

  int *waiting_for_flip = data;
  *waiting_for_flip = 0;
}

static struct drm_fb *legacyCreateFramebuffer(int width, int height,
                                              const struct gbm *gbm,
                                              const struct egl *egl) {
  struct gbm_bo *bo = NULL;
  struct drm_fb *fb = NULL;
  bo = gbm_bo_create(gbm->dev, width, height, GBM_FORMAT_XRGB8888,
                     GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!bo) {
    printf("failed to create a gbm buffer.\n");
    return fb;
  }

  int fd = gbm_bo_get_fd(bo);
  if (fd < 0) {
    printf("failed to get fb for bo: %d\n", fd);
    return fb;
  }

  uint32_t fb_id;
  GLuint gl_tex;
  GLuint gl_fb;

  uint32_t handle = gbm_bo_get_handle(bo).u32;
  uint32_t stride = gbm_bo_get_stride(bo);
  uint32_t offset = 0;
  drmModeAddFB2(drm.fd, width, height, GBM_FORMAT_XRGB8888, &handle, &stride,
                &offset, &fb_id, 0);
  if (!fb_id) {
    printf("failed to create framebuffer from buffer object.\n");
    return fb;
  }

  printf("fb_id: %d\n", fb_id);

  const EGLint khr_image_attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                                    fd,
                                    EGL_WIDTH,
                                    width,
                                    EGL_HEIGHT,
                                    height,
                                    EGL_LINUX_DRM_FOURCC_EXT,
                                    GBM_FORMAT_XRGB8888,
                                    EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                    stride,
                                    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                    offset,
                                    EGL_NONE};

  EGLImageKHR image =
      egl->eglCreateImageKHR(egl->display, EGL_NO_CONTEXT,
                             EGL_LINUX_DMA_BUF_EXT, NULL, khr_image_attrs);
  if (image == EGL_NO_IMAGE_KHR) {
    printf("failed to make image from buffer object: %s\n", eglGetError());
    return fb;
  }

  glGenTextures(1, &gl_tex);
  glBindTexture(GL_TEXTURE_2D, gl_tex);
  egl->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &gl_fb);
  glBindFramebuffer(GL_FRAMEBUFFER, gl_fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         gl_tex, 0);

  printf("gl_fb: %d\n", gl_fb);
  printf("gl_tex: %d\n", gl_tex);

  fb = calloc(1, sizeof *fb);
  fb->bo = bo;
  fb->fb_id = fb_id;

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("failed framebuffer check for created target buffer: %x\n",
           glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glDeleteFramebuffers(1, &gl_fb);
    free(fb);
    return NULL;
  }

  return fb;
}

static int legacy_run(const struct gbm *gbm, const struct egl *egl) {
  fd_set fds;
  drmEventContext evctx = {
      .version = 2, .page_flip_handler = page_flip_handler,
  };
  struct gbm_bo *bo;
  struct drm_fb *fb;
  uint32_t i = 0;
  int ret;

  eglSwapBuffers(egl->display, egl->surface);
  fb = legacyCreateFramebuffer(1920, 1080, gbm, egl);
  bo = fb->bo;

  if (!fb) {
    fprintf(stderr, "Failed to get a new framebuffer BO\n");
    return -1;
  }

  /* set mode: */
  ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0, &drm.connector_id,
                       1, drm.mode);
  if (ret) {
    printf("failed to set mode: %s\n", strerror(errno));
    return ret;
  }

  while (1) {
    struct gbm_bo *next_bo = bo;
    int waiting_for_flip = 1;

    egl->draw(i++);

    eglSwapBuffers(egl->display, egl->surface);

    /*
     * Here you could also update drm plane layers if you want
     * hw composition
     */

    ret = drmModePageFlip(drm.fd, drm.crtc_id, fb->fb_id,
                          DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
    if (ret) {
      printf("failed to queue page flip: %s\n", strerror(errno));
      return -1;
    }

    while (waiting_for_flip) {
      FD_ZERO(&fds);
      FD_SET(0, &fds);
      FD_SET(drm.fd, &fds);

      ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
      if (ret < 0) {
        printf("select err: %s\n", strerror(errno));
        return ret;
      } else if (ret == 0) {
        printf("select timeout!\n");
        return -1;
      } else if (FD_ISSET(0, &fds)) {
        printf("user interrupted!\n");
        return 0;
      }
      drmHandleEvent(drm.fd, &evctx);
    }

    /* release last buffer to render on again: */
    gbm_surface_release_buffer(gbm->surface, bo);
    bo = next_bo;
  }

  return 0;
}

static int get_plane_id(void) {
  drmModePlaneResPtr plane_resources;
  uint32_t i, j;
  int ret = -EINVAL;
  int found_reserved = 0;

  plane_resources = drmModeGetPlaneResources(drm.fd);
  if (!plane_resources) {
    printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
    return -1;
  }
  for (i = 0; (i < plane_resources->count_planes) && !found_reserved; i++) {
    uint32_t id = plane_resources->planes[i];
    printf("------------Plane[%d]-------------\n", id);
    drmModePlanePtr plane = drmModeGetPlane(drm.fd, id);
    if (!plane) {
      printf("drmModeGetPlane(%u) failed: %s\n", id, strerror(errno));
      continue;
    }

    // Third plane is reserved
    if (i == 1) {
      ret = id;
      found_reserved = 1;
    }

    drmModeFreePlane(plane);
  }

  drmModeFreePlaneResources(plane_resources);

  return ret;
}

const struct drm *init_drm_legacy(const char *device) {
  int ret;
  uint32_t plane_id;

  ret = init_drm(&drm, device);
  if (ret)
    return NULL;

  ret = get_plane_id();
  if (!ret) {
    printf("could not find a suitable plane\n");
    return NULL;
  } else {
    plane_id = ret;
  }

  drm.plane = calloc(1, sizeof(*drm.plane));
  drm.crtc = calloc(1, sizeof(*drm.crtc));
  drm.connector = calloc(1, sizeof(*drm.connector));

#define get_resource(type, Type, id)                                   \
  do {                                                                 \
    drm.type->type = drmModeGet##Type(drm.fd, id);                     \
    if (!drm.type->type) {                                             \
      printf("could not get %s %i: %s\n", #type, id, strerror(errno)); \
      return NULL;                                                     \
    }                                                                  \
  } while (0)

  get_resource(plane, Plane, plane_id);
  get_resource(crtc, Crtc, drm.crtc_id);
  get_resource(connector, Connector, drm.connector_id);

#define get_properties(type, TYPE, id)                                      \
  do {                                                                      \
    uint32_t i;                                                             \
    drm.type->props =                                                       \
        drmModeObjectGetProperties(drm.fd, id, DRM_MODE_OBJECT_##TYPE);     \
    if (!drm.type->props) {                                                 \
      printf("could not get %s %u properties: %s\n", #type, id,             \
             strerror(errno));                                              \
      return NULL;                                                          \
    }                                                                       \
    drm.type->props_info =                                                  \
        calloc(drm.type->props->count_props, sizeof(drm.type->props_info)); \
    for (i = 0; i < drm.type->props->count_props; i++) {                    \
      drm.type->props_info[i] =                                             \
          drmModeGetProperty(drm.fd, drm.type->props->props[i]);            \
    }                                                                       \
  } while (0)

  get_properties(plane, PLANE, plane_id);
  get_properties(crtc, CRTC, drm.crtc_id);
  get_properties(connector, CONNECTOR, drm.connector_id);

  printf("Plane[%d], CRTC[%d], CONNECTOR[%d]\n", plane_id, drm.crtc_id,
         drm.connector_id);

  drm.run = legacy_run;

  return &drm;
}
