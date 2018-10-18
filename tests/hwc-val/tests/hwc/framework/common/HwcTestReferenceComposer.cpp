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

#include <stdio.h>
#include <math.h>
#include "HwcTestReferenceComposer.h"
#include "HwcTestKernel.h"

#include <string>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include "HwcTestState.h"
#include "HwcTestUtil.h"

#define PRINTF_SHADERS 0
#define PRINTF_GLFLUSH 0
#define SINGLE_TRIANGLE 0
#define CREATEDESTROY_ONCE 0
#define COMPOSITION_DEBUG 0

using namespace android;
extern int fd;
static HwcTestReferenceComposer *spRefCmp = 0;

bool HwcTestReferenceComposer::verifyContextCreated() {
  // Save the current GL context
  GLContextSaver contextSaver(this);

  // Try to create the GL context, if it is not created already
  if (!isCreated()) {
    if (lazyCreate()) {
      // If the context does not exist and could not be created then
      // no format is supported
      return false;
    }
  }

  return true;
}

bool HwcTestReferenceComposer::isFormatSupportedAsOutput(int32_t format) {
  /*
  This method was moved to be a member since its querying the context
  now to check that the extension is supported.
  You'll need to change it's declaration as well.
  */

  // verify context is created before accessing members
  if (!verifyContextCreated()) {
    return false;
  }

  switch (format) {
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGB565:
      return true;
    case DRM_FORMAT_NV12_Y_TILED_INTEL:
    case DRM_FORMAT_NV12:
      return m_nv12TargetSupported;

    default:
      return false;
  }
}

/// Helper to check the GL status and log errors when found.

bool HwcTestReferenceComposer::getGLError(const char *operation) {
  GLint error = glGetError();
  if (error != GL_NO_ERROR) {
    HWCLOGW("HwcTestReferenceComposer: Error 0x%x on %s", error, operation);
    spRefCmp->mErrorOccurred = true;
    return true;
  }

  return false;
}

/// Helper to check the EGL status and log errors when found.

bool HwcTestReferenceComposer::getEGLError(const char *operation) {
  GLint error = eglGetError();
  if (error != EGL_SUCCESS) {
    HWCLOGW("HwcTestReferenceComposer: Error 0x%x on %s", error, operation);
    mErrorOccurred = true;
    return true;
  }

  return false;
}

HwcTestReferenceComposer::GLContextSaver::GLContextSaver(
    HwcTestReferenceComposer *refCmp)
    : mRefCmp(refCmp) {
  mPrevDisplay = eglGetCurrentDisplay();
  refCmp->getEGLError("eglGetCurrentDisplay");

  mPrevDrawSurface = eglGetCurrentSurface(EGL_DRAW);
  refCmp->getEGLError("eglGetCurrentSurface");

  mPrevReadSurface = eglGetCurrentSurface(EGL_READ);
  refCmp->getEGLError("eglGetCurrentSurface");

  mPrevContext = eglGetCurrentContext();
  refCmp->getEGLError("eglGetCurrentContext");

  m_saved = true;
}

HwcTestReferenceComposer::GLContextSaver::~GLContextSaver() {
  if (m_saved && mPrevContext != EGL_NO_CONTEXT) {
    eglMakeCurrent(mPrevDisplay, mPrevDrawSurface, mPrevReadSurface,
                   mPrevContext);
    mRefCmp->getEGLError("eglMakeCurrent ~GLContextSaver");
  } else {
    eglMakeCurrent(mRefCmp->m_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    mRefCmp->getEGLError("eglMakeCurrent ~GLContextSaver (2)");
  }
}

HwcTestReferenceComposer::CShader::CShader() : m_isIdValid(false) {
}

HwcTestReferenceComposer::CShader::~CShader() {
  destroy();
}

bool HwcTestReferenceComposer::CShader::lazyCreate(GLenum shaderType,
                                                   const char *source) {
  m_id = glCreateShader(shaderType);

  if (getGLError("glCreateShader")) {
    destroy();
    return false;
  }

  m_isIdValid = true;

  // The object creation has finished and we set it up with some values
  glShaderSource(m_id, 1, &source, NULL);
  if (getGLError("glShaderSource")) {
    destroy();
    return false;
  }

  glCompileShader(m_id);
  if (getGLError("glCompileShader")) {
    destroy();
    return false;
  }

  GLint compiledStatus = 0;

  glGetShaderiv(m_id, GL_COMPILE_STATUS, &compiledStatus);
  if (getGLError("glGetShaderiv") || compiledStatus != GL_TRUE) {
    char buffer[1000];

    // Show the shader compilation errors
    const char *description = "Description not available";

    glGetShaderInfoLog(m_id, sizeof(buffer), NULL, buffer);
    if (!getGLError("glGetShaderInfoLog")) {
      description = buffer;
    }

    HWCERROR(
        eCheckGlFail,
        "HwcTestReferenceComposer: Error on shader compilation: %s. \n%s\n",
        description, source);

    destroy();
    return false;
  }

  return true;
}

/* Use one of the mandatory resources created on the lazy constructor as a
 marker
 telling us whether the the instance is fully created */

bool HwcTestReferenceComposer::CShader::isCreated() const {
  return m_isIdValid;
}

void HwcTestReferenceComposer::CShader::destroy() {
  if (isCreated()) {
    glDeleteShader(m_id);
    getGLError("glDeleteShader");

    m_isIdValid = false;
  }
}

GLuint HwcTestReferenceComposer::CShader::getId() const {
  return m_id;
}

HwcTestReferenceComposer::CProgram::CProgram() : m_isIdValid(false) {
}

HwcTestReferenceComposer::CProgram::~CProgram() {
  destroy();
}

/** \brief Link several shaders to produce a ready to use program

Create a new program, attach some shaders and link. If the whole
sequence was successful the id of the newly created program replaces the
old one. The program id becomes 0 otherwise.
*/

bool HwcTestReferenceComposer::CProgram::lazyCreate(unsigned int numShaders,
                                                    ...) {
  m_id = glCreateProgram();
  if (getGLError("glCreateProgram")) {
    destroy();
    return false;
  }

  m_isIdValid = true;

  // The object creation has finished and we set it up with some values

  // Attach the shaders

  unsigned int index;

  va_list arguments;
  va_start(arguments, numShaders);

  for (index = 0; index < numShaders; ++index) {
    const CShader *shader = va_arg(arguments, const CShader *);

    glAttachShader(m_id, shader->getId());
    if (getGLError("glAttachShader")) {
      destroy();
      return false;
    }
  }

  va_end(arguments);

  // Link the program

  glLinkProgram(m_id);
  if (getGLError("glLinkProgram")) {
    destroy();
    return false;
  }

  GLint linkStatus = GL_FALSE;

  glGetProgramiv(m_id, GL_LINK_STATUS, &linkStatus);
  if (getGLError("glGetProgramiv") || linkStatus != GL_TRUE) {
    char buffer[1000];

    // Show the shader compilation errors
    const char *description = "Description not available";

    glGetProgramInfoLog(m_id, sizeof(buffer), NULL, buffer);
    if (!getGLError("glGetProgramInfoLog")) {
      description = buffer;
    }

    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: Error on program linkage: %s.",
             description);

    destroy();
    return false;
  }

  return true;
}

