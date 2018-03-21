/*
// Copyright (c) 2018 Intel Corporation
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

#include <cutils/memory.h>
#include <utils/threads.h>
#include <string>
#include <unistd.h>

#include <hardware/hwcomposer2.h>
#include <platformdefines.h>

#include "HwchDefs.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"

#include "HwchGlInterface.h"
#include "HwchPngImage.h"

using namespace Hwch;

const char gTextureVertexShader[] =
    "attribute vec4 vPosition;\n"
    "attribute vec2 a_TextureCoordinates;\n"
    "varying vec2   v_TextureCoordinates;\n"
    "uniform mat4   uProjectionMatrix;\n"
    "void main()\n"
    "{\n"
    "   v_TextureCoordinates = a_TextureCoordinates;\n"
    "   gl_Position = uProjectionMatrix * vPosition;\n"
    "}\n\n";

// Workaround for a GL bug we thought we had.
#ifdef HWCVAL_FRAGMENTSHADER_WORKAROUND
const char gTextureFragmentShader[] =
    "precision mediump float;\n"
    "uniform sampler2D u_TextureUnit;\n"
    "varying vec2 v_TextureCoordinates;\n"
    "void main()\n"
    "{\n"
    "   vec4 eps = vec4(0.009, 0.009, 0.009, 0.009);\n"
    "   vec4 pix = texture2D(u_TextureUnit, v_TextureCoordinates);\n"
    "   gl_FragColor = pix;\n"
    "}\n\n";
#else
const char gTextureFragmentShader[] =
    "precision mediump float;\n"
    "uniform sampler2D u_TextureUnit;\n"
    "uniform vec4 u_ignoreColour;\n"
    "uniform float u_useDiscard;\n"
    "varying vec2 v_TextureCoordinates;\n"
    "void main()\n"
    "{\n"
    "   vec4 eps = vec4(0.009, 0.009, 0.009, 0.009);\n"
    "   vec4 pix = texture2D(u_TextureUnit, v_TextureCoordinates);\n"
    "   if ( u_useDiscard > 0.5f && all(greaterThanEqual(pix, u_ignoreColour - "
    "eps)) && all(lessThanEqual(pix, u_ignoreColour + eps)) )\n"
    "   {\n"
    "       discard;\n"
    "   }\n"
    "   else\n"
    "   {\n"
    "       gl_FragColor = pix;\n"
    "   }\n"
    "}\n\n";
#endif

const char gLineVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4   uProjectionMatrix;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = uProjectionMatrix * vPosition;\n"
    "}\n\n";

const char gLineFragmentShader[] =
    "precision mediump float;\n"
    "uniform vec4 u_drawColour;\n"
    "void main()\n"
    "{\n"
    "   gl_FragColor = u_drawColour;\n"
    "}\n\n";

static void checkGlError(const char* op) {
  for (GLint error = glGetError(); error; error = glGetError()) {
    HWCERROR(eCheckGlFail, "after %s() glError (0x%x) => %d", op, error, error);
  }
}

static void checkEGLError(const char* op) {
  for (int error = (int)eglGetError(); error != EGL_SUCCESS;
       error = (int)eglGetError()) {
    if (error != EGL_SUCCESS) {
      HWCERROR(eCheckGlFail, "after %s() eglError (0x%x) => %d", op, error,
               error);
    }
  }
}

GlTargetPlatform::GlTargetPlatform() : m_lineProgram(0), m_imageProgram(0) {
  m_clearMask = GL_COLOR_BUFFER_BIT;

  m_display = EGL_NO_DISPLAY;
  m_context = EGL_NO_CONTEXT;
  m_surface = EGL_NO_SURFACE;

  m_surfaceWidth = 0;
  m_surfaceHeight = 0;
  m_initComplete = false;

  m_tX = m_tY = m_tH = m_tW = 0;

  m_fbo = 0;
  m_rtTextureID = 0;
}

GlTargetPlatform::~GlTargetPlatform() {
  Terminate();
}

bool GlTargetPlatform::Initialize(void) {
  bool rv = true;

  HWCLOGD_COND(eLogGl, "GlTargetPlatform::Initialize");

  if (m_display != EGL_NO_DISPLAY) {
    HWCLOGW("m_display is non null in GlTargetPlatform::Initialize");
    m_display = EGL_NO_DISPLAY;
  }

  if (m_surface != EGL_NO_SURFACE) {
    HWCLOGW("m_surface is non null in GlTargetPlatform::Initialize");
    m_surface = EGL_NO_SURFACE;
  }

  if (m_context != EGL_NO_CONTEXT) {
    HWCLOGW("m_context is non null in GlTargetPlatform::Initialize");
    m_context = EGL_NO_CONTEXT;
  }

  return (rv);
}

bool GlTargetPlatform::Terminate(void) {
  bool rv = false;
  HWCLOGD_COND(eLogGl, "GlTargetPlatform::Terminate");

  if (m_display != EGL_NO_DISPLAY) {
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_context != EGL_NO_CONTEXT) {
      HWCLOGD("Display %p: Destroying context %p", m_display, m_context);
      eglDestroyContext(m_display, m_context);
    }
    if (m_surface != EGL_NO_SURFACE) {
      eglDestroySurface(m_display, m_surface);
    }
    eglTerminate(m_display);
    eglReleaseThread();
  }

  m_display = EGL_NO_DISPLAY;
  m_context = EGL_NO_CONTEXT;
  m_surface = EGL_NO_SURFACE;

  return (rv);
}

bool GlTargetPlatform::StartFrame(void) {
  bool rv = true;

  HWCLOGD_COND(eLogGl,
               "GlTargetPlatform::StartFrame: x = %d y = %d w = %d h = %d",
               m_tX, m_tY, m_tW, m_tH);

  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  checkGlError("StartFrame - glBindFramebuffer(m_fbo)");

  GLuint rv2 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  checkGlError("glCheckFramebufferStatus");

  if (rv2 != GL_FRAMEBUFFER_COMPLETE) {
    HWCLOGD_COND(eLogGl, "check framebuffer status = %X %d", rv2, rv2);
  } else {
    HWCLOGD_COND(eLogGl, "Framebuffer ready");
  }

  loadOrtho2Df(projectionMatrix, m_tX, m_tX + m_tW, m_tY, m_tY + m_tH);

  return (rv);
}

bool GlTargetPlatform::EndFrame(void) {
  bool rv = true;
  HWCLOGD_COND(eLogGl, "GlTargetPlatform::EndFrame");

  glFinish();
  glFlush();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  checkGlError("EndFrame - glBindFramebuffer(0)");

  return (rv);
}

bool GlTargetPlatform::Resolve(BufferHandle bh) {
  ALOGE("Resolve support is missing \n");
  return true;
}

bool GlTargetPlatform::InitEGl(uint32_t screenWidth, uint32_t screenHeight) {
  bool rv = false;

  HWCLOGD_COND(eLogGl, "GlTargetPlatform::InitEGL");
  HWCLOGD_COND(eLogGl, "******: GlTargetPlatform::InitEGL - Entry");

  // initialize OpenGl ES and EGL

  /*
   * Here specify the attributes of the desired configuration.
   * Below, we select an EGLConfig with at least 8 bits per color
   * component compatible with on-screen windows
   */

  EGLint w = screenWidth, h = screenHeight;
  EGLint numConfigs;
  EGLConfig config;

  const EGLint attribs[] = {
      EGL_SURFACE_TYPE,   EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT, EGL_RED_SIZE,    8,
      EGL_GREEN_SIZE,     8,               EGL_BLUE_SIZE,
      8,                  EGL_ALPHA_SIZE,  8,
      EGL_DEPTH_SIZE,     16,              EGL_STENCIL_SIZE,
      8,                  EGL_NONE};

  EGLint pBufferattribs[] = {EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE};

  m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  checkEGLError("eglGetDisplay");
  HWCLOGD_COND(eLogGl, "eglGetDisplay() = %d", m_display);

  eglInitialize(m_display, 0, 0);
  checkEGLError("eglInitialize");

  /* Here, the application chooses the configuration it desires. In this
   * sample, we have a very simplified selection process, where we pick
   * the first EGLConfig that matches our criteria */
  eglChooseConfig(m_display, attribs, &config, 1, &numConfigs);
  checkEGLError("eglChooseCOnfig");

  m_surface = eglCreatePbufferSurface(m_display, config, pBufferattribs);
  checkEGLError("eglCreatePbufferSurface");

  const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  m_context =
      eglCreateContext(m_display, config, EGL_NO_CONTEXT, context_attribs);
  HWCLOGD_COND(eLogGl, "eglCreateContext geteglerror = %d Context=%p",
               eglGetError(), m_context);

  if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE) {
    HWCERROR(eCheckGlFail, "Unable to eglMakeCurrent");
    return false;
  }

  eglQuerySurface(m_display, m_surface, EGL_WIDTH, &w);
  eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &h);

  m_surfaceWidth = w;
  m_surfaceHeight = h;

  HWCLOGD_COND(eLogGl, "GlTargetPlatform::InitEGL width = %d height = %d", w,
               h);

  // Settings
  glViewport(0, 0, m_surfaceWidth, m_surfaceHeight);

  GLint error = glGetError();
  if (error != GL_NO_ERROR) {
    HWCLOGW("InitEGL failed status=%d", error);
  }

  rv = true;
  if (m_tW == -1) {
    m_tW = w;
  }

  if (m_tH == -1) {
    m_tH = h;
  }

  // audi:
  HWCLOGD_COND(eLogGl, "******: GlTargetPlatform::InitEGL - Exit ( rv = %d)",
               (int)rv);

  return (rv);
}

