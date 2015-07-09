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
#define LOG_TAG "GLWorker"

#include <string>
#include <sstream>

#include <sys/resource.h>

#include <sync/sync.h>
#include <sw_sync.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>

#include <utils/Trace.h>

#include "glworker.h"

#include "seperate_rects.h"

// TODO(zachr): use hwc_drm_bo to turn buffer handles into textures
#ifndef EGL_NATIVE_HANDLE_ANDROID_NVX
#define EGL_NATIVE_HANDLE_ANDROID_NVX 0x322A
#endif

#define MAX_OVERLAPPING_LAYERS 64

namespace android {

typedef seperate_rects::Rect<float> FRect;
typedef seperate_rects::RectSet<uint64_t, float> FRectSet;

static const char *GetGLError(void) {
  switch (glGetError()) {
    case GL_NO_ERROR:
      return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";
    default:
      return "Unknown error";
  }
}

static const char *GetGLFramebufferError(void) {
  switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    case GL_FRAMEBUFFER_COMPLETE:
      return "GL_FRAMEBUFFER_COMPLETE";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_UNSUPPORTED:
      return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
      return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    default:
      return "Unknown error";
  }
}

static const char *GetEGLError(void) {
  switch (eglGetError()) {
    case EGL_SUCCESS:
      return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
      return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
      return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONTEXT:
      return "EGL_BAD_CONTEXT";
    case EGL_BAD_CONFIG:
      return "EGL_BAD_CONFIG";
    case EGL_BAD_CURRENT_SURFACE:
      return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "EGL_BAD_DISPLAY";
    case EGL_BAD_SURFACE:
      return "EGL_BAD_SURFACE";
    case EGL_BAD_MATCH:
      return "EGL_BAD_MATCH";
    case EGL_BAD_PARAMETER:
      return "EGL_BAD_PARAMETER";
    case EGL_BAD_NATIVE_PIXMAP:
      return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "EGL_BAD_NATIVE_WINDOW";
    case EGL_CONTEXT_LOST:
      return "EGL_CONTEXT_LOST";
    default:
      return "Unknown error";
  }
}

static bool HasExtension(const char *extension, const char *extensions) {
  const char *start, *where, *terminator;
  start = extensions;
  for (;;) {
    where = (char *)strstr((const char *)start, extension);
    if (!where)
      break;
    terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ')
      if (*terminator == ' ' || *terminator == '\0')
        return true;
    start = terminator;
  }
  return false;
}

static AutoGLShader CompileAndCheckShader(GLenum type, unsigned source_count,
                                          const GLchar **sources,
                                          std::string *shader_log) {
  GLint status;
  AutoGLShader shader(glCreateShader(type));
  if (shader.get() == 0) {
    *shader_log = "glCreateShader failed";
    return 0;
  }
  glShaderSource(shader.get(), source_count, sources, NULL);
  glCompileShader(shader.get());
  glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &status);
  if (!status) {
    if (shader_log) {
      GLint log_length;
      glGetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &log_length);
      shader_log->resize(log_length);
      glGetShaderInfoLog(shader.get(), log_length, NULL, &(*shader_log)[0]);
    }
    return 0;
  }

  return shader;
}