bool HwcTestReferenceComposer::CProgram::isCreated() const {
  return m_isIdValid;
}

void HwcTestReferenceComposer::CProgram::destroy() {
  if (isCreated()) {
    glDeleteProgram(m_id);
    getGLError("glDeleteProgram");

    m_isIdValid = false;
  }
}

GLuint HwcTestReferenceComposer::CProgram::getId() const {
  return m_id;
}

bool HwcTestReferenceComposer::CProgram::use() {
  bool result;

  glUseProgram(m_id);
  if (getGLError("glUseProgram")) {
    result = false;
  } else {
    result = true;
  }

  return result;
}

HwcTestReferenceComposer::CProgramStore::CProgramStore() {
  // No program is currently set
  m_current = 0;
}

HwcTestReferenceComposer::CProgramStore::~CProgramStore() {
  destroy();
}

void HwcTestReferenceComposer::CProgramStore::destroy() {
  for (uint32_t i = 0; i < 2; ++i) {
    for (uint32_t j = 0; j < 2; ++j) {
      for (uint32_t k = 0; k < 2; ++k) {
        mPrograms[i][j][k].destroy();
      }
    }
  }

  m_current = 0;
}

bool HwcTestReferenceComposer::CProgramStore::bind(uint32_t planeAlpha,
                                                   bool destIsNV12, bool opaque,
                                                   bool preMult) {
  bool result = false;
  float scaledPlaneAlpha = planeAlpha / 255.f;

  HWCLOGV_COND(eLogHarness,
               "HwcTestReferenceComposer::bind planeAlpha %d %s %s %s",
               planeAlpha, destIsNV12 ? "NV12" : "Not NV12",
               opaque ? "OPAQUE" : "BLEND", preMult ? "PREMULT" : "NOPREMULT");

  CRendererProgram &program =
      mPrograms[destIsNV12 ? 1 : 0][preMult ? 1 : 0][opaque ? 1 : 0];

  if (program.isCreated() ||
      lazyCreateProgram(&program, 1, opaque, preMult, destIsNV12)) {
    if (program.use()) {
      if (program.setPlaneAlphaUniform(scaledPlaneAlpha)) {
        m_current = &program;
        result = true;
      }
    }
  }

  return result;
}

GLint HwcTestReferenceComposer::CProgramStore::getPositionVertexIn() const {
  GLint result = 0;
  if (m_current) {
    result = m_current->getPositionVertexIn();
  }
  return result;
}

GLint HwcTestReferenceComposer::CProgramStore::getTexCoordVertexIn() const {
  GLint result = 0;
  if (m_current) {
    result = m_current->getTexCoordVertexIn();
  }
  return result;
}

HwcTestReferenceComposer::CProgramStore::CRendererProgram::CRendererProgram()
    : m_vinPosition(0) {
  // Setup the per-plane data
  m_vinTexCoord = 0;
  m_uPlaneAlpha = 0;
}

HwcTestReferenceComposer::CProgramStore::CRendererProgram::~CRendererProgram() {
  destroy();
}

