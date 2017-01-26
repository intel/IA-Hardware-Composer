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

GLCubeLayerRenderer::GLCubeLayerRenderer(struct gbm_device *dev)
    : GLLayerRenderer(dev) {
}

GLCubeLayerRenderer::~GLCubeLayerRenderer() {
}

bool GLCubeLayerRenderer::Init(uint32_t width, uint32_t height, uint32_t format,
                               glContext *gl, const char *resource_path) {
  if (format != GBM_FORMAT_XRGB8888)
    return false;
  if (!GLLayerRenderer::Init(width, height, format, gl))
    return false;

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

  glUseProgram(program_);

  modelviewmatrix_ = glGetUniformLocation(program_, "modelviewMatrix");
  modelviewprojectionmatrix_ =
      glGetUniformLocation(program_, "modelviewprojectionMatrix");
  normalmatrix_ = glGetUniformLocation(program_, "normalMatrix");

  glViewport(0, 0, width, height);
  glEnable(GL_CULL_FACE);

  positionsoffset_ = 0;
  colorsoffset_ = sizeof(vVertices);
  normalsoffset_ = sizeof(vVertices) + sizeof(vColors);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, positionsoffset_, sizeof(vVertices),
                  &vVertices[0]);
  glBufferSubData(GL_ARRAY_BUFFER, colorsoffset_, sizeof(vColors), &vColors[0]);
  glBufferSubData(GL_ARRAY_BUFFER, normalsoffset_, sizeof(vNormals),
                  &vNormals[0]);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) positionsoffset_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) normalsoffset_);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0,
                        (const GLvoid *)(uintptr_t) colorsoffset_);
  glEnableVertexAttribArray(2);
  return true;
}

void GLCubeLayerRenderer::glDrawFrame() {
  ESMatrix modelview;

  /* clear the color buffer */
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  esMatrixLoadIdentity(&modelview);
  esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
  esRotate(&modelview, 45.0f + (0.25f * frame_count_), 1.0f, 0.0f, 0.0f);
  esRotate(&modelview, 45.0f - (0.5f * frame_count_), 0.0f, 1.0f, 0.0f);
  esRotate(&modelview, 10.0f + (0.15f * frame_count_), 0.0f, 0.0f, 1.0f);

  frame_count_++;
  uint32_t height = native_handle_.import_data.height;
  uint32_t width = native_handle_.import_data.width;

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