bool GlTargetPlatform::InitTarget(HWCNativeHandle buf) {
  bool rv = false;
  HWCLOGD_COND(eLogGl, "Init Target - Entry");

  m_surfaceWidth = m_tW = buf->meta_data_.width_;
  m_surfaceHeight = m_tH = buf->meta_data_.height_;

  glViewport(0, 0, m_surfaceWidth, m_surfaceHeight);

  uint32_t tgtFormat = buf->meta_data_.format_;

  HWCLOGD_COND(eLogGl, "surfacebuffer pixel fmt = %d", tgtFormat);
  const EGLint image_attrs[] = {
      EGL_WIDTH,
      static_cast<EGLint>(m_surfaceWidth),
      EGL_HEIGHT,
      static_cast<EGLint>(m_surfaceHeight),
      EGL_LINUX_DRM_FOURCC_EXT,
      static_cast<EGLint>(buf->meta_data_.format_),
      EGL_DMA_BUF_PLANE0_FD_EXT,
      static_cast<EGLint>(buf->meta_data_.prime_fds_[0]),
      EGL_DMA_BUF_PLANE0_PITCH_EXT,
      static_cast<EGLint>(buf->meta_data_.pitches_[0]),
      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
      0,
      EGL_NONE,
  };

  m_eglImage =
      eglCreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                        (EGLClientBuffer)NULL, image_attrs);

  checkEGLError("eglCreateImageKHR");

  if (eglGetError() == EGL_SUCCESS) {
    // create framebuffer
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    checkGlError("glGenFramebuffers");
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    checkGlError("glbindFramebuffer");
    m_fbo = framebuffer;

    glActiveTexture(GL_TEXTURE0);
    checkGlError("glActiveTexture");

    GLuint TextureHandle;
    glGenTextures(1, &TextureHandle);
    checkGlError("glGenTextures");
    glBindTexture(GL_TEXTURE_2D, TextureHandle);
    checkGlError("glBindTexture");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    checkGlError("gltexparametersi min - filter");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    checkGlError("gltexparameteri mag - filter");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    checkGlError("gltexparametersi clamp s");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    checkGlError("gltexparameteri clamp t");

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)m_eglImage);
    checkGlError("glEGLImageTargetTexture2DOES");

    // attach color attachment
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           TextureHandle, 0);
    checkGlError("glFramebufferTexture2D");
    m_rtTextureID = TextureHandle;

    GLuint rv2 = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    checkGlError("glCheckFramebufferStatus");

    if (rv2 != GL_FRAMEBUFFER_COMPLETE) {
      HWCLOGD_COND(eLogGl, "check framebuffer status = %X %d", rv2, rv2);
    } else {
      HWCLOGD_COND(eLogGl, "Framebuffer ready");
      rv = true;
    }
  }

  HWCLOGD_COND(eLogGl, "Init Target - Exit rv(%d)", (int)rv);
  return (rv);
}