// lazyCreateProgram function copied DIRECTLY from GlCellComposer with only
// references
// to GlCellComposer removed.
//
// HERE numLayers will ALWAYS be 1.
// Hence opaqueLayerMask and premultLayerMask are just booleans for us.
// This gives us just 2*2*2=8 possible programs.
//
bool HwcTestReferenceComposer::CProgramStore::lazyCreateProgram(
    CProgramStore::CRendererProgram *program, uint32_t numLayers,
    uint32_t opaqueLayerMask, uint32_t premultLayerMask, bool renderToNV12) {
  bool result = false;

  CShader vertexShader;

  String8 vertexShaderSource;

  if (numLayers) {
    // Multiple layers
    static const char vertexShaderFormat[] =
        "#version 300 es\n"
        "in mediump vec2 vinPosition;\n"
        "%s"
        "\n"
        "out mediump vec2 finTexCoords[%d];\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(vinPosition.x, vinPosition.y, 0, 1);\n"
        "%s"
        "}";

    static const char texCoordDeclarationFormat[] =
        "in mediump vec2 vinTexCoords%d;\n";
    static const char texCoordSetupFormat[] =
        "    finTexCoords[%d] = vinTexCoords%d;\n";

    String8 texCoordDeclarationBlock;
    String8 texCoordSetupBlock;
    for (uint32_t i = 0; i < numLayers; ++i) {
      texCoordDeclarationBlock += String8::format(texCoordDeclarationFormat, i);
      texCoordSetupBlock += String8::format(texCoordSetupFormat, i, i);
    }

    vertexShaderSource =
        String8::format(vertexShaderFormat, texCoordDeclarationBlock.c_str(),
                        numLayers, texCoordSetupBlock.c_str());
  } else {
    vertexShaderSource =
        "#version 300 es\n"
        "in mediump vec2 vinPosition;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(vinPosition.x, vinPosition.y, 0, 1);\n"
        "}";
  }
  ALOGD_IF(COMPOSITION_DEBUG, "\nVertex Shader:\n%s\n",
           vertexShaderSource.c_str());

  if (!vertexShader.lazyCreate(GL_VERTEX_SHADER, vertexShaderSource)) {
    ALOGE("Error on \"composite\" vertex shader creation");
  } else {
    HwcTestReferenceComposer::CShader fragmentShader;

    String8 fragmentShaderSource;

    // Additional output declarations for NV12
    static const char fragmentShaderNV12OutputDecls[] =
        "#extension GL_EXT_YUV_target : require\n"
        "layout(yuv) ";  // will add before "out vec4 cOut;\n"

    if (numLayers) {
      // Final Color Conversion for NV12
      static const char fragmentShaderNV12OutputConversion[] =
          "    vec3 yuvColor = rgb_2_yuv(outColor.xyz, itu_601);\n"
          "    outColor = vec4(yuvColor.xyz, outColor.w);\n";

      // Fragment shader main body
      static const char fragmentShaderFormat[] =
          "#version 300 es\n"
          "#extension GL_OES_EGL_image_external : require\n"
          "%sout mediump vec4 outColor;\n"  // Output Decls
          "\n"
          "uniform mediump sampler2D uTexture[%d];\n"
          "uniform mediump float uPlaneAlpha[%d];\n"
          "\n"
          "in mediump vec2 finTexCoords[%d];\n"
          "\n"
          "void main()\n"
          "{\n"
          "    mediump vec4 incoming;\n"
          "    mediump float planeAlpha;\n"
          "%s"  // Layers
          "%s"  // Output conversion
          "}";

      // Sample the given texture and get it's plane-alpha
      static const char blendingFormatSample[] =
          "    incoming = texture(uTexture[%d], finTexCoords[%d]);\n"
          "    planeAlpha = uPlaneAlpha[%d];\n";

      // Apply the plane alpha differently for premult and coverage
      static const char blendingFormatPremultPlaneAlpha[] =
          "    incoming = incoming * planeAlpha;\n";

      static const char blendingFormatCoveragePlaneAlpha[] =
          "    incoming.a = incoming.a * planeAlpha;\n";

      // Apply the plane alpha for opaque surfaces (slightly more optimally)
      static const char blendingFormatOpaquePremultPlaneAlpha[] =
          "    incoming.rgb = incoming.rgb * planeAlpha;\n"
          "    incoming.a = planeAlpha;\n";

      static const char blendingFormatOpaqueCoveragePlaneAlpha[] =
          "    incoming.a = planeAlpha;\n";

      // Note: SurfaceFlinger has a big problem with coverage blending.
      // If asked to render a single plane with coverage: it will apply
      // the specified (SRC_ALPHA, 1-SRC_ALPHA) to all four channels
      // (as per OpenGL spec) and give us a result to blend with
      // (1, 1-SRC_ALPHA) this will produce a different dst alpha than if
      // SF had done the whole composition (with a back layer) in GL.
      // The 'correct' way to do the blend would be to apply
      // (SRC_ALPHA, 1-SRC_ALPHA) only to the rgb channels and
      // (1, 1-SRC_ALPHA) for the alpha.

      // Do the coverage multiply
      static const char blendingFormatCoverageMultiply[] =
          "    incoming.rgb = incoming.rgb * incoming.a;\n";

      // Write the colour directly for the first layer
      static const char blendingFormatWrite[] = "    outColor = incoming;\n";

      // Otherwise blend and write
      static const char blendingFormatWritePremultBlend[] =
          "    outColor = outColor * (1.0-incoming.a) + incoming;\n";

      String8 blendingBlock;

      for (uint32_t i = 0; i < numLayers; ++i) {
        blendingBlock += String8::format(blendingFormatSample, i, i, i);

        bool opaque = opaqueLayerMask & (1 << i);
        bool premult = premultLayerMask & (1 << i);
        if (opaque) {
          if (premult)
            blendingBlock += blendingFormatOpaquePremultPlaneAlpha;
          else
            blendingBlock += blendingFormatOpaqueCoveragePlaneAlpha;
        } else {
          if (premult)
            blendingBlock += blendingFormatPremultPlaneAlpha;
          else
            blendingBlock += blendingFormatCoveragePlaneAlpha;
        }
        if (!premult)
          blendingBlock += blendingFormatCoverageMultiply;
        if (i == 0)
          blendingBlock += blendingFormatWrite;
        else
          blendingBlock += blendingFormatWritePremultBlend;
      }

      String8 outputDecls;
      String8 outputConversion;

      if (renderToNV12) {
        outputDecls = fragmentShaderNV12OutputDecls;
        outputConversion = fragmentShaderNV12OutputConversion;
      }

      fragmentShaderSource = String8::format(
          fragmentShaderFormat, outputDecls.c_str(), numLayers, numLayers,
          numLayers, blendingBlock.c_str(), outputConversion.c_str());
    } else {
      // Zero layers should result in clear to transparent
      static const char fragmentShaderFormat[] =
          "#version 300 es\n"
          "%sout mediump vec4 outColor;\n"
          "void main()\n"
          "{\n"
          "    outColor = %s;\n"
          "}";

      String8 outputDecls;
      String8 outputValue;
      if (renderToNV12) {
        outputDecls = fragmentShaderNV12OutputDecls;
        outputValue = "vec4(rgb_2_yuv(vec3(0,0,0), itu_601), 0)";
      } else {
        outputValue = "vec4(0,0,0,0)";
      }

      fragmentShaderSource = String8::format(
          fragmentShaderFormat, outputDecls.c_str(), outputValue.c_str());
    }

    ALOGD_IF(COMPOSITION_DEBUG, "Fragment Shader:\n%s\n",
             fragmentShaderSource.c_str());

    if (!fragmentShader.lazyCreate(GL_FRAGMENT_SHADER, fragmentShaderSource)) {
      ALOGE("Error on \"composite\" fragment shader creation");
    } else if (!program->lazyCreate(2, &vertexShader, &fragmentShader)) {
      ALOGE("Error on \"composite\" program shader creation");
    } else if (!program->use()) {
      ALOGE("Error on \"composite\" program binding");
    } else if (!program->getLocations()) {
      ALOGE("Error on \"composite\" program shader locations query");
      program->destroy();
    } else {
      result = true;
    }
  }

  return result;
}

bool
HwcTestReferenceComposer::CProgramStore::CRendererProgram::setPlaneAlphaUniform(
    float alpha) {
  bool result = true;

  if (m_planeAlpha != alpha) {
    glUniform1f(m_uPlaneAlpha, alpha);
    if (getGLError("glUniform1f")) {
      ALOGE("Unable to set the plane alpha uniform (%d) to %f", m_uPlaneAlpha,
            (double)alpha);
      result = false;
    } else {
      m_planeAlpha = alpha;
    }
  }
  return result;
}

bool HwcTestReferenceComposer::CProgramStore::CRendererProgram::getLocations() {
  return CProgramStore::getLocations(getId(), &m_vinPosition, &m_vinTexCoord,
                                     &m_uPlaneAlpha, &m_planeAlpha);
}