static int GenerateShaders(std::vector<AutoGLProgram> *blend_programs) {
  // Limits: GL_MAX_VARYING_COMPONENTS, GL_MAX_TEXTURE_IMAGE_UNITS,
  // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
  // clang-format off
  const GLchar *shader_preamble = "#version 300 es\n#define LAYER_COUNT ";

  const GLchar *vertex_shader_source =
"\n"
"precision mediump int;                                                     \n"
"uniform vec4 uViewport;                                                    \n"
"uniform sampler2D uLayerTextures[LAYER_COUNT];                             \n"
"uniform vec4 uLayerCrop[LAYER_COUNT];                                      \n"
"in vec2 vPosition;                                                         \n"
"in vec2 vTexCoords;                                                        \n"
"out vec2 fTexCoords[LAYER_COUNT];                                          \n"
"void main() {                                                              \n"
"  for (int i = 0; i < LAYER_COUNT; i++) {                                  \n"
"    fTexCoords[i] = (uLayerCrop[i].xy + vTexCoords * uLayerCrop[i].zw) /   \n"
"                     vec2(textureSize(uLayerTextures[i], 0));              \n"
"  }                                                                        \n"
"  vec2 scaledPosition = uViewport.xy + vPosition * uViewport.zw;           \n"
"  gl_Position = vec4(scaledPosition * vec2(2.0) - vec2(1.0), 0.0, 1.0);    \n"
"}                                                                          \n";

  const GLchar *fragment_shader_source =
"\n"
"precision mediump float;                                                   \n"
"uniform sampler2D uLayerTextures[LAYER_COUNT];                             \n"
"uniform float uLayerAlpha[LAYER_COUNT];                                    \n"
"in vec2 fTexCoords[LAYER_COUNT];                                           \n"
"out vec4 oFragColor;                                                       \n"
"void main() {                                                              \n"
"  vec3 color = vec3(0.0, 0.0, 0.0);                                        \n"
"  float alphaCover = 1.0;                                                  \n"
"  for (int i = 0; i < LAYER_COUNT; i++) {                                  \n"
"    vec4 texSample = texture(uLayerTextures[i], fTexCoords[i]);            \n"
"    float a = texSample.a * uLayerAlpha[i];                                \n"
"    color += a * alphaCover * texSample.rgb;                               \n"
"    alphaCover *= 1.0 - a;                                                 \n"
"    if (alphaCover <= 0.5/255.0)                                           \n"
"      break;                                                               \n"
"  }                                                                        \n"
"  oFragColor = vec4(color, 1.0 - alphaCover);                              \n"
"}                                                                          \n";
  // clang-format on

  int i, ret = 1;
  GLint max_texture_images, status;
  AutoGLShader vertex_shader, fragment_shader;
  AutoGLProgram program;
  std::string shader_log;

  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_images);

  for (i = 1; i <= max_texture_images; i++) {
    std::ostringstream layer_count_formatter;
    layer_count_formatter << i;
    std::string layer_count(layer_count_formatter.str());
    const GLchar *shader_sources[3] = {shader_preamble, layer_count.c_str(),
                                       NULL};

    shader_sources[2] = vertex_shader_source;
    vertex_shader = CompileAndCheckShader(GL_VERTEX_SHADER, 3, shader_sources,
                                          ret ? &shader_log : NULL);
    if (!vertex_shader.get()) {
      if (ret)
        ALOGE("Failed to make vertex shader:\n%s", shader_log.c_str());
      break;
    }

    shader_sources[2] = fragment_shader_source;
    fragment_shader = CompileAndCheckShader(
        GL_FRAGMENT_SHADER, 3, shader_sources, ret ? &shader_log : NULL);
    if (!fragment_shader.get()) {
      if (ret)
        ALOGE("Failed to make fragment shader:\n%s", shader_log.c_str());
      break;
    }

    program = AutoGLProgram(glCreateProgram());
    if (!program.get()) {
      if (ret)
        ALOGE("Failed to create program %s", GetGLError());
      break;
    }

    glAttachShader(program.get(), vertex_shader.get());
    glAttachShader(program.get(), fragment_shader.get());
    glBindAttribLocation(program.get(), 0, "vPosition");
    glBindAttribLocation(program.get(), 1, "vTexCoords");
    glLinkProgram(program.get());
    glDetachShader(program.get(), vertex_shader.get());
    glDetachShader(program.get(), fragment_shader.get());

    glGetProgramiv(program.get(), GL_LINK_STATUS, &status);
    if (!status) {
      if (ret) {
        GLint log_length;
        glGetProgramiv(program.get(), GL_INFO_LOG_LENGTH, &log_length);
        std::string program_log(log_length, ' ');
        glGetProgramInfoLog(program.get(), log_length, NULL, &program_log[0]);
        ALOGE("Failed to link program: \n%s", program_log.c_str());
      }
      break;
    }

    ret = 0;
    blend_programs->emplace_back(std::move(program));
  }

  return ret;
}

struct RenderingCommand {
  struct TextureSource {
    unsigned texture_index;
    float crop_bounds[4];
    float alpha;
  };

  float bounds[4];
  unsigned texture_count;
  TextureSource textures[MAX_OVERLAPPING_LAYERS];

  RenderingCommand() : texture_count(0) {
  }
};