bool GlTargetPlatform::ReleaseTarget(void) {
  bool rv = true;

  if (m_fbo) {
    glDeleteFramebuffers(1, &m_fbo);
    checkGlError("ReleaseTarget - glDeleteFramebuffers");
  }

  if (m_rtTextureID) {
    glDeleteTextures(1, &m_rtTextureID);
    checkGlError("ReleaseTarget - glDeletetextures");
  }

  if (m_eglImage) {
    eglDestroyImageKHR(m_display, m_eglImage);
    checkEGLError("ReleaseTarget - eglDestroyImageKHR");
  }

  return (rv);
}

void GlTargetPlatform::Clear(float r, float g, float b, float a, int x, int y,
                             int w, int h) {
  HWCLOGD_COND(
      eLogGl, "GlTargetPlatform::Clear r=%f g=%f b=%f a=%f x=%d y=%d w=%d h=%d",
      double(r), double(g), double(b), double(a), x, y, w, h);

  glClearColor(r, g, b, a);
  checkGlError("glclearcolor");

  if (x == -1 && y == -1) {
    glClear(m_clearMask);
    checkGlError("glclear");
  } else {
    glEnable(GL_SCISSOR_TEST);
    checkGlError("glenablescissor");
    glScissor(x, y, w, h);
    checkGlError("glscissor");
    glClear(m_clearMask);
    checkGlError("glclear");
    glDisable(GL_SCISSOR_TEST);
    checkGlError("gldisablescissor");
  }
}

