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
#include <platformdefines.h>

#include <hardware/hwcomposer2.h>

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#ifndef GL_INTERFACE_HPP
#define GL_INTERFACE_HPP

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES 1
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

extern "C" {
#include "png.h"
}

#include <stdlib.h>

#ifndef GLAPIENTRYP
#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

#define GLAPIENTRYP GLAPIENTRY *
#endif

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

// temp definition until the real one is available

#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif

typedef void(GLAPIENTRYP PFNGLEGLIMAGERESOLVEOESINTEL)(GLenum target,
                                                       GLeglImageOES eglImage);

#define MAP_RED(x) (((dwColor & 0xFF000000) >> 24) * (1.0f / 255.0f))
#define MAP_GREEN(x) (((dwColor & 0x00FF0000) >> 16) * (1.0f / 255.0f))
#define MAP_BLUE(x) (((dwColor & 0x0000FF00) >> 8) * (1.0f / 255.0f))
#define MAP_ALPHA(x) (((dwColor & 0x000000FF)) * (1.0f / 255.0f))

namespace Hwch {

class PngImage;

class GlImage {
 public:
  png_uint_32 imWidth, imHeight;
  png_uint_32 glWidth, glHeight;
  int bit_depth, color_type;
  char* data;
  GLuint textureHandle;
  GLuint vboBuffer;
  GLint aPositionLocation;
  GLint aTextureCoordinateLocation;
  GLint aTextureUnitLocation;
  GLint aIgnoreColourLocation;
  GLint uProjMatrix;
  GLint aUseDiscardLocation;
  bool bDoneInit;

  GlImage() {
    imWidth = 0;
    imHeight = 0;
    glWidth = 0;
    glHeight = 0;
    bit_depth = 0;
    color_type = 0;
    data = NULL;
    textureHandle = 0;
    vboBuffer = 0;
    bDoneInit = false;
  }

  ~GlImage() {
    if (vboBuffer) {
      glDeleteBuffers(1, &vboBuffer);
      vboBuffer = 0;
    }

    if (textureHandle) {
      glDeleteTextures(1, &textureHandle);
      textureHandle = 0;
    }
  }
};

typedef GlImage* TexturePtr;
typedef void* BufferHandle;

enum TextureMode { None, Nearest, Bilinear };

class GlTargetPlatform {
 public:
  GlTargetPlatform();
  ~GlTargetPlatform();
  bool Initialize();
  bool Terminate(void);
  bool InitEGl(uint32_t screenWidth, uint32_t screenHeight);
  bool InitTarget(HWCNativeHandle buf);
  bool ReleaseTarget();
  void Clear(float r, float g, float b, float a, int x = -1, int y = -1,
             int w = -1, int h = -1);
  TexturePtr LoadTexture(PngImage& pngImage, TextureMode aMode = Nearest);
  void FreeTexture(TexturePtr& aTexture);
  bool ApplyTexture(TexturePtr aTexture, int x, int y, int w, int h,
                    bool bIgnore, float ignorer, float ignoreg, float ignoreb,
                    float ignorea);
  void Scissor(int x, int y, int w, int h);
  void DisableScissor();
  bool CompileShader(char* szShader);
  GLuint LoadShader(GLenum shaderType, const char* pSource);
  GLuint CreateProgram(const char* pVertexSource, const char* pFragmentSource);
  void FreeProgram(GLuint& program, GLuint vertexShader, GLuint pixelShader);

  void DrawLine(float x1, float y1, float x2, float y2, int lineWidth,
                float drawr, float drawg, float drawb, float drawa);
  bool StartFrame(void);
  bool EndFrame(void);
  bool Resolve(BufferHandle bh);

  bool CopySurface(void* data, uint32_t stride);

  int GetWidth(void) {
    return (m_surfaceWidth);
  };
  int GetHeight(void) {
    return (m_surfaceHeight);
  };

 private:
  GLbitfield m_clearMask;
  EGLDisplay m_display;
  EGLContext m_context;
  EGLSurface m_surface;
  EGLImageKHR m_eglImage;

  int m_surfaceWidth;
  int m_surfaceHeight;
  bool m_initComplete;

  int m_tX;
  int m_tY;
  int m_tW;
  int m_tH;

  GLfloat projectionMatrix[16];

  GLuint m_lineProgram;
  GLuint m_imageProgram;

  GLenum GetGlColorFormat(const int png_color_format);
  GLuint CreateTexture(const GLsizei width, const GLsizei height,
                       const GLenum type, const GLvoid* pixels,
                       TextureMode aMode);
  GLuint CreateVbo(const GLsizeiptr size, const GLvoid* data,
                   const GLenum usage);

  void loadOrthof(GLfloat* m, GLfloat l, GLfloat r, GLfloat b, GLfloat t,
                  GLfloat n, GLfloat f);
  void loadOrtho2Df(GLfloat* m, GLfloat l, GLfloat r, GLfloat b, GLfloat t);