static void ConstructCommands(const hwc_layer_1 *layers, size_t num_layers,
                              std::vector<RenderingCommand> *commands) {
  std::vector<FRect> in_rects;
  std::vector<FRectSet> out_rects;
  int i;

  for (unsigned rect_index = 0; rect_index < num_layers; rect_index++) {
    const hwc_layer_1 &layer = layers[rect_index];
    FRect rect;
    in_rects.push_back(FRect(layer.displayFrame.left, layer.displayFrame.top,
                             layer.displayFrame.right,
                             layer.displayFrame.bottom));
  }

  seperate_frects_64(in_rects, &out_rects);

  for (unsigned rect_index = 0; rect_index < out_rects.size(); rect_index++) {
    const FRectSet &out_rect = out_rects[rect_index];
    commands->push_back(RenderingCommand());
    RenderingCommand &cmd = commands->back();

    memcpy(cmd.bounds, out_rect.rect.bounds, sizeof(cmd.bounds));

    uint64_t tex_set = out_rect.id_set.getBits();
    for (unsigned i = num_layers - 1; tex_set != 0x0; i--) {
      if (tex_set & (0x1 << i)) {
        tex_set &= ~(0x1 << i);

        const hwc_layer_1 &layer = layers[i];

        FRect display_rect(layer.displayFrame.left, layer.displayFrame.top,
                           layer.displayFrame.right, layer.displayFrame.bottom);
        float display_size[2] = {
            display_rect.bounds[2] - display_rect.bounds[0],
            display_rect.bounds[3] - display_rect.bounds[1]};

        FRect crop_rect(layer.sourceCropf.left, layer.sourceCropf.top,
                        layer.sourceCropf.right, layer.sourceCropf.bottom);
        float crop_size[2] = {crop_rect.bounds[2] - crop_rect.bounds[0],
                              crop_rect.bounds[3] - crop_rect.bounds[1]};

        RenderingCommand::TextureSource &src = cmd.textures[cmd.texture_count];
        cmd.texture_count++;
        src.texture_index = i;

        for (int b = 0; b < 4; b++) {
          float bound_percent = (cmd.bounds[b] - display_rect.bounds[b % 2]) /
                                display_size[b % 2];
          src.crop_bounds[b] =
              crop_rect.bounds[b % 2] + bound_percent * crop_size[b % 2];
        }

        if (layer.blending == HWC_BLENDING_NONE) {
          src.alpha = 1.0f;
          // This layer is opaque. There is no point in using layers below this
          // one.
          break;
        }

        src.alpha = layer.planeAlpha / 255.0f;
      }
    }
  }
}

static int EGLFenceWait(EGLDisplay egl_display, int acquireFenceFd) {
  int ret = 0;

  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, acquireFenceFd,
                      EGL_NONE};
  EGLSyncKHR egl_sync =
      eglCreateSyncKHR(egl_display, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
  if (egl_sync == EGL_NO_SYNC_KHR) {
    ALOGE("Failed to make EGLSyncKHR from acquireFenceFd: %s", GetEGLError());
    close(acquireFenceFd);
    return 1;
  }

  EGLint success = eglWaitSyncKHR(egl_display, egl_sync, 0);
  if (success == EGL_FALSE) {
    ALOGE("Failed to wait for acquire: %s", GetEGLError());
    ret = 1;
  }
  eglDestroySyncKHR(egl_display, egl_sync);

  return ret;
}

static int CreateTextureFromHandle(EGLDisplay egl_display,
                                   buffer_handle_t handle,
                                   AutoEGLImageAndGLTexture *out) {
  EGLImageKHR image = eglCreateImageKHR(
      egl_display, EGL_NO_CONTEXT, EGL_NATIVE_HANDLE_ANDROID_NVX,
      (EGLClientBuffer)handle, NULL /* no attribs */);

  if (image == EGL_NO_IMAGE_KHR) {
    ALOGE("Failed to make image %s %p", GetEGLError(), handle);
    return -EINVAL;
  }

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  out->image.reset(image);
  out->texture.reset(texture);

  return 0;
}

GLWorker::Compositor::Compositor()
    : egl_display_(EGL_NO_DISPLAY), egl_ctx_(EGL_NO_CONTEXT) {
}