TexturePtr GlTargetPlatform::LoadTexture(PngImage& pngImage,
                                         TextureMode aMode) {
  HWCLOGD_COND(eLogGl, "GlTargetPlatform::LoadTexture");

  TexturePtr texture = new GlImage();

  if (texture) {
    int width = pngImage.GetWidth();
    int height = pngImage.GetHeight();
    texture->color_type = pngImage.GetColorType();
    texture->bit_depth = pngImage.GetBitDepth();

    HWCLOGD_COND(eLogGl, "W = %d H = %d ct = %d bt = %d", width, height,
                 texture->color_type, texture->bit_depth);

    texture->glWidth = texture->imWidth = width;
    texture->glHeight = texture->imHeight = height;

    HWCLOGD_COND(eLogGl, "Texture width = %d height = %d", texture->glWidth,
                 texture->glHeight);

    texture->data = (char*)(pngImage.GetDataBlob());

    texture->textureHandle = CreateTexture(
        texture->glWidth, texture->glHeight,
        GetGlColorFormat(texture->color_type), texture->data, aMode);
    texture->data = 0;
    texture->bDoneInit = false;
  }

  return (texture);
}

bool GlTargetPlatform::ApplyTexture(TexturePtr aTexture, int x, int y, int w,
                                    int h, bool bIgnore, float ignorer,
                                    float ignoreg, float ignoreb,
                                    float ignorea) {
  bool rv = false;
  HWCLOGD_COND(eLogGl, "GlTargetPlatform::ApplyTexture");
  GLfloat rect[] = {(float)x,       (float)y,       0.0f, 0.0f,
                    (float)(x + w), (float)y,       0.0f, 1.0f,
                    (float)x,       (float)(y + h), 1.0f, 0.0f,
                    (float)(x + w), (float)(y + h), 1.0f, 1.0f};

  if (aTexture) {
    HWCLOGD_COND(eLogGl, "GlTargetPlatform::ApplyTexture aTexture is valid");
    if (aTexture->bDoneInit == false) {
      HWCLOGD_COND(
          eLogGl,
          "GlTargetPlatform::ApplyTexture initializing vbo and attributes");
      aTexture->vboBuffer = CreateVbo(sizeof(rect), rect, GL_STATIC_DRAW);

      aTexture->bDoneInit = true;
    }

    if (m_imageProgram == 0) {
      m_imageProgram =
          CreateProgram(gTextureVertexShader, gTextureFragmentShader);
      if (m_imageProgram) {
        HWCLOGD_COND(eLogGl, "CreateProgram succeeds for texture");
      } else {
        HWCERROR(eCheckGlFail, "CreateProgram fails for texture");
        return false;
      }
    }

    glUseProgram(m_imageProgram);
    checkGlError("useprogram");

    aTexture->aPositionLocation =
        glGetAttribLocation(m_imageProgram, "vPosition");
    checkGlError("glGetAttribLocation1");
    if (aTexture->aPositionLocation < 0) {
      HWCERROR(eCheckGlFail,
               "ApplyTexture: No location for vPosition attribute");
      return false;
    }

    aTexture->aTextureCoordinateLocation =
        glGetAttribLocation(m_imageProgram, "a_TextureCoordinates");
    checkGlError("glGetAttribLocation2");
    if (aTexture->aTextureCoordinateLocation < 0) {
      HWCERROR(eCheckGlFail,
               "ApplyTexture: No location for a_TextureCoordinates attribute");
      return false;
    }

    aTexture->aTextureUnitLocation =
        glGetUniformLocation(m_imageProgram, "u_TextureUnit");
    checkGlError("glGetUniformLocation");
    if (aTexture->aTextureUnitLocation < 0) {
      HWCERROR(eCheckGlFail, "ApplyTexture: u_TextureUnit not found");
      return false;
    }

    aTexture->uProjMatrix =
        glGetUniformLocation(m_imageProgram, "uProjectionMatrix");
    checkGlError("glGetUniformLocation2");
    if (aTexture->uProjMatrix < 0) {
      HWCERROR(eCheckGlFail, "ApplyTexture: u_ProjectionMatrix not found");
      return false;
    }

#ifndef HWCVAL_FRAGMENTSHADER_WORKAROUND
    aTexture->aIgnoreColourLocation =
        glGetUniformLocation(m_imageProgram, "u_ignoreColour");
    checkGlError("glGetUniformLocation");
    if (aTexture->aIgnoreColourLocation < 0) {
      HWCERROR(eCheckGlFail, "ApplyTexture: u_ignoreColour not found");
      return false;
    }

    aTexture->aUseDiscardLocation =
        glGetUniformLocation(m_imageProgram, "u_useDiscard");
    checkGlError("glGetUniformLocation");
    if (aTexture->aUseDiscardLocation < 0) {
      HWCERROR(eCheckGlFail, "ApplyTexture: u_useDiscard not found");
      return false;
    }
#endif

    HWCLOGD_COND(eLogGl, "aPositionLocation = %d", aTexture->aPositionLocation);
    HWCLOGD_COND(eLogGl, "aTextureCoordinateLocation = %d",
                 aTexture->aTextureCoordinateLocation);
    HWCLOGD_COND(eLogGl, "aTextureUnitLocation = %d",
                 aTexture->aTextureUnitLocation);
    HWCLOGD_COND(eLogGl, "uProjMatrix = %d", aTexture->uProjMatrix);

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    checkGlError("glActiveTexture");

    glBindTexture(GL_TEXTURE_2D, aTexture->textureHandle);
    checkGlError("glBindTexture");
    HWCLOGD_COND(eLogGl, "textureHandle = %d", aTexture->textureHandle);

    glUniform1i(aTexture->aTextureUnitLocation, 0);
    checkGlError("glUniform1i");

    glUniform4f(aTexture->aIgnoreColourLocation, ignorer, ignoreg, ignoreb,
                ignorea);
    checkGlError("glUniform4f");

    glUniformMatrix4fv(aTexture->uProjMatrix, 1, GL_FALSE, projectionMatrix);

    glUniform1f(aTexture->aUseDiscardLocation, bIgnore ? 1.0f : 0.0f);
    checkGlError("glUniform1f");

    glBindBuffer(GL_ARRAY_BUFFER, aTexture->vboBuffer);
    checkGlError("glBindBuffer");

    glVertexAttribPointer(aTexture->aPositionLocation, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GL_FLOAT), BUFFER_OFFSET(0));
    checkGlError("glVertexAttribPointer1");

    glVertexAttribPointer(aTexture->aTextureCoordinateLocation, 2, GL_FLOAT,
                          GL_FALSE, 4 * sizeof(GL_FLOAT),
                          BUFFER_OFFSET(2 * sizeof(GL_FLOAT)));
    checkGlError("glVertexAttribPointer2");

    glEnableVertexAttribArray(aTexture->aPositionLocation);
    checkGlError("glEnableVertexAttribArray1");

    glEnableVertexAttribArray(aTexture->aTextureCoordinateLocation);
    checkGlError("glEnableVertexAttribArray2");

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    checkGlError("glDrawArrays(trianglestrip)");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    checkGlError("glBindBuffer2");
  }
  return (rv);
}

