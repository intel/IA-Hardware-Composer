/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
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

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <drm_fourcc.h>

#include "esUtil.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>

#include <libsync.h>

#include <gpudevice.h>
#include <hwclayer.h>
#include <nativedisplay.h>
#include <platformdefines.h>
#include <nativefence.h>
#include <spinlock.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Exit after rendering the given number of frames. If 0, then continue
 * rendering forever.
 */
static uint64_t arg_frames = 0;

struct frame {
  struct gbm_bo *gbm_bo;
  EGLImageKHR egl_image;
  GLuint gl_renderbuffer;
  GLuint gl_framebuffer;
  hwcomposer::HwcLayer layer;
  struct gbm_handle native_handle;
  // NativeFence release_fence;
};

static struct frame frames[2];

static struct {
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  GLuint program;
  GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
  GLuint vbo;
  GLuint positionsoffset, colorsoffset, normalsoffset;

  PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC
  glEGLImageTargetRenderbufferStorageOES;
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
  PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
  PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
  PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
  PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
  PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
} gl;

class HotPlugEventCallback : public hwcomposer::DisplayHotPlugEventCallback {
 public:
  HotPlugEventCallback(hwcomposer::GpuDevice *device) : device_(device) {
  }

  void Callback(std::vector<hwcomposer::NativeDisplay *> connected_displays) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    connected_displays_.swap(connected_displays);
  }

  const std::vector<hwcomposer::NativeDisplay *> &GetConnectedDisplays() {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    if (connected_displays_.empty())
      connected_displays_ = device_->GetConnectedPhysicalDisplays();

    return connected_displays_;
  }

  void PresentLayers(std::vector<hwcomposer::HwcLayer *> &layers) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    if (connected_displays_.empty())
      connected_displays_ = device_->GetConnectedPhysicalDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->Present(layers);
  }

 private:
  std::vector<hwcomposer::NativeDisplay *> connected_displays_;
  hwcomposer::GpuDevice *device_;
  hwcomposer::SpinLock spin_lock_;
};

static struct { struct gbm_device *dev; } gbm;

struct drm_fb {
  struct gbm_bo *bo;
};

static int init_gbm(int fd) {
  gbm.dev = gbm_create_device(fd);
  if (!gbm.dev) {
    printf("failed to create gbm device\n");
    return -1;
  }

  return 0;
}