int GLWorker::Compositor::Init() {
  int ret = 0;
  const char *egl_extensions;
  const char *gl_extensions;
  EGLint num_configs;
  EGLint attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE, EGL_NONE};
  EGLConfig egl_config;

  // clang-format off
  const GLfloat verts[] = {
    0.0f,  0.0f,    0.0f, 0.0f,
    0.0f,  2.0f,    0.0f, 2.0f,
    2.0f,  0.0f,    2.0f, 0.0f
  };
  // clang-format on

  const EGLint config_attribs[] = {EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_ES2_BIT,
                                   EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_NONE};

  const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

  egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl_display_ == EGL_NO_DISPLAY) {
    ALOGE("Failed to get egl display");
    return 1;
  }

  if (!eglInitialize(egl_display_, NULL, NULL)) {
    ALOGE("Failed to initialize egl: %s", GetEGLError());
    return 1;
  }

  egl_extensions = eglQueryString(egl_display_, EGL_EXTENSIONS);

  // These extensions are all technically required but not always reported due
  // to meta EGL filtering them out.
  if (!HasExtension("EGL_KHR_image_base", egl_extensions))
    ALOGW("EGL_KHR_image_base extension not supported");

  if (!HasExtension("EGL_ANDROID_image_native_buffer", egl_extensions))
    ALOGW("EGL_ANDROID_image_native_buffer extension not supported");

  if (!HasExtension("EGL_ANDROID_native_fence_sync", egl_extensions))
    ALOGW("EGL_ANDROID_native_fence_sync extension not supported");

  if (!eglChooseConfig(egl_display_, config_attribs, &egl_config, 1,
                       &num_configs)) {
    ALOGE("eglChooseConfig() failed with error: %s", GetEGLError());
    return 1;
  }

  egl_ctx_ =
      eglCreateContext(egl_display_, egl_config,
                       EGL_NO_CONTEXT /* No shared context */, context_attribs);

  if (egl_ctx_ == EGL_NO_CONTEXT) {
    ALOGE("Failed to create OpenGL ES Context: %s", GetEGLError());
    return 1;
  }

  if (!eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_ctx_)) {
    ALOGE("Failed to make the OpenGL ES Context current: %s", GetEGLError());
    return 1;
  }

  gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

  if (!HasExtension("GL_OES_EGL_image", gl_extensions))
    ALOGW("GL_OES_EGL_image extension not supported");

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  vertex_buffer_.reset(vertex_buffer);

  if (GenerateShaders(&blend_programs_)) {
    return 1;
  }

  return 0;
}

GLWorker::Compositor::~Compositor() {
  if (egl_display_ != EGL_NO_DISPLAY && egl_ctx_ != EGL_NO_CONTEXT)
    if (eglDestroyContext(egl_display_, egl_ctx_) == EGL_FALSE)
      ALOGE("Failed to destroy OpenGL ES Context: %s", GetEGLError());
}