void GlTargetPlatform::FreeTexture(TexturePtr& aTexture) {
  if (aTexture) {
    delete aTexture;
    aTexture = NULL;
  }
}

void GlTargetPlatform::Scissor(int x, int y, int w, int h) {
  glScissor(x, y, w, h);
  checkGlError("glScissor");

  glEnable(GL_SCISSOR_TEST);
  checkGlError("glEnable(GL_SCISSOR_TEST)");
}

void GlTargetPlatform::DisableScissor() {
  glDisable(GL_SCISSOR_TEST);
  checkGlError("glDisable(GL_SCISSOR_TEST)");
}

void GetShaderInfo(GLint shader, const char* szData) {
  GLint infoLen = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
  if (infoLen) {
    char* buf = (char*)malloc(infoLen);
    if (buf) {
      glGetShaderInfoLog(shader, infoLen, NULL, buf);
      HWCLOGI("[%s] Error shader:len=%d [%s]", szData, infoLen, buf);
      free(buf);
    }
  }
}

GLuint GlTargetPlatform::LoadShader(GLenum shaderType, const char* pSource) {
  GLuint shader = glCreateShader(shaderType);
  checkGlError("createshader");
  if (shader) {
    glShaderSource(shader, 1, &pSource, NULL);
    checkGlError("shadersource");
    GetShaderInfo(shader, "glshadersource");
    glCompileShader(shader);
    checkGlError("compileshader");
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      HWCERROR(eCheckGlFail, "compile shader failed");
      GetShaderInfo(shader, "glcompileshader");
      glDeleteShader(shader);
      shader = 0;
    } else {
      HWCLOGD_COND(eLogGl, "compile shader SUCCESS");
    }
  } else {
    HWCERROR(eCheckGlFail, "create shader failed");
  }
  return shader;
}