bool HwcTestReferenceComposer::CProgramStore::getLocations(
    GLint programId, GLint *pvinPosition, GLint *pvinTexCoord,
    GLint *puPlaneAlpha, GLfloat *pPlaneAlpha) {
  bool result = true;

  GLint vinPosition = 0;
  GLint vinTexCoord = 0;
  GLint uTexture = 0;

  // Force plane alpha to be set first time
  static const GLfloat defaultAlpha = -1.f;
  GLint uPlaneAlpha = 0;

  if (pvinPosition) {
    vinPosition = glGetAttribLocation(programId, "vinPosition");
    if (getGLError("glGetAttribLocation")) {
      HWCERROR(eCheckGlFail,
               "HwcTestReferenceComposer: Error on glGetAttribLocation");
      result = false;
    }
  }

  if (pvinTexCoord) {
    vinTexCoord = glGetAttribLocation(programId, "vinTexCoords0");
    if (getGLError("glGetAttribLocation")) {
      HWCERROR(eCheckGlFail,
               "HwcTestReferenceComposer: Error on glGetAttribLocation");
      result = false;
    }

    if (result) {
      uTexture = glGetUniformLocation(programId, "uTexture");
      if (getGLError("glGetUniformLocation")) {
        HWCERROR(eCheckGlFail,
                 "HwcTestReferenceComposer: Unable to find the uTexture "
                 "uniform location");
        result = false;
      }
    }

    // Setup a default value
    if (result) {
      GLint texturingUnits[] = {0};
      glUniform1iv(uTexture, 1, texturingUnits);
      if (getGLError("glUniform1iv")) {
        HWCERROR(
            eCheckGlFail,
            "HwcTestReferenceComposer: Unable to set the uTexture uniform");
        result = false;
      }
    }
  }

  if (puPlaneAlpha) {
    uPlaneAlpha = glGetUniformLocation(programId, "uPlaneAlpha[0]");
    if (getGLError("glGetUniformLocation")) {
      HWCERROR(eCheckGlFail,
               "HwcTestReferenceComposer: Unable to find the uPlaneAlpha[0] "
               "uniform location");
      result = false;
    }
  }

  // Setup the outputs, if everything went ok
  if (result) {
    if (pvinPosition) {
      *pvinPosition = vinPosition;
    }

    if (pvinTexCoord) {
      *pvinTexCoord = vinTexCoord;
    }

    if (puPlaneAlpha) {
      *puPlaneAlpha = uPlaneAlpha;
      *pPlaneAlpha = defaultAlpha;
    }
  }

  return result;
}

// This composition class adds a dither pattern to the render target.
// It currently assumes that the dithering is to a 6:6:6 display

HwcTestReferenceComposer::HwcTestReferenceComposer()
    : m_remainingConstructorAttempts(1),
      m_display(EGL_NO_DISPLAY),
      m_surface(EGL_NO_SURFACE),
      m_context(EGL_NO_CONTEXT),
      m_isFboIdValid(false),
      m_areVboIdsValid(false),
      m_nextVboIdIndex(0),
      m_destEGLImageCreated(false),
      m_destTextureCreated(false),
      m_destTextureSet(false),
      m_destWidth(0),
      m_destHeight(0),
      m_destGraphicBuffer(0),
      m_destTextureId(0),
      m_destTextureAttachedToFBO(false),
      m_nv12TargetSupported(false),
      m_destIsNV12(false),
      m_sourceEGLImagesCreated(0),
      m_sourceTexturesCreated(0),
      m_sourceTexturesSet(0),
      m_sourceGraphicBuffers(0),
      m_sourceEGLImages(0),
      m_sourceTextureIds(0),
      m_maxSourceLayers(0) {
  spRefCmp = this;
}

HwcTestReferenceComposer::~HwcTestReferenceComposer() {
  if (isCreated()) {
    destroy();
  }
}

/**
 Use the validity of one of the mandatory resources created by the lazy
 constructor as a marker telling us whether a successful lazy construction
 was already performed.
*/

bool HwcTestReferenceComposer::isCreated() const {
  return m_isFboIdValid != 0;
}

bool HwcTestReferenceComposer::lazyCreate() {
  // If the construction failed too many times we do not want to retry it
  // forever and further flood the logs

  if (m_remainingConstructorAttempts == 0) {
    return false;
  }

  --m_remainingConstructorAttempts;

  // Get a connection to the display
  m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  ALOGE("m_displayi 3 = %p ", m_display);
  if (getEGLError("eglGetDisplay") || m_display == EGL_NO_DISPLAY) {
    HWCERROR(eCheckGlFail, "HwcTestReferenceComposer: Error on eglGetDisplay");
    destroy();
    return false;
  }

  // Initialize EGL

  GLint majorVersion, minorVersion;

  GLint status = eglInitialize(m_display, &majorVersion, &minorVersion);
  if (getEGLError("eglInitialize") || status == EGL_FALSE) {
    HWCERROR(eCheckGlFail, "HwcTestReferenceComposer: Error on eglInitialize");
    destroy();
    return false;
  }

  // Get a configuration with at least 8 bits for red, green, blue and alpha.
  EGLConfig config;
  EGLint numConfigs;
  static EGLint const attributes[] = {
      EGL_RED_SIZE,       8,                EGL_GREEN_SIZE,
      8,                  EGL_BLUE_SIZE,    8,
      EGL_ALPHA_SIZE,     8,                EGL_DEPTH_SIZE,
      0,                  EGL_STENCIL_SIZE, 0,
      EGL_SURFACE_TYPE,   EGL_PBUFFER_BIT,  EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT, EGL_NONE};

  eglChooseConfig(m_display, attributes, &config, 1, &numConfigs);
  if (getEGLError("eglChooseConfig") || numConfigs == 0) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: Error on eglChooseConfig");
    destroy();
    return false;
  }

  // Create the context
  EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  m_context =
      eglCreateContext(m_display, config, EGL_NO_CONTEXT, context_attribs);
  if (getEGLError("eglCreateContext") || m_context == EGL_NO_CONTEXT) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: Error on eglCreateContext");
    destroy();
    return false;
  }

  // Create a 16x16 pbuffer which is never going to be written to, so the
  // dimmensions do not really matter
  static EGLint const pbuffer_attributes[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16,
                                              EGL_NONE};

  m_surface = eglCreatePbufferSurface(m_display, config, pbuffer_attributes);
  if (getEGLError("eglCreatePbufferSurface") || m_surface == EGL_NO_SURFACE) {
    ALOGE("Error on eglCreatePbufferSurface");
    destroy();
    return false;
  }

  // Save the GL context
  GLContextSaver contextSaver(this);

  // Switch to the newly created context.
  eglMakeCurrent(m_display, m_surface, m_surface, m_context);
  if (getEGLError("eglMakeCurrent lazyCreate")) {
    HWCERROR(eCheckGlFail, "HwcTestReferenceComposer: Error on eglMakeCurrent");
    destroy();
    return false;
  }

  // Create the FBO
  glGenFramebuffers(1, &m_fboId);
  if (getGLError("glGenFramebuffers")) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: Error on glGenFramebuffers");
    destroy();
    return false;
  }

  m_isFboIdValid = true;

  // Create the vertex buffer object
  glGenBuffers(NumVboIds, m_vboIds);
  if (getGLError("glGenBuffers")) {
    HWCERROR(eCheckGlFail, "HwcTestReferenceComposer: Error on glGenBuffers");
    destroy();
    return false;
  }

  m_areVboIdsValid = true;

  // Because this is a dedicated context that is only used for the
  // dithering operations we can setup most of the context state as
  // constant for the whole context life cycle.

  // Bind the frame buffer object
  glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);
  if (getGLError("glBindFramebuffer")) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: Error on glBindFramebuffer");
    destroy();
    return false;
  }

  if (NumVboIds == 1) {
    // Bind the VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
    getGLError("glBindBuffer");
  }

  // Disable blending
  glDisable(GL_BLEND);
  getGLError("glDisable GL_BLEND");

  // Query the context for extension support
  m_nv12TargetSupported = strstr((const char *)glGetString(GL_EXTENSIONS),
                                 "GL_EXT_YUV_target") != NULL;

  return true;
}