int GLWorker::Compositor::Composite(hwc_layer_1 *layers, size_t num_layers,
                                    sp<GraphicBuffer> framebuffer) {
  ATRACE_CALL();
  int ret = 0;
  size_t i;
  std::vector<AutoEGLImageAndGLTexture> layer_textures;
  std::vector<RenderingCommand> commands;

  if (num_layers == 0) {
    return -EALREADY;
  }

  GLint frame_width = framebuffer->getWidth();
  GLint frame_height = framebuffer->getHeight();
  EGLSyncKHR finished_sync;

  AutoEGLImageKHR egl_fb_image(
      eglCreateImageKHR(egl_display_, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                        (EGLClientBuffer)framebuffer->getNativeBuffer(),
                        NULL /* no attribs */),
      EGLImageDeleter(egl_display_));

  if (egl_fb_image.get() == EGL_NO_IMAGE_KHR) {
    ALOGE("Failed to make image from target buffer: %s", GetEGLError());
    return -EINVAL;
  }

  GLuint gl_fb_tex;
  glGenTextures(1, &gl_fb_tex);
  AutoGLTexture gl_fb_tex_auto(gl_fb_tex);
  glBindTexture(GL_TEXTURE_2D, gl_fb_tex);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                               (GLeglImageOES)egl_fb_image.get());
  glBindTexture(GL_TEXTURE_2D, 0);

  GLuint gl_fb;
  glGenFramebuffers(1, &gl_fb);
  AutoGLFramebuffer gl_fb_auto(gl_fb);
  glBindFramebuffer(GL_FRAMEBUFFER, gl_fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         gl_fb_tex, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    ALOGE("Failed framebuffer check for created target buffer: %s",
          GetGLFramebufferError());
    return -EINVAL;
  }

  for (i = 0; i < num_layers; i++) {
    const struct hwc_layer_1 *layer = &layers[i];

    if (ret) {
      if (layer->acquireFenceFd >= 0)
        close(layer->acquireFenceFd);
      continue;
    }

    layer_textures.emplace_back(egl_display_);
    ret = CreateTextureFromHandle(egl_display_, layer->handle,
                                  &layer_textures.back());
    if (!ret) {
      ret = EGLFenceWait(egl_display_, layer->acquireFenceFd);
    }
    if (ret) {
      layer_textures.pop_back();
      ret = -EINVAL;
    }
  }

  if (ret)
    return ret;

  ConstructCommands(layers, num_layers, &commands);

  glViewport(0, 0, frame_width, frame_height);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_.get());
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, NULL);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                        (void *)(sizeof(float) * 2));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnable(GL_SCISSOR_TEST);

  for (const RenderingCommand &cmd : commands) {
    if (cmd.texture_count <= 0) {
      continue;
    }

    // TODO(zachr): handle the case of too many overlapping textures for one
    // area by falling back to rendering as many layers as possible using
    // multiple blending passes.
    if (cmd.texture_count > blend_programs_.size()) {
      ALOGE("Too many layers to render in one area");
      continue;
    }

    GLint program = blend_programs_[cmd.texture_count - 1].get();
    glUseProgram(program);
    GLint gl_viewport_loc = glGetUniformLocation(program, "uViewport");
    GLint gl_tex_loc = glGetUniformLocation(program, "uLayerTextures");
    GLint gl_crop_loc = glGetUniformLocation(program, "uLayerCrop");
    GLint gl_alpha_loc = glGetUniformLocation(program, "uLayerAlpha");
    glUniform4f(gl_viewport_loc, cmd.bounds[0] / (float)frame_width,
                cmd.bounds[1] / (float)frame_height,
                (cmd.bounds[2] - cmd.bounds[0]) / (float)frame_width,
                (cmd.bounds[3] - cmd.bounds[1]) / (float)frame_height);

    for (unsigned src_index = 0; src_index < cmd.texture_count; src_index++) {
      const RenderingCommand::TextureSource &src = cmd.textures[src_index];
      glUniform1f(gl_alpha_loc + src_index, src.alpha);
      glUniform4f(gl_crop_loc + src_index, src.crop_bounds[0],
                  src.crop_bounds[1], src.crop_bounds[2] - src.crop_bounds[0],
                  src.crop_bounds[3] - src.crop_bounds[1]);

      glUniform1i(gl_tex_loc + src_index, src_index);
      glActiveTexture(GL_TEXTURE0 + src_index);
      glBindTexture(GL_TEXTURE_2D,
                    layer_textures[src.texture_index].texture.get());
    }

    glScissor(cmd.bounds[0], cmd.bounds[1], cmd.bounds[2] - cmd.bounds[0],
              cmd.bounds[3] - cmd.bounds[1]);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    for (unsigned src_index = 0; src_index < cmd.texture_count; src_index++) {
      glActiveTexture(GL_TEXTURE0 + src_index);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
  }

  glDisable(GL_SCISSOR_TEST);
  glActiveTexture(GL_TEXTURE0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return ret;
}

int GLWorker::DoComposition(Compositor &compositor, Work *work) {
  int ret =
      compositor.Composite(work->layers, work->num_layers, work->framebuffer);

  int timeline_fd = work->timeline_fd;
  work->timeline_fd = -1;

  if (ret) {
    worker_ret_ = ret;
    glFinish();
    sw_sync_timeline_inc(timeline_fd, work->num_layers);
    close(timeline_fd);
    return pthread_cond_signal(&work_done_cond_);
  }

  unsigned timeline_count = work->num_layers + 1;
  worker_ret_ = sw_sync_fence_create(timeline_fd, "GLComposition done fence",
                                     timeline_count);
  ret = pthread_cond_signal(&work_done_cond_);

  glFinish();

  sw_sync_timeline_inc(timeline_fd, timeline_count);
  close(timeline_fd);

  return ret;
}

GLWorker::GLWorker() : initialized_(false) {
}

GLWorker::~GLWorker() {
  if (!initialized_)
    return;

  if (SignalWorker(NULL, true) != 0 || pthread_join(thread_, NULL) != 0)
    pthread_kill(thread_, SIGTERM);

  pthread_cond_destroy(&work_ready_cond_);
  pthread_cond_destroy(&work_done_cond_);
  pthread_mutex_destroy(&lock_);
}

#define TRY(x, n, g)                  \
  ret = x;                            \
  if (ret) {                          \
    ALOGE("Failed to " n " %d", ret); \
    g;                                \
  }

#define TRY_RETURN(x, n) TRY(x, n, return ret)

int GLWorker::Init() {
  int ret = 0;

  worker_work_ = NULL;
  worker_exit_ = false;
  worker_ret_ = -1;

  ret = pthread_cond_init(&work_ready_cond_, NULL);
  if (ret) {
    ALOGE("Failed to int GLThread condition %d", ret);
    return ret;
  }

  ret = pthread_cond_init(&work_done_cond_, NULL);
  if (ret) {
    ALOGE("Failed to int GLThread condition %d", ret);
    pthread_cond_destroy(&work_ready_cond_);
    return ret;
  }

  ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to init GLThread lock %d", ret);
    pthread_cond_destroy(&work_ready_cond_);
    pthread_cond_destroy(&work_done_cond_);
    return ret;
  }

  ret = pthread_create(&thread_, NULL, StartRoutine, this);
  if (ret) {
    ALOGE("Failed to create GLThread %d", ret);
    pthread_cond_destroy(&work_ready_cond_);
    pthread_cond_destroy(&work_done_cond_);
    pthread_mutex_destroy(&lock_);
    return ret;
  }

  initialized_ = true;

  TRY_RETURN(pthread_mutex_lock(&lock_), "lock GLThread");

  while (!worker_exit_ && worker_ret_ != 0)
    TRY(pthread_cond_wait(&work_done_cond_, &lock_), "wait on condition",
        goto out_unlock);

  ret = worker_ret_;

out_unlock:
  int unlock_ret = pthread_mutex_unlock(&lock_);
  if (unlock_ret) {
    ret = unlock_ret;
    ALOGE("Failed to unlock GLThread %d", unlock_ret);
  }
  return ret;
}

