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

#define LOG_TAG "GLCompositor"

#include <sstream>
#include <string>
#include <vector>

#include <cutils/log.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>

#include <cutils/properties.h>
#include <sync/sync.h>
#include <sw_sync.h>

#include "drm_hwcomposer.h"

#include "gl_compositor.h"
#include "seperate_rects.h"

// TODO(zachr): use hwc_drm_bo to turn buffer handles into textures
#ifndef EGL_NATIVE_HANDLE_ANDROID_NVX
#define EGL_NATIVE_HANDLE_ANDROID_NVX 0x322A
#endif

#define MAX_OVERLAPPING_LAYERS 64

namespace android {

struct GLCompositor::texture_from_handle {
  EGLImageKHR image;
  GLuint texture;
};

static const char *get_gl_error(void);
static const char *get_egl_error(void);
static bool has_extension(const char *extension, const char *extensions);

template <typename T>
int AllocResource(std::vector<T> &array) {
  for (typename std::vector<T>::iterator it = array.begin(); it != array.end();
       ++it) {
    if (!it->is_some()) {
      return std::distance(array.begin(), it);
    }
  }

  array.push_back(T());
  return array.size() - 1;
}

template <typename T>
void FreeResource(std::vector<T> &array, int index) {
  if (index == (int)array.size() - 1) {
    array.pop_back();
  } else if (index >= 0 && (unsigned)index < array.size()) {
    array[index].Reset();
  }
}

struct GLTarget {
  sp<GraphicBuffer> fb;
  EGLImageKHR egl_fb_image;
  GLuint gl_fb;
  GLuint gl_fb_tex;
  bool forgotten;
  unsigned composition_count;

  GLTarget()
      : egl_fb_image(EGL_NO_IMAGE_KHR),
        gl_fb(0),
        gl_fb_tex(0),
        forgotten(true),
        composition_count(0) {
  }

  void Reset() {
    fb.clear();
    egl_fb_image = EGL_NO_IMAGE_KHR;
    gl_fb = 0;
    gl_fb_tex = 0;
    forgotten = true;
    composition_count = 0;
  }

  bool is_some() const {
    return egl_fb_image != EGL_NO_IMAGE_KHR;
  }
};

struct GLCompositor::priv_data {
  EGLDisplay egl_display;
  EGLContext egl_ctx;

  EGLDisplay saved_egl_display;
  EGLContext saved_egl_ctx;
  EGLSurface saved_egl_read;
  EGLSurface saved_egl_draw;

  int current_target;
  std::vector<GLTarget> targets;
  std::vector<GLComposition *> compositions;

  std::vector<GLint> blend_programs;
  GLuint vertex_buffer;

  priv_data()
      : egl_display(EGL_NO_DISPLAY),
        egl_ctx(EGL_NO_CONTEXT),
        saved_egl_display(EGL_NO_DISPLAY),
        saved_egl_ctx(EGL_NO_CONTEXT),
        saved_egl_read(EGL_NO_SURFACE),
        saved_egl_draw(EGL_NO_SURFACE),
        current_target(-1) {
  }
};

class GLComposition : public Composition {
 public:
  struct LayerData {
    hwc_layer_1 layer;
    hwc_drm_bo bo;
  };

  GLComposition(GLCompositor *owner, Importer *imp)
      : compositor(owner), importer(imp), target_handle(-1) {
  }

  virtual ~GLComposition() {
    if (compositor == NULL) {
      return;
    }

    // Removes this composition from the owning compositor automatically.
    std::vector<GLComposition *> &compositions =
        compositor->priv_->compositions;
    std::vector<GLComposition *>::iterator it =
        std::find(compositions.begin(), compositions.end(), this);
    if (it != compositions.end()) {
      compositions.erase(it);
    }

    GLTarget *target = &compositor->priv_->targets[target_handle];
    target->composition_count--;
    compositor->CheckAndDestroyTarget(target_handle);
  }

  virtual int AddLayer(int display, hwc_layer_1 *layer, hwc_drm_bo *bo) {
    (void)display;
    if (layer->compositionType != HWC_OVERLAY) {
      ALOGE("Must add layers with compositionType == HWC_OVERLAY");
      return 1;
    }

    if (layer->handle == 0) {
      ALOGE("Must add layers with valid buffer handle");
      return 1;
    }

    layer_data.push_back(LayerData());
    LayerData &layer_datum = layer_data.back();
    layer_datum.layer = *layer;
    layer_datum.bo = *bo;

    return importer->ReleaseBuffer(bo);
  }

  virtual unsigned GetRemainingLayers(int display, unsigned num_needed) const {
    (void)display;
    return num_needed;
  }