static int init_gl(int32_t width, int32_t height) {
  EGLint major, minor, n;
  GLuint vertex_shader, fragment_shader;
  GLint ret;
  static const GLfloat vVertices[] = {// front
                                      -1.0f, -1.0f,
                                      +1.0f,  // point blue
                                      +1.0f, -1.0f,
                                      +1.0f,  // point magenta
                                      -1.0f, +1.0f,
                                      +1.0f,  // point cyan
                                      +1.0f, +1.0f,
                                      +1.0f,  // point white
                                      // back
                                      +1.0f, -1.0f,
                                      -1.0f,  // point red
                                      -1.0f, -1.0f,
                                      -1.0f,  // point black
                                      +1.0f, +1.0f,
                                      -1.0f,  // point yellow
                                      -1.0f, +1.0f,
                                      -1.0f,  // point green
                                      // right
                                      +1.0f, -1.0f,
                                      +1.0f,  // point magenta
                                      +1.0f, -1.0f,
                                      -1.0f,  // point red
                                      +1.0f, +1.0f,
                                      +1.0f,  // point white
                                      +1.0f, +1.0f,
                                      -1.0f,  // point yellow
                                      // left
                                      -1.0f, -1.0f,
                                      -1.0f,  // point black
                                      -1.0f, -1.0f,
                                      +1.0f,  // point blue
                                      -1.0f, +1.0f,
                                      -1.0f,  // point green
                                      -1.0f, +1.0f,
                                      +1.0f,  // point cyan
                                      // top
                                      -1.0f, +1.0f,
                                      +1.0f,  // point cyan
                                      +1.0f, +1.0f,
                                      +1.0f,  // point white
                                      -1.0f, +1.0f,
                                      -1.0f,  // point green
                                      +1.0f, +1.0f,
                                      -1.0f,  // point yellow
                                      // bottom
                                      -1.0f, -1.0f,
                                      -1.0f,  // point black
                                      +1.0f, -1.0f,
                                      -1.0f,  // point red
                                      -1.0f, -1.0f,
                                      +1.0f,  // point blue
                                      +1.0f, -1.0f,
                                      +1.0f  // point magenta
  };

  static const GLfloat vColors[] = {// front
                                    0.0f, 0.0f,
                                    1.0f,  // blue
                                    1.0f, 0.0f,
                                    1.0f,  // magenta
                                    0.0f, 1.0f,
                                    1.0f,  // cyan
                                    1.0f, 1.0f,
                                    1.0f,  // white
                                    // back
                                    1.0f, 0.0f,
                                    0.0f,  // red
                                    0.0f, 0.0f,
                                    0.0f,  // black
                                    1.0f, 1.0f,
                                    0.0f,  // yellow
                                    0.0f, 1.0f,
                                    0.0f,  // green
                                    // right
                                    1.0f, 0.0f,
                                    1.0f,  // magenta
                                    1.0f, 0.0f,
                                    0.0f,  // red
                                    1.0f, 1.0f,
                                    1.0f,  // white
                                    1.0f, 1.0f,
                                    0.0f,  // yellow
                                    // left
                                    0.0f, 0.0f,
                                    0.0f,  // black
                                    0.0f, 0.0f,
                                    1.0f,  // blue
                                    0.0f, 1.0f,
                                    0.0f,  // green
                                    0.0f, 1.0f,
                                    1.0f,  // cyan
                                    // top
                                    0.0f, 1.0f,
                                    1.0f,  // cyan
                                    1.0f, 1.0f,
                                    1.0f,  // white
                                    0.0f, 1.0f,
                                    0.0f,  // green
                                    1.0f, 1.0f,
                                    0.0f,  // yellow
                                    // bottom
                                    0.0f, 0.0f,
                                    0.0f,  // black
                                    1.0f, 0.0f,
                                    0.0f,  // red
                                    0.0f, 0.0f,
                                    1.0f,  // blue
                                    1.0f, 0.0f,
                                    1.0f  // magenta
  };

  static const GLfloat vNormals[] = {// front
                                     +0.0f, +0.0f,
                                     +1.0f,  // forward
                                     +0.0f, +0.0f,
                                     +1.0f,  // forward
                                     +0.0f, +0.0f,
                                     +1.0f,  // forward
                                     +0.0f, +0.0f,
                                     +1.0f,  // forward
                                     // back
                                     +0.0f, +0.0f,
                                     -1.0f,  // backbard
                                     +0.0f, +0.0f,
                                     -1.0f,  // backbard
                                     +0.0f, +0.0f,
                                     -1.0f,  // backbard
                                     +0.0f, +0.0f,
                                     -1.0f,  // backbard
                                     // right
                                     +1.0f, +0.0f,
                                     +0.0f,  // right
                                     +1.0f, +0.0f,
                                     +0.0f,  // right
                                     +1.0f, +0.0f,
                                     +0.0f,  // right
                                     +1.0f, +0.0f,
                                     +0.0f,  // right
                                     // left
                                     -1.0f, +0.0f,
                                     +0.0f,  // left
                                     -1.0f, +0.0f,
                                     +0.0f,  // left
                                     -1.0f, +0.0f,
                                     +0.0f,  // left
                                     -1.0f, +0.0f,
                                     +0.0f,  // left
                                     // top
                                     +0.0f, +1.0f,
                                     +0.0f,  // up
                                     +0.0f, +1.0f,
                                     +0.0f,  // up
                                     +0.0f, +1.0f,
                                     +0.0f,  // up
                                     +0.0f, +1.0f,
                                     +0.0f,  // up
                                     // bottom
                                     +0.0f, -1.0f,
                                     +0.0f,  // down
                                     +0.0f, -1.0f,
                                     +0.0f,  // down
                                     +0.0f, -1.0f,
                                     +0.0f,  // down
                                     +0.0f, -1.0f,
                                     +0.0f  // down
  };

  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                           EGL_NONE};

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                          EGL_NONE};

  static const char *vertex_shader_source =
      "uniform mat4 modelviewMatrix;      \n"
      "uniform mat4 modelviewprojectionMatrix;\n"
      "uniform mat3 normalMatrix;         \n"
      "                                   \n"
      "attribute vec4 in_position;        \n"
      "attribute vec3 in_normal;          \n"
      "attribute vec4 in_color;           \n"
      "\n"
      "vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    gl_Position = modelviewprojectionMatrix * in_position;\n"
      "    vec3 vEyeNormal = normalMatrix * in_normal;\n"
      "    vec4 vPosition4 = modelviewMatrix * in_position;\n"
      "    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
      "    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
      "    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
      "    vVaryingColor = vec4(diff * in_color.rgb, 1.0);\n"
      "}                                  \n";

  static const char *fragment_shader_source =
      "precision mediump float;           \n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    gl_FragColor = vVaryingColor;  \n"
      "}                                  \n";

  gl.display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                     EGL_DEFAULT_DISPLAY, NULL);

  if (!eglInitialize(gl.display, &major, &minor)) {
    printf("failed to initialize\n");
    return -1;
  }