  GLuint m_fbo;
  GLuint m_rtTextureID;
};

class GlInterface {
 public:
  // create render target of specified size, or use the screen size if no values
  // supplied
  GlInterface() {
    m_initComplete = false;
    m_aGlTargetPlatform.Initialize();
  }

  ~GlInterface() {
    m_aGlTargetPlatform.Terminate();
  }

  bool Init(void) {
    if (m_initComplete == false) {
      m_initComplete = m_aGlTargetPlatform.InitEGl(1, 1);
    }

    return (m_initComplete);
  }

  bool InitTarget(HWCNativeHandle buf) {
    return (m_aGlTargetPlatform.InitTarget(buf));
  }

  bool ReleaseTarget(void) {
    return (m_aGlTargetPlatform.ReleaseTarget());
  }

  bool Term(void) {
    return true;
  }

  // defaults to (0, 0, 0, 1);
  void SetClearColour(uint32_t dwColor) {
    m_clearR = MAP_RED(dwColor);
    m_clearG = MAP_GREEN(dwColor);
    m_clearB = MAP_BLUE(dwColor);
    m_clearA = MAP_ALPHA(dwColor);
  }

  // defaults to (1, 0, 0, 1);
  void SetDrawColour(uint32_t dwColor) {
    m_drawR = MAP_RED(dwColor);
    m_drawG = MAP_GREEN(dwColor);
    m_drawB = MAP_BLUE(dwColor);
    m_drawA = MAP_ALPHA(dwColor);
  }

  // defaults to (0, 0, 0, 1);
  void SetIgnoreColour(uint32_t dwColor) {
    m_ignoreR = MAP_RED(dwColor);
    m_ignoreG = MAP_GREEN(dwColor);
    m_ignoreB = MAP_BLUE(dwColor);
    m_ignoreA = MAP_ALPHA(dwColor);
  }

  // clears entire buffer with currently defined clear colour
  void Clear(void) {
    m_aGlTargetPlatform.Clear(m_clearR, m_clearG, m_clearB, m_clearA);
  }

  // clears specified region of buffer with currently defined clear colour
  void Clear(int x, int y, int w, int h) {
    m_aGlTargetPlatform.Clear(m_clearR, m_clearG, m_clearB, m_clearA, x, y, w,
                              h);
  }

  // draws a line from (x1, y1) to (x2, y2) using color set by SetColour
  // can specify line width or use default
  void DrawLine(float x1, float y1, float x2, float y2, int lineWidth = 1) {
    m_aGlTargetPlatform.DrawLine(x1, y1, x2, y2, lineWidth, m_drawR, m_drawG,
                                 m_drawB, m_drawA);
  }

  // Loads a texture and returns a pointer to the data or NULL
  TexturePtr LoadTexture(PngImage& pngImage, TextureMode dwMode = Nearest) {
    return (m_aGlTargetPlatform.LoadTexture(pngImage, dwMode));
  }

  // Free the texture specified
  void FreeTexture(TexturePtr aTexture) {
    m_aGlTargetPlatform.FreeTexture(aTexture);
  }

  // Applies the specified texture at the specified location, optionally can
  // ignore 'IgnoreColour' pixels
  bool ApplyTexture(TexturePtr aTexture, int x, int y, int w, int h,
                    bool bIgnore = false) {
    return (m_aGlTargetPlatform.ApplyTexture(aTexture, x, y, w, h, bIgnore,
                                             m_ignoreR, m_ignoreG, m_ignoreB,
                                             m_ignoreA));
  }

  void Scissor(int x, int y, int w, int h) {
    m_aGlTargetPlatform.Scissor(x, y, w, h);
  }

  void DisableScissor() {
    m_aGlTargetPlatform.DisableScissor();
  }

  bool StartFrame(void) {
    return (m_aGlTargetPlatform.StartFrame());
  }

  bool EndFrame(void) {
    return (m_aGlTargetPlatform.EndFrame());
  }

  // Return the handle of the currently in use buffer (ie the one we have been
  // or will be drawing to)
  BufferHandle GetBufferHandle(void) {
    return (NULL);
  }

  // Get the size of current RB
  int GetWidth(void) {
    return (m_aGlTargetPlatform.GetWidth());
  }

  int GetHeight(void) {
    return (m_aGlTargetPlatform.GetHeight());
  }

  bool ReadyToDraw(void) {
    return (m_initComplete);
  }

  bool Resolve(void) {
    return (m_aGlTargetPlatform.Resolve(GetBufferHandle()));
  }

  bool CopySurface(void* data, uint32_t stride) {
    return (m_aGlTargetPlatform.CopySurface(data, stride));
  }

 private:
  // clear colour
  float m_clearR, m_clearG, m_clearB, m_clearA;
  // drawing color
  float m_drawR, m_drawG, m_drawB, m_drawA;
  // ignore color
  float m_ignoreR, m_ignoreG, m_ignoreB, m_ignoreA;

  bool m_initComplete;

  GlTargetPlatform m_aGlTargetPlatform;
};
};

#endif