GLuint GlTargetPlatform::CreateProgram(const char* pVertexSource,
                                       const char* pFragmentSource) {
  GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexSource);
  if (!vertexShader) {
    HWCERROR(eCheckGlFail, "Failed to load vertex shader");
    return 0;
  }

  GLuint pixelShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentSource);
  if (!pixelShader) {
    HWCERROR(eCheckGlFail, "Failed to load pixel shader");
    return 0;
  }

  GLuint program = glCreateProgram();
  if (program) {
    glAttachShader(program, vertexShader);
    checkGlError("glAttachShader");

    glAttachShader(program, pixelShader);
    checkGlError("glAttachShader");

    glBindAttribLocation(program, 0, "vPosition");
    checkGlError("glBindAttribLocation");

    glLinkProgram(program);
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
      GLint bufLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
      if (bufLength) {
        char* buf = (char*)malloc(bufLength);
        if (buf) {
          glGetProgramInfoLog(program, bufLength, NULL, buf);
          HWCERROR(eCheckGlFail, "Could not link program: %s", buf);
          free(buf);
        }
      }

      FreeProgram(program, vertexShader, pixelShader);
    }
  } else {
    FreeProgram(program, vertexShader, pixelShader);
  }

  return program;
}

void GlTargetPlatform::FreeProgram(GLuint& program, GLuint vertexShader,
                                   GLuint pixelShader) {
  if (vertexShader) {
    glDeleteShader(vertexShader);
    vertexShader = 0;
    checkGlError("FreeProgram - DeleteShader - vertexShader");
  }

  if (pixelShader) {
    glDeleteShader(pixelShader);
    pixelShader = 0;
    checkGlError("FreeProgram - DeleteShader - pixelShader");
  }

  if (program) {
    glDeleteProgram(program);
    program = 0;
    checkGlError("FreeProgram - DeleteProgram - program");
  }
}