#define get_proc(name, proc)                  \
  do {                                        \
    gl.name = (proc)eglGetProcAddress(#name); \
    assert(gl.name);                          \
  } while (0)
  get_proc(glEGLImageTargetRenderbufferStorageOES,
           PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC);
  get_proc(eglCreateImageKHR, PFNEGLCREATEIMAGEKHRPROC);
  get_proc(eglCreateSyncKHR, PFNEGLCREATESYNCKHRPROC);
  get_proc(eglDestroySyncKHR, PFNEGLDESTROYSYNCKHRPROC);
  get_proc(eglWaitSyncKHR, PFNEGLWAITSYNCKHRPROC);
  get_proc(eglClientWaitSyncKHR, PFNEGLCLIENTWAITSYNCKHRPROC);
  get_proc(eglDupNativeFenceFDANDROID, PFNEGLDUPNATIVEFENCEFDANDROIDPROC);

  printf("Using display %p with EGL version %d.%d\n", gl.display, major, minor);

  printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
  printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
  printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    printf("failed to bind api EGL_OPENGL_ES_API\n");
    return -1;
  }

  if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) ||
      n != 1) {
    printf("failed to choose config: %d\n", n);
    return -1;
  }

  gl.context =
      eglCreateContext(gl.display, gl.config, EGL_NO_CONTEXT, context_attribs);
  if (gl.context == NULL) {
    printf("failed to create context\n");
    return -1;
  }

  eglMakeCurrent(gl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, gl.context);

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
  if (!ret) {
    GLint log_length;
    glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
    std::string info_log(log_length, ' ');

    printf("vertex shader compilation failed!:\n");
    if (log_length > 1) {
      glGetShaderInfoLog(vertex_shader, log_length, NULL,
                         (GLchar *)info_log.c_str());
      printf("%s", info_log.c_str());
    }

    return -1;
  }

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
  if (!ret) {
    GLint log_length;
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
    std::string info_log(log_length, ' ');

    printf("fragment shader compilation failed!:\n");

    if (log_length > 1) {
      glGetShaderInfoLog(fragment_shader, log_length, NULL,
                         (GLchar *)info_log.c_str());
      printf("%s", info_log.c_str());
    }

    return -1;
  }

  gl.program = glCreateProgram();

  glAttachShader(gl.program, vertex_shader);
  glAttachShader(gl.program, fragment_shader);

  glBindAttribLocation(gl.program, 0, "in_position");
  glBindAttribLocation(gl.program, 1, "in_normal");
  glBindAttribLocation(gl.program, 2, "in_color");

  glLinkProgram(gl.program);

  glGetProgramiv(gl.program, GL_LINK_STATUS, &ret);
  if (!ret) {
    printf("program linking failed!:\n");
    GLint log_length;
    glGetProgramiv(gl.program, GL_INFO_LOG_LENGTH, &log_length);
    std::string program_log(log_length, ' ');

    if (log_length > 1) {
      glGetProgramInfoLog(gl.program, log_length, NULL,
                          (GLchar *)program_log.c_str());
      printf("%s", program_log.c_str());
    }

    return -1;
  }

  glUseProgram(gl.program);

  gl.modelviewmatrix = glGetUniformLocation(gl.program, "modelviewMatrix");
  gl.modelviewprojectionmatrix =
      glGetUniformLocation(gl.program, "modelviewprojectionMatrix");
  gl.normalmatrix = glGetUniformLocation(gl.program, "normalMatrix");

  glViewport(0, 0, width, height);
  glEnable(GL_CULL_FACE);

  gl.positionsoffset = 0;
  gl.colorsoffset = sizeof(vVertices);
  gl.normalsoffset = sizeof(vVertices) + sizeof(vColors);
  glGenBuffers(1, &gl.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, gl.positionsoffset, sizeof(vVertices),
                  &vVertices[0]);
  glBufferSubData(GL_ARRAY_BUFFER, gl.colorsoffset, sizeof(vColors),
                  &vColors[0]);
  glBufferSubData(GL_ARRAY_BUFFER, gl.normalsoffset, sizeof(vNormals),
                  &vNormals[0]);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) gl.positionsoffset);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) gl.normalsoffset);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) gl.colorsoffset);
  glEnableVertexAttribArray(2);
  printf("KMS: EGL initialization succeeded. \n");
  return 0;
}