void HwcTestReferenceComposer::destroy() {
  if (!isCreated()) {
    return;
  }

  // Save the GL context
  GLContextSaver contextSaver(this);

  // Switch to our context (if available)
  if (m_display != EGL_NO_DISPLAY && m_surface != EGL_NO_SURFACE &&
      m_context != EGL_NO_CONTEXT) {
    eglMakeCurrent(m_display, m_surface, m_surface, m_context);
    if (getEGLError("eglMakeCurrent destroy (2)")) {
      // Note, resources may leak if there is an error here. However, typically
      // this
      // happens with global destructors in tests where its harmless.
      return;
    }
  }

  // Destroy the program store
  m_programStore.destroy();

  // Delete the vertex buffer object
  if (m_areVboIdsValid) {
    // Destroy the VBO
    glDeleteBuffers(NumVboIds, m_vboIds);
    getGLError("glDeleteBuffers");

    // Mark it as invalid
    m_areVboIdsValid = false;
  }

  // Delete the frame buffer object
  if (m_isFboIdValid) {
    // Destroy the FBO
    glDeleteFramebuffers(1, &m_fboId);
    getGLError("glDeleteFramebuffers");

    // Mark it as invalid
    m_isFboIdValid = false;
  }

  // Unset the context and surface
  if (m_display != EGL_NO_DISPLAY) {
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    getEGLError("eglMakeCurrent destroy(3)");
  }

  // Destroy the surface
  if (m_surface != EGL_NO_SURFACE) {
    eglDestroySurface(m_display, m_surface);
    getEGLError("eglDestroySurface");

    // Mark it as invalid
    m_surface = EGL_NO_SURFACE;
  }

  // Destroy the context
  if (m_context != EGL_NO_CONTEXT) {
    eglDestroyContext(m_display, m_context);
    getEGLError("eglDestroyContext");
  }

  // Mark the display as invalid
  m_display = EGL_NO_DISPLAY;

  ALOGE("m_displayi 5 = %p ", m_display);
  // Free the source layers array
  freeSourceLayers();
}

bool HwcTestReferenceComposer::attachToFBO(GLuint textureId) {
  bool done = false;

  // Attach the colour buffer
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         m_destIsNV12 ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D,
                         textureId, 0);
  if (getGLError("glFramebufferTexture2D")) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: A temporary texture could not be "
             "attached to the frame buffer object for target %p",
             mTargetHandle);
  } else {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (getGLError("glCheckFramebufferStatus") ||
        status != GL_FRAMEBUFFER_COMPLETE) {
      HWCERROR(eCheckGlFail,
               "HwcTestReferenceComposer: The frame buffer is not ready");
    } else {
      done = true;
    }
  }

  return done;
}

void HwcTestReferenceComposer::setTexture(
    const hwcval_layer_t *layer, uint32_t texturingUnit, bool *pEGLImageCreated,
    bool *pTextureCreated, bool *pTextureSet,
    HWCNativeHandle *pGraphicBuffer, EGLImageKHR *pEGLImage,
    GLuint *pTextureId, int filter) {
  // Nothing is successfull, unless something different is later stated
  *pEGLImageCreated = false;
  *pTextureCreated = false;
  *pTextureSet = false;

  bufferHandler_->CopyHandle(layer->gralloc_handle, pGraphicBuffer);
  const EGLint image_attrs[] = {
      EGL_WIDTH,
      static_cast<EGLint>(layer->gralloc_handle->meta_data_.width_),
      EGL_HEIGHT,
      static_cast<EGLint>(layer->gralloc_handle->meta_data_.height_),
      EGL_LINUX_DRM_FOURCC_EXT,
      static_cast<EGLint>(layer->gralloc_handle->meta_data_.format_),
      EGL_DMA_BUF_PLANE0_FD_EXT,
      static_cast<EGLint>(layer->gralloc_handle->meta_data_.prime_fds_[0]),
      EGL_DMA_BUF_PLANE0_PITCH_EXT,
      static_cast<EGLint>(layer->gralloc_handle->meta_data_.pitches_[0]),
      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
      0,
      EGL_NONE,
  };

  *pEGLImage =
      eglCreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                        (EGLClientBuffer)NULL, image_attrs);

  if (getEGLError("eglCreateImageKHR")) {
    HWCERROR(
        eCheckGlFail,
        "HwcTestReferenceComposer: A temporary EGL image could not be created");
  } else {
    *pEGLImageCreated = true;

    // Create a texture for the EGL image

    glGenTextures(1, pTextureId);
    if (getGLError("glGenTextures")) {
      HWCERROR(
          eCheckGlFail,
          "HwcTestReferenceComposer: A temporary texture could not be created");
    } else {
      *pTextureCreated = true;

      glActiveTexture(GL_TEXTURE0 + texturingUnit);
      if (getGLError("glActiveTexture")) {
        HWCERROR(
            eCheckGlFail,
            "HwcTestReferenceComposer: A temporary texture could not be set\n");
      } else {
        glBindTexture(GL_TEXTURE_2D, *pTextureId);
        if (getGLError("glBindTexture")) {
          HWCERROR(
              eCheckGlFail,
              "HwcTestReferenceComposer: A temporary texture could not be set");
        } else {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
          if (getGLError("glTexParameteri")) {
            HWCERROR(eCheckGlFail,
                     "HwcTestReferenceComposer: A temporary texture could not "
                     "be set");
          } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            if (getGLError("glTexParameteri")) {
              HWCERROR(eCheckGlFail,
                       "HwcTestReferenceComposer: A temporary texture could "
                       "not be set");
            } else {
              glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                                           (GLeglImageOES)*pEGLImage);
              if (getGLError("glEGLImageTargetTexture2DOES")) {
                HWCERROR(eCheckGlFail,
                         "HwcTestReferenceComposer: A temporary texture could "
                         "not be set");
              } else {
                *pTextureSet = true;
              }
            }
          }
        }
      }
    }
  }
}

status_t HwcTestReferenceComposer::bindTexture(GLuint texturingUnit,
                                               GLuint textureId) {
  status_t result = UNKNOWN_ERROR;

  glActiveTexture(GL_TEXTURE0 + texturingUnit);
  if (getGLError("glActiveTexture")) {
    HWCERROR(
        eCheckGlFail,
        "HwcTestReferenceComposer: A temporary texture could not be set\n");
  } else {
    glBindTexture(GL_TEXTURE_2D, textureId);
    if (getGLError("glBindTexture")) {
      HWCERROR(
          eCheckGlFail,
          "HwcTestReferenceComposer: A temporary texture could not be set");
    } else {
      result = OK;
    }
  }

  return result;
}