void GlTargetPlatform::DrawLine(float x1, float y1, float x2, float y2,
                                int lineWidth, float drawr, float drawg,
                                float drawb, float drawa) {
  HWCLOGD_COND(eLogGl,
               "GlTargetPlatform::DrawLine (%f, %f) -> (%f, %f): width=%d "
               "color = (%f, %f, %f, %f)",
               double(x1), double(y1), double(x2), double(y2), lineWidth,
               double(drawr), double(drawg), double(drawb), double(drawa));

  GLfloat line[] = {
      x1, y1, x2, y2,
  };

  if (m_lineProgram == 0) {
    m_lineProgram = CreateProgram(gLineVertexShader, gLineFragmentShader);
    if (m_lineProgram) {
      HWCLOGD_COND(eLogGl, "CreateProgram succeeds for line");
    } else {
      HWCERROR(eCheckGlFail, "CreateProgram fails for line");
      return;
    }
  }

  glUseProgram(m_lineProgram);
  checkGlError("useprogram line");

  GLuint uProjMatrix = glGetUniformLocation(m_lineProgram, "uProjectionMatrix");
  checkGlError("glGetUniformLocation2");
  glUniformMatrix4fv(uProjMatrix, 1, GL_FALSE, projectionMatrix);

  GLuint uDrawColourLocation =
      glGetUniformLocation(m_lineProgram, "u_drawColour");
  checkGlError("glGetUniformLocation");
  glUniform4f(uDrawColourLocation, drawr, drawg, drawb, drawa);
  checkGlError("glUniform4f");

  GLint aPositionLocation = glGetAttribLocation(m_lineProgram, "vPosition");
  checkGlError("glGetAttribLocation1");
  if (aPositionLocation < 0) {
    HWCERROR(eCheckGlFail, "DrawLine: No location for vPosition attribute");
    return;
  }

  glVertexAttribPointer(aPositionLocation, 2, GL_FLOAT, GL_FALSE, 0, line);
  checkGlError("glVertexAttribPointer");

  glEnableVertexAttribArray(aPositionLocation);
  checkGlError("glEnableVertexAttribArray");

  glLineWidth(lineWidth);
  glDrawArrays(GL_LINES, 0, 2);  // Line 0, 2
  checkGlError("glDrawArrays(lines)");

  glDisableVertexAttribArray(aPositionLocation);
  checkGlError("glDisableVertexAttribArray");
}