static void init_frames(int32_t width, int32_t height) {
  for (int i = 0; i < ARRAY_SIZE(frames); ++i) {
    struct frame *frame = &frames[i];

    frame->gbm_bo = gbm_bo_create(gbm.dev, width, height, GBM_FORMAT_XRGB8888,
                                  GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!frame->gbm_bo) {
      printf("failed to create gbm_bo\n");
      exit(EXIT_FAILURE);
    }

    int gbm_bo_fd = gbm_bo_get_fd(frame->gbm_bo);
    if (gbm_bo_fd == -1) {
      printf("gbm_bo_get_fd() failed\n");
      exit(EXIT_FAILURE);
    }

    const EGLint image_attrs[] = {
        EGL_WIDTH,                     width,
        EGL_HEIGHT,                    height,
        EGL_LINUX_DRM_FOURCC_EXT,      DRM_FORMAT_XRGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT,     gbm_bo_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  gbm_bo_get_stride(frame->gbm_bo),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE,
    };

    frame->egl_image =
        gl.eglCreateImageKHR(gl.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                             (EGLClientBuffer)NULL, image_attrs);
    if (!frame->egl_image) {
      printf("failed to create EGLImage from gbm_bo\n");
      exit(EXIT_FAILURE);
    }

    glGenRenderbuffers(1, &frame->gl_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, frame->gl_renderbuffer);
    gl.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
                                              frame->egl_image);
    if (glGetError() != GL_NO_ERROR) {
      printf("failed to create GL renderbuffer from EGLImage\n");
      exit(EXIT_FAILURE);
    }

    glGenFramebuffers(1, &frame->gl_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frame->gl_framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, frame->gl_renderbuffer);
    if (glGetError() != GL_NO_ERROR) {
      printf("failed to create GL framebuffer\n");
      exit(EXIT_FAILURE);
    }

    frame->native_handle.import_data.fd = gbm_bo_fd;
    frame->native_handle.import_data.width = width;
    frame->native_handle.import_data.height = height;
    frame->native_handle.import_data.stride = gbm_bo_get_stride(frame->gbm_bo);
    frame->native_handle.import_data.format = gbm_bo_get_format(frame->gbm_bo);

    frame->layer.SetTransform(0);
    frame->layer.SetSourceCrop(hwcomposer::HwcRect<float>(0, 0, width, height));
    frame->layer.SetDisplayFrame(hwcomposer::HwcRect<int>(0, 0, width, height));
    frame->layer.SetNativeHandle(&frame->native_handle);
  }
}

static void draw(uint32_t i, int32_t width, int32_t height) {
  ESMatrix modelview;

  /* clear the color buffer */
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  esMatrixLoadIdentity(&modelview);
  esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
  esRotate(&modelview, 45.0f + (0.25f * i), 1.0f, 0.0f, 0.0f);
  esRotate(&modelview, 45.0f - (0.5f * i), 0.0f, 1.0f, 0.0f);
  esRotate(&modelview, 10.0f + (0.15f * i), 0.0f, 0.0f, 1.0f);

  GLfloat aspect = (GLfloat)(height) / (GLfloat)(width);

  ESMatrix projection;
  esMatrixLoadIdentity(&projection);
  esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f,
            10.0f);

  ESMatrix modelviewprojection;
  esMatrixLoadIdentity(&modelviewprojection);
  esMatrixMultiply(&modelviewprojection, &modelview, &projection);

  float normal[9];
  normal[0] = modelview.m[0][0];
  normal[1] = modelview.m[0][1];
  normal[2] = modelview.m[0][2];
  normal[3] = modelview.m[1][0];
  normal[4] = modelview.m[1][1];
  normal[5] = modelview.m[1][2];
  normal[6] = modelview.m[2][0];
  normal[7] = modelview.m[2][1];
  normal[8] = modelview.m[2][2];

  glUniformMatrix4fv(gl.modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
  glUniformMatrix4fv(gl.modelviewprojectionmatrix, 1, GL_FALSE,
                     &modelviewprojection.m[0][0]);
  glUniformMatrix3fv(gl.normalmatrix, 1, GL_FALSE, normal);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
}