/**
 Create a quad where each vertex contains:
 - Anticlockwise-defined positions in NDC
 - Texture coordinates in [0,1] range, one pair per layer
*/

static void setupVBOData(GLfloat *vboData, uint32_t stride, uint32_t destWidth,
                         uint32_t destHeight, const hwcval_layer_t *layer) {
  // First get input buffer size

  if (!layer->gralloc_handle) {
    // If we have a handle, make sure the BufferInfo is updated
    HWCERROR(eCheckGrallocDetails,
               "gralloc handle is null in reference composer");
  }

  GLfloat destCenterX = 0.5f * destWidth;
  GLfloat destCenterY = 0.5f * destHeight;

// Calculate an oversized destination region where the right edge
// is moved to the right and the top edge is moved upper
#if SINGLE_TRIANGLE
  static const float oversizing = 2.0f;
#else
  static const float oversizing = 1.0f;
#endif
  float left = layer->displayFrame.left;
  float right = layer->displayFrame.right;
  float top = layer->displayFrame.top;
  float bottom = layer->displayFrame.bottom;

  float width2 = right - left;
  float height2 = bottom - top;
  float right2 = left + oversizing * width2;
  float top2 = bottom - oversizing * height2;

  // Calculate the corners in normalized device coordinates
  GLfloat ndcX0 = 2.f * (left - destCenterX) / destWidth;
  GLfloat ndcX1 = 2.f * (right2 - destCenterX) / destWidth;
  GLfloat ndcY0 = 2.f * (top2 - destCenterY) / destHeight;
  GLfloat ndcY1 = 2.f * (bottom - destCenterY) / destHeight;

  // Left-top
  vboData[0 * stride + 0] = ndcX0;
  vboData[0 * stride + 1] = ndcY0;

  // Left-bottom
  vboData[1 * stride + 0] = ndcX0;
  vboData[1 * stride + 1] = ndcY1;

  // Right-bottom
  vboData[2 * stride + 0] = ndcX1;
  vboData[2 * stride + 1] = ndcY1;

  // Right-top
  vboData[3 * stride + 0] = ndcX1;
  vboData[3 * stride + 1] = ndcY0;

  // Set the texture coordinates
  GLfloat texCoords[8];

  // Calculate the insideness in the 0..+1 range
  const GLfloat insidenessLeft = 0;
  const GLfloat insidenessRight = 1.0;
  const GLfloat insidenessTop = 0;
  const GLfloat insidenessBottom = 1.0;

  // Use the insideness for calculating the texture coordinates
  GLfloat sourceWidthRec =
      1.f / (GLfloat)layer->gralloc_handle->meta_data_.width_;
  GLfloat sourceHeightRec =
      1.f / (GLfloat)layer->gralloc_handle->meta_data_.height_;

  GLfloat sourceLeft = ((GLfloat)layer->sourceCropf.left) * sourceWidthRec;
  GLfloat sourceTop = ((GLfloat)layer->sourceCropf.top) * sourceHeightRec;
  GLfloat sourceRight = ((GLfloat)layer->sourceCropf.right) * sourceWidthRec;
  GLfloat sourceBottom = ((GLfloat)layer->sourceCropf.bottom) * sourceHeightRec;

  // Apply transforms and scale appropriately.
  if (layer->transform & HWC_TRANSFORM_FLIP_H) {
    swap(sourceLeft, sourceRight);
  }
  if (layer->transform & HAL_TRANSFORM_FLIP_V) {
    swap(sourceTop, sourceBottom);
  }
  if (layer->transform & HAL_TRANSFORM_ROT_90) {
    GLfloat scaledLeftY =
        sourceBottom + (sourceTop - sourceBottom) * insidenessLeft;
    GLfloat scaledRightY =
        sourceBottom + (sourceTop - sourceBottom) * insidenessRight;
    GLfloat scaledTopX =
        sourceLeft + (sourceRight - sourceLeft) * insidenessTop;
    GLfloat scaledBottomX =
        sourceLeft + (sourceRight - sourceLeft) * insidenessBottom;

    texCoords[0] = scaledTopX;
    texCoords[1] = scaledLeftY;
    texCoords[2] = scaledBottomX;
    texCoords[3] = scaledLeftY;
    texCoords[4] = scaledBottomX;
    texCoords[5] = scaledRightY;
    texCoords[6] = scaledTopX;
    texCoords[7] = scaledRightY;
  } else {
    GLfloat scaledLeftX =
        sourceLeft + (sourceRight - sourceLeft) * insidenessLeft;
    GLfloat scaledRightX =
        sourceLeft + (sourceRight - sourceLeft) * insidenessRight;
    GLfloat scaledTopY = sourceTop + (sourceBottom - sourceTop) * insidenessTop;
    GLfloat scaledBottomY =
        sourceTop + (sourceBottom - sourceTop) * insidenessBottom;

    texCoords[0] = scaledLeftX;
    texCoords[1] = scaledTopY;
    texCoords[2] = scaledLeftX;
    texCoords[3] = scaledBottomY;
    texCoords[4] = scaledRightX;
    texCoords[5] = scaledBottomY;
    texCoords[6] = scaledRightX;
    texCoords[7] = scaledTopY;
  }

  // Adjust the effect of the oversizing on the texturing
  float VertAdjU = (oversizing - 1.f) * (texCoords[0] - texCoords[2]);
  float VertAdjV = (oversizing - 1.f) * (texCoords[1] - texCoords[3]);
  float HorAdjU = (oversizing - 1.f) * (texCoords[4] - texCoords[2]);
  float HorAdjV = (oversizing - 1.f) * (texCoords[5] - texCoords[3]);

  texCoords[0] += VertAdjU;
  texCoords[1] += VertAdjV;

  texCoords[4] += HorAdjU;
  texCoords[5] += HorAdjV;

  texCoords[6] += HorAdjU + VertAdjU;
  texCoords[7] += HorAdjV + VertAdjV;

  // Copy to the VBO
  vboData[0 * stride + 2 + 0] = texCoords[0];
  vboData[0 * stride + 2 + 1] = texCoords[1];
  vboData[1 * stride + 2 + 0] = texCoords[2];
  vboData[1 * stride + 2 + 1] = texCoords[3];
  vboData[2 * stride + 2 + 0] = texCoords[4];
  vboData[2 * stride + 2 + 1] = texCoords[5];
  vboData[3 * stride + 2 + 0] = texCoords[6];
  vboData[3 * stride + 2 + 1] = texCoords[7];
}