  GLCompositor *compositor;
  Importer *importer;
  int target_handle;
  std::vector<LayerData> layer_data;
};

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

static void ConstructCommands(const GLComposition &composition,
                              std::vector<RenderingCommand> *commands) {
  std::vector<seperate_rects::Rect<float> > in_rects;
  std::vector<seperate_rects::RectSet<uint64_t, float> > out_rects;
  int i;

  for (unsigned rect_index = 0; rect_index < composition.layer_data.size();
       rect_index++) {
    const struct hwc_layer_1 &layer = composition.layer_data[rect_index].layer;
    seperate_rects::Rect<float> rect;
    in_rects.push_back(seperate_rects::Rect<float>(
        layer.displayFrame.left, layer.displayFrame.top,
        layer.displayFrame.right, layer.displayFrame.bottom));
  }

  seperate_frects_64(in_rects, &out_rects);

  for (unsigned rect_index = 0; rect_index < out_rects.size(); rect_index++) {
    const seperate_rects::RectSet<uint64_t, float> &out_rect =
        out_rects[rect_index];
    commands->push_back(RenderingCommand());
    RenderingCommand &cmd = commands->back();

    memcpy(cmd.bounds, out_rect.rect.bounds, sizeof(cmd.bounds));

    uint64_t tex_set = out_rect.id_set.getBits();
    for (unsigned i = composition.layer_data.size() - 1; tex_set != 0x0; i--) {
      if (tex_set & (0x1 << i)) {
        tex_set &= ~(0x1 << i);

        const struct hwc_layer_1 &layer = composition.layer_data[i].layer;

        seperate_rects::Rect<float> display_rect(
            layer.displayFrame.left, layer.displayFrame.top,
            layer.displayFrame.right, layer.displayFrame.bottom);
        float display_size[2] = {
            display_rect.bounds[2] - display_rect.bounds[0],
            display_rect.bounds[3] - display_rect.bounds[1]};

        seperate_rects::Rect<float> crop_rect(
            layer.sourceCropf.left, layer.sourceCropf.top,
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

GLCompositor::GLCompositor() {
  priv_ = new priv_data;
}

GLCompositor::~GLCompositor() {
  if (BeginContext()) {
    goto destroy_ctx;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  for (std::vector<GLTarget>::iterator it = priv_->targets.end();
       it != priv_->targets.begin(); it = priv_->targets.end()) {
    --it;
    glDeleteFramebuffers(1, &it->gl_fb);
    glDeleteTextures(1, &it->gl_fb_tex);
    eglDestroyImageKHR(priv_->egl_display, it->egl_fb_image);
    priv_->targets.erase(it);
  }

  for (std::vector<GLComposition *>::iterator it = priv_->compositions.end();
       it != priv_->compositions.begin(); it = priv_->compositions.end()) {
    --it;

    // Prevents compositor from trying to erase itself
    (*it)->compositor = NULL;
    delete *it;
    priv_->compositions.erase(it);
  }

destroy_ctx:
  eglMakeCurrent(priv_->egl_display,
                 EGL_NO_SURFACE /* No default draw surface */,
                 EGL_NO_SURFACE /* No default draw read */, EGL_NO_CONTEXT);
  eglDestroyContext(priv_->egl_display, priv_->egl_ctx);

  EndContext();
  delete priv_;
}

int GLCompositor::Init() {
  int ret = 0;
  const char *egl_extensions;
  const char *gl_extensions;
  EGLint num_configs;
  EGLint attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE, EGL_NONE};
  EGLConfig egl_config;

  // clang-format off
  const GLfloat verts[] = {
    0.0f,  0.0f,    0.0f, 0.0f,
    0.0f,  1.0f,    0.0f, 1.0f,
    1.0f,  0.0f,    1.0f, 0.0f,
    1.0f,  1.0f,    1.0f, 1.0f
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

  priv_->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (priv_->egl_display == EGL_NO_DISPLAY) {
    ALOGE("Failed to get egl display");
    ret = 1;
    goto out;
  }

  if (!eglInitialize(priv_->egl_display, NULL, NULL)) {
    ALOGE("Failed to initialize egl: %s", get_egl_error());
    ret = 1;
    goto out;
  }

  egl_extensions = eglQueryString(priv_->egl_display, EGL_EXTENSIONS);

  // These extensions are all technically required but not always reported due
  // to meta EGL filtering them out.
  if (!has_extension("EGL_KHR_image_base", egl_extensions))
    ALOGW("EGL_KHR_image_base extension not supported");

  if (!has_extension("EGL_ANDROID_image_native_buffer", egl_extensions))
    ALOGW("EGL_ANDROID_image_native_buffer extension not supported");

  if (!has_extension("EGL_ANDROID_native_fence_sync", egl_extensions))
    ALOGW("EGL_ANDROID_native_fence_sync extension not supported");

  if (!eglChooseConfig(priv_->egl_display, config_attribs, &egl_config, 1,
                       &num_configs)) {
    ALOGE("eglChooseConfig() failed with error: %s", get_egl_error());
    goto out;
  }

  priv_->egl_ctx =
      eglCreateContext(priv_->egl_display, egl_config,
                       EGL_NO_CONTEXT /* No shared context */, context_attribs);

  if (priv_->egl_ctx == EGL_NO_CONTEXT) {
    ALOGE("Failed to create OpenGL ES Context: %s", get_egl_error());
    ret = 1;
    goto out;
  }

  ret = BeginContext();
  if (ret)
    goto out;

  gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

  if (!has_extension("GL_OES_EGL_image", gl_extensions))
    ALOGW("GL_OES_EGL_image extension not supported");

  glGenBuffers(1, &priv_->vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, priv_->vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  if (GenerateShaders()) {
    ret = 1;
    goto end_ctx;
  }

end_ctx:
  EndContext();

out:
  return ret;
}

Targeting *GLCompositor::targeting() {
  return (Targeting *)this;
}

int GLCompositor::CreateTarget(sp<GraphicBuffer> &buffer) {
  int ret;

  ret = BeginContext();
  if (ret)
    return -1;

  int target_handle = AllocResource(priv_->targets);
  GLTarget *target = &priv_->targets[target_handle];

  target->fb = buffer;

  target->egl_fb_image = eglCreateImageKHR(
      priv_->egl_display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
      (EGLClientBuffer)target->fb->getNativeBuffer(), NULL /* no attribs */);

  if (target->egl_fb_image == EGL_NO_IMAGE_KHR) {
    ALOGE("Failed to make image from target buffer: %s", get_egl_error());
    ret = -1;
    goto fail_create_image;
  }

  glGenTextures(1, &target->gl_fb_tex);
  glBindTexture(GL_TEXTURE_2D, target->gl_fb_tex);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                               (GLeglImageOES)target->egl_fb_image);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &target->gl_fb);
  glBindFramebuffer(GL_FRAMEBUFFER, target->gl_fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         target->gl_fb_tex, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    ALOGE("Failed framebuffer check for created target buffer");
    ret = 1;
    goto fail_framebuffer_status;
  }

  target->forgotten = false;

  ret = target_handle;
  goto out;

fail_framebuffer_status:
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &target->gl_fb);
  glDeleteTextures(1, &target->gl_fb_tex);
  eglDestroyImageKHR(priv_->egl_display, target->egl_fb_image);
  target->gl_fb = 0;
  target->gl_fb_tex = 0;
  target->egl_fb_image = EGL_NO_IMAGE_KHR;

fail_create_image:
  target->fb.clear();
  FreeResource(priv_->targets, target_handle);

out:
  EndContext();
  return ret;
}

void GLCompositor::SetTarget(int target_handle) {
  if (target_handle >= 0 && (unsigned)target_handle < priv_->targets.size()) {
    GLTarget *target = &priv_->targets[target_handle];
    if (target->is_some()) {
      priv_->current_target = target_handle;
      return;
    }
  }

  priv_->current_target = -1;
}

void GLCompositor::ForgetTarget(int target_handle) {
  if (target_handle >= 0 && (unsigned)target_handle < priv_->targets.size()) {
    if (target_handle == priv_->current_target) {
      priv_->current_target = -1;
    }

    GLTarget *target = &priv_->targets[target_handle];
    if (target->is_some()) {
      target->forgotten = true;
      CheckAndDestroyTarget(target_handle);
      return;
    }
  }

  ALOGE("Failed to forget target because of invalid handle");
}

void GLCompositor::CheckAndDestroyTarget(int target_handle) {
  GLTarget *target = &priv_->targets[target_handle];
  if (target->composition_count == 0 && target->forgotten) {
    if (BeginContext() == 0) {
      glDeleteFramebuffers(1, &target->gl_fb);
      glDeleteTextures(1, &target->gl_fb_tex);
      eglDestroyImageKHR(priv_->egl_display, target->egl_fb_image);
      EndContext();
    }

    FreeResource(priv_->targets, target_handle);
  }
}

Composition *GLCompositor::CreateComposition(Importer *importer) {
  if (priv_->current_target >= 0 &&
      (unsigned)priv_->current_target < priv_->targets.size()) {
    GLTarget *target = &priv_->targets[priv_->current_target];
    if (target->is_some()) {
      GLComposition *composition = new GLComposition(this, importer);
      composition->target_handle = priv_->current_target;
      target->composition_count++;
      priv_->compositions.push_back(composition);
      return composition;
    }
  }

  ALOGE("Failed to create composition because of invalid target handle %d",
        priv_->current_target);

  return NULL;
}

int GLCompositor::QueueComposition(Composition *composition) {
  if (composition) {
    int ret = DoComposition(*(GLComposition *)composition);
    delete composition;
    return ret;
  }

  ALOGE("Failed to queue composition because of invalid composition handle");

  return -EINVAL;
}

int GLCompositor::Composite() {
  return 0;
}

int GLCompositor::BeginContext() {
  priv_->saved_egl_display = eglGetCurrentDisplay();
  priv_->saved_egl_ctx = eglGetCurrentContext();

  if (priv_->saved_egl_display != priv_->egl_display ||
      priv_->saved_egl_ctx != priv_->egl_ctx) {
    priv_->saved_egl_read = eglGetCurrentSurface(EGL_READ);
    priv_->saved_egl_draw = eglGetCurrentSurface(EGL_DRAW);
  } else {
    return 0;
  }

  if (!eglMakeCurrent(priv_->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      priv_->egl_ctx)) {
    ALOGE("Failed to make the OpenGL ES Context current: %s", get_egl_error());
    return 1;
  }
  return 0;
}

int GLCompositor::EndContext() {
  if (priv_->saved_egl_display != eglGetCurrentDisplay() ||
      priv_->saved_egl_ctx != eglGetCurrentContext()) {
    if (!eglMakeCurrent(priv_->saved_egl_display, priv_->saved_egl_read,
                        priv_->saved_egl_draw, priv_->saved_egl_ctx)) {
      ALOGE("Failed to make the old OpenGL ES Context current: %s",
            get_egl_error());
      return 1;
    }
  }

  return 0;
}

GLint CompileAndCheckShader(GLenum type, unsigned source_count,
                            const GLchar **sources, std::string *shader_log) {
  GLint status;
  GLint shader = glCreateShader(type);
  if (!shader) {
    *shader_log = "glCreateShader failed";
    return 0;
  }
  glShaderSource(shader, source_count, sources, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    if (shader_log) {
      GLint log_length;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
      shader_log->resize(log_length);
      glGetShaderInfoLog(shader, log_length, NULL, &(*shader_log)[0]);
    }
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

int GLCompositor::GenerateShaders() {
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
  GLint max_texture_images, vertex_shader, fragment_shader, program, status;
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
    if (!vertex_shader) {
      if (ret) {
        ALOGE("Failed to make vertex shader:\n%s", shader_log.c_str());
      }
      break;
    }

    shader_sources[2] = fragment_shader_source;
    fragment_shader = CompileAndCheckShader(
        GL_FRAGMENT_SHADER, 3, shader_sources, ret ? &shader_log : NULL);
    if (!fragment_shader) {
      if (ret) {
        ALOGE("Failed to make fragment shader:\n%s", shader_log.c_str());
      }
      goto delete_vs;
    }

    program = glCreateProgram();
    if (!program) {
      if (ret)
        ALOGE("Failed to create program %s", get_gl_error());
      goto delete_fs;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glBindAttribLocation(program, 0, "vPosition");
    glBindAttribLocation(program, 1, "vTexCoords");
    glLinkProgram(program);
    glDetachShader(program, vertex_shader);
    glDeleteShader(vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(fragment_shader);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
      if (ret) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::string program_log(log_length, ' ');
        glGetProgramInfoLog(program, log_length, NULL, &program_log[0]);
        ALOGE("Failed to link program: \n%s", program_log.c_str());
      }
      glDeleteProgram(program);
      break;
    }

    ret = 0;
    priv_->blend_programs.push_back(program);

    continue;

  delete_fs:
    glDeleteShader(fragment_shader);

  delete_vs:
    glDeleteShader(vertex_shader);

    if (ret)
      break;
  }

  return ret;
}

int GLCompositor::DoComposition(const GLComposition &composition) {
  int ret = 0;
  size_t i;
  std::vector<struct texture_from_handle> layer_textures;
  std::vector<RenderingCommand> commands;

  if (composition.layer_data.size() == 0) {
    return -EALREADY;
  }

  if (BeginContext()) {
    return -EINVAL;
  }

  GLTarget *target = &priv_->targets[composition.target_handle];
  GLint frame_width = target->fb->getWidth();
  GLint frame_height = target->fb->getHeight();
  EGLSyncKHR finished_sync;

  for (i = 0; i < composition.layer_data.size(); i++) {
    const struct hwc_layer_1 *layer = &composition.layer_data[i].layer;

    if (ret) {
      if (layer->acquireFenceFd >= 0)
        close(layer->acquireFenceFd);
      continue;
    }

    layer_textures.push_back(texture_from_handle());
    ret = CreateTextureFromHandle(layer->handle, &layer_textures.back());
    if (!ret) {
      ret = DoFenceWait(layer->acquireFenceFd);
    }
    if (ret) {
      layer_textures.pop_back();
      ret = -EINVAL;
    }
  }

  if (ret) {
    goto destroy_textures;
  }

  ConstructCommands(composition, &commands);

  glBindFramebuffer(GL_FRAMEBUFFER, target->gl_fb);

  glViewport(0, 0, frame_width, frame_height);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindBuffer(GL_ARRAY_BUFFER, priv_->vertex_buffer);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, NULL);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                        (void *)(sizeof(float) * 2));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  for (std::vector<RenderingCommand>::iterator it = commands.begin();
       it != commands.end(); ++it) {
    const RenderingCommand &cmd = *it;

    if (cmd.texture_count <= 0) {
      continue;
    }

    // TODO(zachr): handle the case of too many overlapping textures for one
    // area by falling back to rendering as many layers as possible using
    // multiple blending passes.
    if (cmd.texture_count > priv_->blend_programs.size()) {
      ALOGE("Too many layers to render in one area");
      continue;
    }

    GLint program = priv_->blend_programs[cmd.texture_count - 1];
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
      glBindTexture(GL_TEXTURE_2D, layer_textures[src.texture_index].texture);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    for (unsigned src_index = 0; src_index < cmd.texture_count; src_index++) {
      glActiveTexture(GL_TEXTURE0 + src_index);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
  }

  glActiveTexture(GL_TEXTURE0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  finished_sync =
      eglCreateSyncKHR(priv_->egl_display, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
  if (finished_sync != EGL_NO_SYNC_KHR) {
    glFlush();  // Creates the syncpoint
    ret = eglDupNativeFenceFDANDROID(priv_->egl_display, finished_sync);
    eglDestroySyncKHR(priv_->egl_display, finished_sync);
    if (ret != EGL_NO_NATIVE_FENCE_FD_ANDROID) {
      goto destroy_textures;
    }
  }

  // Used as a fallback if the native sync fence fails.
  ret = -EALREADY;
  glFinish();

destroy_textures:
  for (i = 0; i < layer_textures.size(); i++)
    DestroyTextureFromHandle(layer_textures[i]);

  EndContext();

  return ret;
}

int GLCompositor::DoFenceWait(int acquireFenceFd) {
  int ret = 0;

  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, acquireFenceFd,
                      EGL_NONE};
  EGLSyncKHR egl_sync = eglCreateSyncKHR(
      priv_->egl_display, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
  if (egl_sync == EGL_NO_SYNC_KHR) {
    ALOGE("Failed to make EGLSyncKHR from acquireFenceFd: %s", get_egl_error());
    close(acquireFenceFd);
    return 1;
  }

  EGLint success = eglWaitSyncKHR(priv_->egl_display, egl_sync, 0);
  if (success == EGL_FALSE) {
    ALOGE("Failed to wait for acquire: %s", get_egl_error());
    ret = 1;
  }
  eglDestroySyncKHR(priv_->egl_display, egl_sync);

  return ret;
}

int GLCompositor::CreateTextureFromHandle(buffer_handle_t handle,
                                          struct texture_from_handle *tex) {
  EGLImageKHR image = eglCreateImageKHR(
      priv_->egl_display, EGL_NO_CONTEXT, EGL_NATIVE_HANDLE_ANDROID_NVX,
      (EGLClientBuffer)handle, NULL /* no attribs */);

  if (image == EGL_NO_IMAGE_KHR) {
    ALOGE("Failed to make image %s %p", get_egl_error(), handle);
    return 1;
  }

  glGenTextures(1, &tex->texture);
  glBindTexture(GL_TEXTURE_2D, tex->texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  tex->image = image;

  return 0;
}

void GLCompositor::DestroyTextureFromHandle(
    const struct texture_from_handle &tex) {
  glDeleteTextures(1, &tex.texture);
  eglDestroyImageKHR(priv_->egl_display, tex.image);
}

static const char *get_gl_error(void) {
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

static const char *get_egl_error(void) {
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

static bool has_extension(const char *extension, const char *extensions) {
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

}  // namespace android