static EGLSyncKHR create_fence(int fd) {
  EGLint attrib_list[] = {
      EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd, EGL_NONE,
  };
  EGLSyncKHR fence = gl.eglCreateSyncKHR(
      gl.display, EGL_SYNC_NATIVE_FENCE_ANDROID, attrib_list);
  assert(fence);
  return fence;
}

static void print_help(void) {
  printf("usage: kmscube [-h|--help] [-f|--frames <frames>]\n");
}

static void parse_args(int argc, char *argv[]) {
  static const struct option longopts[] = {
      {"help", no_argument, NULL, 'h'},
      {"frames", required_argument, NULL, 'f'},
      {0},
  };

  char *endptr;
  int opt;
  int longindex = 0;

  /* Suppress getopt's poor error messages */
  opterr = 0;

  while ((opt = getopt_long(argc, argv, "+:hf:", longopts,
                            /*longindex*/ &longindex)) != -1) {
    switch (opt) {
      case 'h':
        print_help();
        exit(0);
        break;
      case 'f':
        errno = 0;
        arg_frames = strtoul(optarg, &endptr, 0);
        if (errno || *endptr != '\0') {
          fprintf(stderr, "usage error: invalid value for <frames>\n");
          exit(EXIT_FAILURE);
        }
        break;
      case ':':
        fprintf(stderr, "usage error: %s requires an argument\n",
                argv[optind - 1]);
        exit(EXIT_FAILURE);
        break;
      case '?':
      default:
        assert(opt == '?');
        fprintf(stderr, "usage error: unknown option '%s'\n", argv[optind - 1]);
        exit(EXIT_FAILURE);
        break;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "usage error: trailing args\n");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  struct drm_fb *fb;
  int ret, fd, primary_width, primary_height;
  hwcomposer::GpuDevice device;
  device.Initialize();
  auto callback = std::make_shared<HotPlugEventCallback>(&device);
  device.RegisterHotPlugEventCallback(callback);
  const std::vector<hwcomposer::NativeDisplay *> &displays =
      callback->GetConnectedDisplays();
  if (displays.empty())
    return 0;

  parse_args(argc, argv);
  fd = open("/dev/dri/renderD128", O_RDWR);
  primary_width = displays.at(0)->Width();
  primary_height = displays.at(0)->Height();

  ret = init_gbm(fd);
  if (ret) {
    printf("failed to initialize GBM\n");
    close(fd);
    return ret;
  }

  ret = init_gl(primary_width, primary_height);
  if (ret) {
    printf("failed to initialize EGL\n");
    close(fd);
    return ret;
  }

  init_frames(primary_width, primary_height);

  /* clear the color buffer */
  glBindFramebuffer(GL_FRAMEBUFFER, frames[0].gl_framebuffer);
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  int64_t gpu_fence_fd = -1; /* out-fence from gpu, in-fence to kms */
  std::vector<hwcomposer::HwcLayer *> layers;

  for (uint64_t i = 1; arg_frames == 0 || i < arg_frames; ++i) {
    struct frame *frame = &frames[i % ARRAY_SIZE(frames)];
    if (frame->layer.release_fence.get() != -1) {
      ret = sync_wait(frame->layer.release_fence.get(), 1000);
      frame->layer.release_fence.Reset(-1);
      if (ret) {
        printf("failed waiting on sync fence: %s\n", strerror(errno));
        return -1;
      }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, frame->gl_framebuffer);
    draw(i, primary_width, primary_height);
    EGLSyncKHR gpu_fence = create_fence(EGL_NO_NATIVE_FENCE_FD_ANDROID);
    int64_t gpu_fence_fd = gl.eglDupNativeFenceFDANDROID(gl.display, gpu_fence);
    gl.eglDestroySyncKHR(gl.display, gpu_fence);
    assert(gpu_fence_fd != -1);
    frame->layer.acquire_fence = gpu_fence_fd;
    std::vector<hwcomposer::HwcLayer *>().swap(layers);
    layers.emplace_back(&frame->layer);
    callback->PresentLayers(layers);
  }

  close(fd);
  return ret;
}