status_t HwcTestReferenceComposer::beginFrame(uint32_t numSources,
                                              const hwcval_layer_t *source,
                                              const hwcval_layer_t *target) {
#if CREATEDESTROY_ONCE
  static bool firstTime = true;
#endif
  mTargetHandle = target->gralloc_handle;
  ALOG_ASSERT(mTargetHandle);

  uint32_t numSourcesToCompose = 0;
  for (uint32_t i = 0; i < numSources; ++i) {
    if ((source[i].compositionType == HWC2_COMPOSITION_CLIENT) &&
        (source[i].gralloc_handle != 0)) {
      ++numSourcesToCompose;
    }
  }

  // Realloc the source layers array
  if (numSourcesToCompose > m_maxSourceLayers) {
    if (!reallocSourceLayers(numSourcesToCompose)) {
      return UNKNOWN_ERROR;
    }
  }

  // Ensure the instance is fully created
  if (!isCreated() && lazyCreate() == false) {
    return UNKNOWN_ERROR;
  }

  // Switch to our context
  eglMakeCurrent(m_display, m_surface, m_surface, m_context);
  if (getEGLError("eglMakeCurrent beginFrame")) {
    return UNKNOWN_ERROR;
  }

// Save the layers
#if CREATEDESTROY_ONCE
  if (firstTime) {
    firstTime = false;
  } else {
    return OK;
  }
#endif
  // Set the destination texture
  setTexture(target, numSourcesToCompose, &m_destEGLImageCreated,
             &m_destTextureCreated, &m_destTextureSet, &m_destGraphicBuffer,
             &m_destEGLImage, &m_destTextureId,
             GL_NEAREST);  // Filter should be GL_NEAREST or GL_LINEAR

  m_destWidth = target->displayFrame.right - target->displayFrame.left;
  m_destHeight = target->displayFrame.bottom - target->displayFrame.top;

  m_destIsNV12 = IsLayerNV12(target);
  HWCLOGD_COND(eLogGl,
               "HwcTestReferenceComposer::BeginFrame target %p is %sNV12",
               target->gralloc_handle, (m_destIsNV12 ? "" : "NOT "));

  if (m_destTextureSet) {
    m_destTextureAttachedToFBO = attachToFBO(m_destTextureId);
  }

  // Create the source textures
  m_sourceEGLImagesCreated = 0;
  m_sourceTexturesCreated = 0;
  m_sourceTexturesSet = 0;
  uint32_t textureIx = 0;

  for (uint32_t i = 0; i < numSources; ++i) {
    if ((source[i].compositionType == HWC2_COMPOSITION_CLIENT) &&
        (source[i].gralloc_handle != 0)) {
      bool sourceEGLImageCreated = false;
      bool sourceTextureCreated = false;
      bool sourceTextureSet = false;

      float sw = source[i].sourceCropf.right - source[i].sourceCropf.left;
      float sh = source[i].sourceCropf.bottom - source[i].sourceCropf.top;
      float dw = source[i].displayFrame.right - source[i].displayFrame.left;
      float dh = source[i].displayFrame.bottom - source[i].displayFrame.top;

      bool scaling = (source[i].transform & HAL_TRANSFORM_ROT_90)
                         ? ((sw != dh) || (sh != dw))
                         : ((sw != dw) || (sh != dh));

      setTexture(source + i, textureIx++, &sourceEGLImageCreated,
                 &sourceTextureCreated, &sourceTextureSet,
                 &m_sourceGraphicBuffers[m_sourceTexturesSet],
                 &m_sourceEGLImages[m_sourceTexturesSet],
                 &m_sourceTextureIds[m_sourceTexturesSet],
                 scaling ? GL_LINEAR : GL_NEAREST);

      if (!sourceEGLImageCreated) {
        break;
      }
      ++m_sourceEGLImagesCreated;

      if (!sourceTextureCreated) {
        break;
      }
      ++m_sourceTexturesCreated;

      if (!sourceTextureSet) {
        break;
      }
      ++m_sourceTexturesSet;
    }
  }

  // Adjust the view port for covering the whole destination rectangle
  glViewport(0, 0, m_destWidth, m_destHeight);
  getGLError("glViewport");

  status_t result;

  if (m_destTextureAttachedToFBO) {
    if (m_sourceTexturesSet < numSourcesToCompose) {
      // We are indicating that parts of the composition were a failure
      // so we know not to trust the result.
      HWCERROR(eCheckGlFail,
               "Reference composer: some layers could not be composed.");
      result = UNKNOWN_ERROR;
    } else {
      result = OK;
    }
  } else {
    result = UNKNOWN_ERROR;
  }

  return result;
}

status_t HwcTestReferenceComposer::draw(const hwcval_layer_t *layer,
                                        uint32_t index) {
  // Check that the destination texture is attached to the FBO
  if (!m_destTextureAttachedToFBO) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: The destination texture is not "
             "attached to the FBO");
    return UNKNOWN_ERROR;
  }

  // Bind the source texture
  status_t status = bindTexture(0, m_sourceTextureIds[index]);
  if (status != OK) {
    HWCERROR(eCheckGlFail,
             "HwcTestReferenceComposer: Unable to bind a source texture");
    return UNKNOWN_ERROR;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // Bind the program
  bool opaque = HasAlpha(layer) && (layer->blending == HWC_BLENDING_NONE);
  bool preMult = (layer->blending != HWC_BLENDING_COVERAGE);
  bool isProgramBound =
      m_programStore.bind(layer->planeAlpha, m_destIsNV12, opaque, preMult);

  if (isProgramBound) {
    // Setup the VBO contents
    uint32_t vertexStride = 4;
    GLfloat vboData[4 * vertexStride];

    setupVBOData(vboData, vertexStride, m_destWidth, m_destHeight, layer);

    // Bind a VBO
    bindAVbo();

    // Discard the previous contents and setup new ones
    glBufferData(GL_ARRAY_BUFFER, sizeof(vboData), vboData, GL_STREAM_DRAW);
    getGLError("glBufferData");

    glVertexAttribPointer(m_programStore.getPositionVertexIn(), 2, GL_FLOAT,
                          GL_FALSE, vertexStride * sizeof(GLfloat), (void *)0);
    getGLError("glVertexAttribPointer");

    glEnableVertexAttribArray(m_programStore.getPositionVertexIn());
    getGLError("glEnableVertexAttribArray");

    glVertexAttribPointer(m_programStore.getTexCoordVertexIn(), 2, GL_FLOAT,
                          GL_FALSE, vertexStride * sizeof(GLfloat),
                          (void *)(2 * sizeof(float)));
    getGLError("glVertexAttribPointer");

    glEnableVertexAttribArray(m_programStore.getTexCoordVertexIn());
    getGLError("glEnableVertexAttribArray");

    // Draw the quad (fan)
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    getGLError("glDrawArrays");
  }

  return OK;
}