int GLWorker::SignalWorker(Work *work, bool worker_exit) {
  int ret = 0;
  if (worker_exit_)
    return -EINVAL;
  TRY_RETURN(pthread_mutex_lock(&lock_), "lock GLThread");
  worker_work_ = work;
  worker_exit_ = worker_exit;
  ret = pthread_cond_signal(&work_ready_cond_);
  if (ret) {
    ALOGE("Failed to signal GLThread caller %d", ret);
    pthread_mutex_unlock(&lock_);
    return ret;
  }
  ret = pthread_cond_wait(&work_done_cond_, &lock_);
  if (ret) {
    ALOGE("Failed to wait on GLThread %d", ret);
    pthread_mutex_unlock(&lock_);
    return ret;
  }

  ret = worker_ret_;
  if (ret) {
    pthread_mutex_unlock(&lock_);
    return ret;
  }
  TRY_RETURN(pthread_mutex_unlock(&lock_), "unlock GLThread");
  return ret;
}

int GLWorker::DoWork(Work *work) {
  return SignalWorker(work, false);
}

void GLWorker::WorkerRoutine() {
  int ret = 0;

  TRY(pthread_mutex_lock(&lock_), "lock GLThread", return );

  Compositor compositor;

  TRY(compositor.Init(), "initialize GL", goto out_signal_done);

  worker_ret_ = 0;
  TRY(pthread_cond_signal(&work_done_cond_), "signal GLThread caller",
      goto out_signal_done);

  while (true) {
    while (worker_work_ == NULL && !worker_exit_)
      TRY(pthread_cond_wait(&work_ready_cond_, &lock_), "wait on condition",
          goto out_signal_done);

    if (worker_exit_) {
      ret = 0;
      break;
    }

    ret = DoComposition(compositor, worker_work_);

    worker_work_ = NULL;
    if (ret) {
      break;
    }
  }

out_signal_done:
  worker_exit_ = true;
  worker_ret_ = ret;
  TRY(pthread_cond_signal(&work_done_cond_), "signal GLThread caller",
      goto out_unlock);
out_unlock:
  TRY(pthread_mutex_unlock(&lock_), "unlock GLThread", return );
}

/* static */
void *GLWorker::StartRoutine(void *arg) {
  setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);
  GLWorker *worker = (GLWorker *)arg;
  worker->WorkerRoutine();
  return NULL;
}

}  // namespace android
