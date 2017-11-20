/*
// Copyright (c) 2016 Intel Corporation
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

#include "glcubelayerrenderer.h"
#include <unistd.h>
#include <sys/mman.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <cmath>

#include <nativebufferhandler.h>

struct Dimension {
  EGLint width = 0;
  EGLint height = 0;
  EGLint stride = 0;
};

class StreamTextureImpl {
 public:
  ~StreamTextureImpl() {
    glDeleteTextures(1, &gl_tex_);
    gl_->eglDestroyImageKHR(gl_->display, image_);
    close(fd_);
    if (buffer_handler_ && handle_) {
      buffer_handler_->ReleaseBuffer(handle_);
      buffer_handler_->DestroyHandle(handle_);
    }
  }

  void *Map() {
    assert(addr_ == nullptr);
    size_t size = dimension_.stride * dimension_.height;
    addr_ = mmap(nullptr, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd_, 0);
    if (addr_ == MAP_FAILED)
      return nullptr;
    return addr_;
  }

  void Unmap() {
    assert(addr_ != nullptr);
    size_t size = dimension_.stride * dimension_.height;
    munmap(addr_, size);
    addr_ = nullptr;
  }

  GLuint GetTextureID() const {
    return gl_tex_;
  }

  Dimension GetDimension() const {
    return dimension_;
  }

  bool Initialize(hwcomposer::NativeBufferHandler *buffer_handler) {
    // bo_ = gbm_bo_create(gbm, dimension_.width, dimension_.height,
    //			GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR);
    buffer_handler_ = buffer_handler;
    if (!buffer_handler_->CreateBuffer(dimension_.width, dimension_.height,
                                       DRM_FORMAT_ARGB8888, &handle_)) {
      ETRACE("StreamTextureImpl: CreateBuffer failed");
      return false;
    }

    if (!buffer_handler_->ImportBuffer(handle_)) {
      ETRACE("StreamTextureImpl: ImportBuffer failed");
      return false;
    }

    dimension_.stride = handle_->meta_data_.pitches_[0];
    fd_ = handle_->meta_data_.prime_fd_;
    EGLint offset = 0;
    const EGLint khr_image_attrs[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT,     fd_,
        EGL_WIDTH,                     dimension_.width,
        EGL_HEIGHT,                    dimension_.height,
        EGL_LINUX_DRM_FOURCC_EXT,      DRM_FORMAT_ARGB8888,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  dimension_.stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
        EGL_NONE};

    image_ =
        gl_->eglCreateImageKHR(gl_->display, EGL_NO_CONTEXT,
                               EGL_LINUX_DMA_BUF_EXT, nullptr, khr_image_attrs);

    if (image_ == EGL_NO_IMAGE_KHR) {
      printf("failed to make image from buffer object: %x\n", glGetError());
      return false;
    }

    glGenTextures(1, &gl_tex_);
    glBindTexture(GL_TEXTURE_2D, gl_tex_);
    gl_->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
  }

  StreamTextureImpl(glContext *gl, size_t width, size_t height) {
    gl_ = gl;
    dimension_.width = width;
    dimension_.height = height;
  }

 private:
  glContext *gl_;
  HWCNativeHandle handle_;
  hwcomposer::NativeBufferHandler *buffer_handler_ = nullptr;
  int fd_ = -1;
  EGLImageKHR image_ = nullptr;
  GLuint gl_tex_ = 0;
  Dimension dimension_;
  void *addr_ = nullptr;
};

GLCubeLayerRenderer::GLCubeLayerRenderer(
    hwcomposer::NativeBufferHandler *buffer_handler, bool enable_texture)
    : GLLayerRenderer(buffer_handler) {
  enable_texture_ = enable_texture;
}

GLCubeLayerRenderer::~GLCubeLayerRenderer() {
}

bool GLCubeLayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                               uint32_t usage_format, uint32_t usage,
                               glContext *gl, const char *resource_path) {
  if (format != DRM_FORMAT_XRGB8888)
    return false;
  if (!GLLayerRenderer::Init(width, height, format, usage_format, usage, gl))
    return false;

  GLuint vertex_shader, fragment_shader;
  GLint ret;
  const char *vertex_shader_source = NULL;
  const char *fragment_shader_source = NULL;

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

  static const GLfloat vTexCoord[] = {
      // front
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      // back
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      // right
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      // left
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      // top
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      // bottom
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
  };

  static const char *cube_texture_vss =
      "uniform mat4 modelviewMatrix;      \n"
      "uniform mat4 modelviewprojectionMatrix;\n"
      "uniform mat3 normalMatrix;         \n"
      "                                   \n"
      "attribute vec4 in_position;        \n"
      "attribute vec3 in_normal;          \n"
      "attribute vec4 in_color;           \n"
      "attribute vec2 in_texCoord;        \n"
      "                                   \n"
      "vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "varying float vVaryingDiff;        \n"
      "varying vec2 vTexCoord;            \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    gl_Position = modelviewprojectionMatrix * in_position;\n"
      "    vec3 vEyeNormal = normalMatrix * in_normal;\n"
      "    vec4 vPosition4 = modelviewMatrix * in_position;\n"
      "    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
      "    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
      "    vVaryingDiff = max(0.0, dot(vEyeNormal, vLightDir));\n"
      "    vVaryingColor = in_color;      \n"
      "    vTexCoord = in_texCoord;       \n"
      "}                                  \n";

  static const char *cube_texture_fss =
      "precision mediump float;           \n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "varying float vVaryingDiff;        \n"
      "varying vec2 vTexCoord;            \n"
      "uniform sampler2D s_texture;       \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    vec4 color = vec4(texture2D(s_texture, vTexCoord).a * "
      "vVaryingColor.rgb, 1.0);\n"
      "    gl_FragColor = vec4(vVaryingDiff * color.rgb, 1.0);\n"
      "}                                  \n";

  static const char *cube_vss =
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

  static const char *cube_fss =
      "precision mediump float;           \n"
      "                                   \n"
      "varying vec4 vVaryingColor;        \n"
      "                                   \n"
      "void main()                        \n"
      "{                                  \n"
      "    gl_FragColor = vVaryingColor;  \n"
      "}                                  \n";

  if (enable_texture_) {
    vertex_shader_source = cube_texture_vss;
    fragment_shader_source = cube_texture_fss;
  } else {
    vertex_shader_source = cube_vss;
    fragment_shader_source = cube_fss;
  }

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

  program_ = glCreateProgram();

  glAttachShader(program_, vertex_shader);
  glAttachShader(program_, fragment_shader);

  glBindAttribLocation(program_, 0, "in_position");
  glBindAttribLocation(program_, 1, "in_normal");
  glBindAttribLocation(program_, 2, "in_color");
  if (enable_texture_) {
    glBindAttribLocation(program_, 3, "in_texCoord");
  }

  glLinkProgram(program_);

  glGetProgramiv(program_, GL_LINK_STATUS, &ret);
  if (!ret) {
    printf("program linking failed!:\n");
    GLint log_length;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_length);
    std::string program_log(log_length, ' ');

    if (log_length > 1) {
      glGetProgramInfoLog(program_, log_length, NULL,
                          (GLchar *)program_log.c_str());
      printf("%s", program_log.c_str());
    }

    return -1;
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glUseProgram(program_);

  modelviewmatrix_ = glGetUniformLocation(program_, "modelviewMatrix");
  modelviewprojectionmatrix_ =
      glGetUniformLocation(program_, "modelviewprojectionMatrix");
  normalmatrix_ = glGetUniformLocation(program_, "normalMatrix");

  GLuint samplerLoc = glGetUniformLocation(program_, "s_texture");
  glUniform1i(samplerLoc, 0);

  glViewport(0, 0, width, height);
  glEnable(GL_CULL_FACE);

  positionsoffset_ = 0;
  colorsoffset_ = sizeof(vVertices);
  normalsoffset_ = sizeof(vVertices) + sizeof(vColors);
  if (enable_texture_)
    texcoordoffset_ = sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  if (enable_texture_) {
    glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vColors) +
                                      sizeof(vNormals) + sizeof(vTexCoord),
                 0, GL_STATIC_DRAW);
  } else {
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0,
                 GL_STATIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, positionsoffset_, sizeof(vVertices),
                  &vVertices[0]);
  glBufferSubData(GL_ARRAY_BUFFER, colorsoffset_, sizeof(vColors), &vColors[0]);
  glBufferSubData(GL_ARRAY_BUFFER, normalsoffset_, sizeof(vNormals),
                  &vNormals[0]);
  if (enable_texture_)
    glBufferSubData(GL_ARRAY_BUFFER, texcoordoffset_, sizeof(vTexCoord),
                    &vTexCoord[0]);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) positionsoffset_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) normalsoffset_);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) colorsoffset_);
  glEnableVertexAttribArray(2);
  if (enable_texture_) {
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)(uintptr_t) texcoordoffset_);
    glEnableVertexAttribArray(3);
  }

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (enable_texture_) {
    stream_texture_ = new StreamTextureImpl(gl_, s_length, s_length);
    if (!stream_texture_)
      return false;

    stream_texture_->Initialize(buffer_handler_);
  }

  return true;
}

void GLCubeLayerRenderer::UpdateStreamTexture(unsigned long usec) {
  // 100% every 2 sec
  static const int interval = 2 * 1000000;
  float progress = 1.f * (usec % interval) / interval;

  int *ptr = (int *)stream_texture_->Map();
  assert(ptr);

  // Fill check pattern sliding to x axis as time goes on.
  Dimension dimension = stream_texture_->GetDimension();
  static const size_t byte_per_pixel = 4;
  int row_color[2][s_length] = {};
  std::memset(&row_color[0][0], 0, s_length * byte_per_pixel);
  std::memset(&row_color[1][0], -1, s_length * byte_per_pixel);
  static const size_t pattern_width = 64;
  for (size_t x = progress * pattern_width; x < s_length;) {
    assert(s_length >= x);
    size_t step = std::min(s_length - x, pattern_width) * byte_per_pixel;
    std::memset(&row_color[0][x], -1, step);
    std::memset(&row_color[1][x], 0, step);
    x += pattern_width * 2;
  }

  if (last_progress_ > progress)
    even_turn_ = !even_turn_;

  for (int y = 0; y < dimension.height; y++) {
    size_t index =
        (y % (2 * pattern_width) < pattern_width) ^ even_turn_ ? 0 : 1;
    std::copy(&row_color[index][0], &row_color[index][0] + s_length,
              &ptr[y * dimension.stride / byte_per_pixel]);
  }
  stream_texture_->Unmap();

  last_progress_ = progress;
}

void GLCubeLayerRenderer::glDrawFrame() {
  if (enable_texture_) {
    unsigned long usec = frame_count_ * 100000;
    static const int interval = 10000000.f;
    float progress = 1.f * (usec % interval) / interval;
    float red = pow(cos(M_PI * 2 * progress), 2) / 3;
    float green = pow(cos(M_PI * 2 * (progress + 0.33)), 2) / 3;
    float blue = pow(cos(M_PI * 2 * (progress + 0.66)), 2) / 3;
    glClearColor(red, green, blue, 1.0f);
    UpdateStreamTexture(usec);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stream_texture_->GetTextureID());
  } else {
    glClearColor(0.5, 0.5, 0.5, 1.0);
  }
  glClear(GL_COLOR_BUFFER_BIT);

  ESMatrix modelview;
  esMatrixLoadIdentity(&modelview);
  esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
  esRotate(&modelview, 45.0f + (0.25f * frame_count_), 1.0f, 0.0f, 0.0f);
  esRotate(&modelview, 45.0f - (0.5f * frame_count_), 0.0f, 1.0f, 0.0f);
  esRotate(&modelview, 10.0f + (0.15f * frame_count_), 0.0f, 0.0f, 1.0f);
  frame_count_++;

  GLfloat aspect = (GLfloat)(height_) / (GLfloat)(width_);

  ESMatrix projection;
  esMatrixLoadIdentity(&projection);
  esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f,
            10.0f);

  ESMatrix modelviewprojection;
  esMatrixLoadIdentity(&modelviewprojection);
  esMatrixMultiply(&modelviewprojection, &modelview, &projection);

  float normal[9] = {};
  normal[0] = modelview.m[0][0];
  normal[1] = modelview.m[0][1];
  normal[2] = modelview.m[0][2];
  normal[3] = modelview.m[1][0];
  normal[4] = modelview.m[1][1];
  normal[5] = modelview.m[1][2];
  normal[6] = modelview.m[2][0];
  normal[7] = modelview.m[2][1];
  normal[8] = modelview.m[2][2];

  glUniformMatrix4fv(modelviewmatrix_, 1, GL_FALSE, &modelview.m[0][0]);
  glUniformMatrix4fv(modelviewprojectionmatrix_, 1, GL_FALSE,
                     &modelviewprojection.m[0][0]);
  glUniformMatrix3fv(normalmatrix_, 1, GL_FALSE, normal);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
  glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
}