status_t HwcTestReferenceComposer::endFrame() {
  status_t result;

  glFinish();
  if (getGLError("glFinish"))  // Make it synchronous
  {
    result = UNKNOWN_ERROR;
  } else {
    result = OK;
  }

#if !CREATEDESTROY_ONCE
  // Destroy the destination texture
  if (m_destTextureCreated) {
    glDeleteTextures(1, &m_destTextureId);
    getGLError("glDeleteTextures");
  }

  // Destroy the destination EGL image
  if (m_destEGLImageCreated) {
    eglDestroyImageKHR(m_display, m_destEGLImage);
    getEGLError("eglDestroyImageKHR");
  }

  // Destroy the source textures
  glDeleteTextures(m_sourceTexturesCreated, m_sourceTextureIds);
  getEGLError("glDeleteTextures");

  // Destroy the source EGL images
  uint32_t i;
  for (i = 0; i < m_sourceEGLImagesCreated; ++i) {
    eglDestroyImageKHR(m_display, m_sourceEGLImages[i]);
    getEGLError("eglDestroyImageKHR");
  }
#endif
  return result;
}

status_t HwcTestReferenceComposer::Compose(uint32_t numSources,
                                           hwcval_layer_t *source,
                                           hwcval_layer_t *target,
                                           bool waitForFences) {
  status_t result;

  HWCVAL_LOCK(_l, mComposeMutex);
  mErrorOccurred = false;

  // Save the GL context
  GLContextSaver contextSaver(this);
  result = beginFrame(numSources, source, target);

  if (result == OK) {
    glClearColor(0.f, 0.f, 0.f, 0.f);

    glClear(GL_COLOR_BUFFER_BIT);
    if (getGLError("glClear")) {
      result = UNKNOWN_ERROR;
    }
  }

  uint32_t index;

  if (waitForFences && (target->acquireFence > 0)) {
    if (hwcomposer::HWCPoll(target->acquireFence, HWCVAL_SYNC_WAIT_100MS) < 0) {
      HWCERROR(eCheckGlFail,
               "HwcTestReferenceComposer: Target acquire fence timeout");
    }
  }

  uint32_t screenIndex = 0;
  for (index = 0; index < numSources && result == OK; ++index) {
    hwcval_layer_t &srcLayer = source[index];

    if ((srcLayer.compositionType == HWC2_COMPOSITION_CLIENT) &&
        (srcLayer.gralloc_handle != 0)) {
      // Wait for any acquire fence
      if (waitForFences && (srcLayer.acquireFence > 0)) {
        if (hwcomposer::HWCPoll(srcLayer.acquireFence, HWCVAL_SYNC_WAIT_100MS) < 0) {
          HWCERROR(eCheckGlFail,
                   "HwcTestReferenceComposer: Acquire fence timeout layer %d",
                   index);
        }
      }

      // We know that the vp renderer is synchronous, indicate that here.
      srcLayer.releaseFence = -1;

      result = (result == OK) ? draw(&srcLayer, screenIndex++) : result;
    }
  }

  result = (result == OK) ? endFrame() : result;
  if ((result == OK) && (mErrorOccurred)) {
    result = UNKNOWN_ERROR;
  }

  return result;
}

void HwcTestReferenceComposer::bindAVbo() {
  if (NumVboIds > 1) {
    // Bind the VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vboIds[m_nextVboIdIndex]);
    getGLError("glBindBuffer");

    m_nextVboIdIndex = (m_nextVboIdIndex + 1) % NumVboIds;
  }
}

bool HwcTestReferenceComposer::reallocSourceLayers(uint32_t maxSourceLayers) {
  bool result;
  m_sourceGraphicBuffers = new HWCNativeHandle[ maxSourceLayers ];
  m_sourceEGLImages = new EGLImageKHR[maxSourceLayers];
  m_sourceTextureIds = new GLuint[maxSourceLayers];

  if (m_sourceGraphicBuffers && m_sourceEGLImages && m_sourceTextureIds) {
    m_maxSourceLayers = maxSourceLayers;
    result = true;
  } else {
    m_maxSourceLayers = 0;
    result = false;
  }

  return result;
}

void HwcTestReferenceComposer::freeSourceLayers() {
  delete[] m_sourceGraphicBuffers;
  delete[] m_sourceEGLImages;
  delete[] m_sourceTextureIds;

  m_sourceGraphicBuffers = 0;
  m_sourceEGLImages = 0;
  m_sourceTextureIds = 0;
  m_maxSourceLayers = 0;
}

HWCNativeHandle HwcTestReferenceComposer::CopyBuf(HWCNativeHandle handle) {
  if (!handle) {
    return 0;
  }

  // Get destination graphic buffer
  HWCNativeHandle spDestBuffer;
  bufferHandler_->ImportBuffer(handle);
  ALOGE("buffer = %p  width = %u height = %u", handle->meta_data_.width_,
        handle->meta_data_.height_, handle->meta_data_.format_);
  bufferHandler_->CreateBuffer(handle->meta_data_.width_,
                               handle->meta_data_.height_,
                               handle->meta_data_.format_, &spDestBuffer);
  bufferHandler_->CopyHandle(spDestBuffer, &spDestBuffer);
  bufferHandler_->ImportBuffer(spDestBuffer);

  hwcval_layer_t srcLayer;
  srcLayer.gralloc_handle = handle;
  srcLayer.compositionType = HWC2_COMPOSITION_CLIENT;
  srcLayer.hints = 0;
  srcLayer.flags = 0;
  srcLayer.transform = 0;
  srcLayer.blending = HWC_BLENDING_PREMULT;
  srcLayer.sourceCropf.left = 0.0;
  srcLayer.sourceCropf.top = 0.0;
  srcLayer.sourceCropf.right = spDestBuffer->meta_data_.width_;
  srcLayer.sourceCropf.bottom = spDestBuffer->meta_data_.height_;
  srcLayer.displayFrame.left = 0;
  srcLayer.displayFrame.top = 0;
  srcLayer.displayFrame.right = spDestBuffer->meta_data_.width_;
  srcLayer.displayFrame.bottom = spDestBuffer->meta_data_.height_;
  srcLayer.visibleRegionScreen.numRects = 1;
  srcLayer.visibleRegionScreen.rects = &srcLayer.displayFrame;
  srcLayer.acquireFence = -1;
  srcLayer.releaseFence = -1;
  srcLayer.planeAlpha = 255;

  hwcval_layer_t tgtLayer = srcLayer;
  tgtLayer.gralloc_handle = spDestBuffer;

  if (Compose(1, &srcLayer, &tgtLayer, false) == OK) {
    return spDestBuffer;
  } else {
    // Don't return a copy buffer if any part of the copy might not be correct
    return 0;
  }
}

bool HwcTestReferenceComposer::IsLayerNV12(const hwcval_layer_t *pDest) {

  if (pDest->gralloc_handle) {
    // No handle
    return false;
  }

  return (IsNV12(pDest->gralloc_handle->meta_data_.format_));
}

bool HwcTestReferenceComposer::HasAlpha(const hwcval_layer_t *pSrc) {
  if (pSrc->gralloc_handle) {
    // No handle
    return false;
  }

  return (::HasAlpha(pSrc->gralloc_handle->meta_data_.format_));
}