GLenum GlTargetPlatform::GetGlColorFormat(const int png_color_format) {
  switch (png_color_format) {
    case PNG_COLOR_TYPE_GRAY:
      return GL_LUMINANCE;
    case PNG_COLOR_TYPE_RGB_ALPHA:
      return GL_RGBA;
    case PNG_COLOR_TYPE_RGB:
      return GL_RGB;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
      return GL_LUMINANCE_ALPHA;
  }
  return 0;
}

GLuint GlTargetPlatform::CreateTexture(const GLsizei width,
                                       const GLsizei height, const GLenum type,
                                       const GLvoid* pixels,
                                       TextureMode aMode) {
  GLuint texture_object_id;

  HWCLOGD_COND(eLogGl, "BindTexture: width=%d height=%d type=%d pixels=%p",
               width, height, type, pixels);

  glGenTextures(1, &texture_object_id);
  checkGlError("glGenTextures");

  glBindTexture(GL_TEXTURE_2D, texture_object_id);
  checkGlError("glBindTexture");

  if (aMode == Nearest) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    checkGlError("glTexParameteri1");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    checkGlError("glTexParameteri2");
    HWCLOGD_COND(eLogGl, "Configuring for nearest mode");
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    checkGlError("glTexParameteri1");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    checkGlError("glTexParameteri2");
    HWCLOGD_COND(eLogGl, "Configuring for linear mode");
  }

  glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0, type, GL_UNSIGNED_BYTE,
               pixels);
  checkGlError("glTexImage2D");

  glBindTexture(GL_TEXTURE_2D, 0);
  checkGlError("glBindTexture2");

  return texture_object_id;
}

GLuint GlTargetPlatform::CreateVbo(const GLsizeiptr size, const GLvoid* data,
                                   const GLenum usage) {
  GLuint vboBuffer;
  glGenBuffers(1, &vboBuffer);
  checkGlError("glGenBuffers");

  glBindBuffer(GL_ARRAY_BUFFER, vboBuffer);
  checkGlError("glBindBuffer");
  glBufferData(GL_ARRAY_BUFFER, size, data, usage);
  checkGlError("glBufferData");
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  checkGlError("glBindBuffer2");

  return vboBuffer;
}

void GlTargetPlatform::loadOrthof(GLfloat* m, GLfloat l, GLfloat r, GLfloat b,
                                  GLfloat t, GLfloat n, GLfloat f) {
  m[0] = 2.0f / (r - l);
  m[1] = 0.0f;
  m[2] = 0.0f;
  m[3] = 0.0f;

  m[4] = 0.0f;
  m[5] = 2.0f / (t - b);
  m[6] = 0.0f;
  m[7] = 0.0f;

  m[8] = 0.0f;
  m[9] = 0.0f;
  m[10] = -2.0f / (f - n);
  m[11] = 0.0f;

  m[12] = -(r + l) / (r - l);
  m[13] = -(t + b) / (t - b);
  m[14] = -(f + n) / (f - n);
  m[15] = 1.0f;
}

void GlTargetPlatform::loadOrtho2Df(GLfloat* m, GLfloat l, GLfloat r, GLfloat b,
                                    GLfloat t) {
  loadOrthof(m, l, r, t, b, -1.0f, 1.0f);
}

bool GlTargetPlatform::CopySurface(void* data, uint32_t /*stride*/) {
  bool rv = false;

  glReadPixels(0, 0, m_surfaceWidth, m_surfaceHeight, GL_RGBA, GL_UNSIGNED_BYTE,
               data);
  checkGlError("glReadPixels");

  return (rv);
}
