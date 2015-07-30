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

#ifndef ANDROID_GL_WORKER_H_
#define ANDROID_GL_WORKER_H_

#include <pthread.h>

#include <memory>
#include <vector>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <ui/GraphicBuffer.h>

struct hwc_layer_1;

namespace android {

#define AUTO_GL_TYPE(name, type, zero, deleter) \
  struct name##Deleter {                        \
    typedef type pointer;                       \
                                                \
    void operator()(pointer p) const {          \
      if (p != zero) {                          \
        deleter;                                \
      }                                         \
    }                                           \
  };                                            \
  typedef std::unique_ptr<type, name##Deleter> name;

AUTO_GL_TYPE(AutoGLFramebuffer, GLuint, 0, glDeleteFramebuffers(1, &p))
AUTO_GL_TYPE(AutoGLBuffer, GLuint, 0, glDeleteBuffers(1, &p))
AUTO_GL_TYPE(AutoGLTexture, GLuint, 0, glDeleteTextures(1, &p))
AUTO_GL_TYPE(AutoGLShader, GLint, 0, glDeleteShader(p))
AUTO_GL_TYPE(AutoGLProgram, GLint, 0, glDeleteProgram(p))

struct EGLImageDeleter {
  typedef EGLImageKHR pointer;

  EGLDisplay egl_display_;

  EGLImageDeleter(EGLDisplay egl_display) : egl_display_(egl_display) {
  }

  void operator()(EGLImageKHR p) const {
    if (p != EGL_NO_IMAGE_KHR) {
      eglDestroyImageKHR(egl_display_, p);
    }
  }
};
typedef std::unique_ptr<EGLImageKHR, EGLImageDeleter> AutoEGLImageKHR;

struct AutoEGLImageAndGLTexture {
  AutoEGLImageKHR image;
  AutoGLTexture texture;

  AutoEGLImageAndGLTexture(EGLDisplay egl_display)
      : image(EGL_NO_IMAGE_KHR, EGLImageDeleter(egl_display)) {
  }
};

class GLWorkerCompositor {
 public:
  GLWorkerCompositor();
  ~GLWorkerCompositor();

  int Init();

  int Composite(hwc_layer_1 *layers, size_t num_layers,
                sp<GraphicBuffer> framebuffer);
  int CompositeAndFinish(hwc_layer_1 *layers, size_t num_layers,
                         sp<GraphicBuffer> framebuffer);

 private:
  EGLDisplay egl_display_;
  EGLContext egl_ctx_;

  std::vector<AutoGLProgram> blend_programs_;
  AutoGLBuffer vertex_buffer_;
};

class GLWorker {
 public:
  struct Work {
    hwc_layer_1 *layers;
    size_t num_layers;
    int timeline_fd;
    sp<GraphicBuffer> framebuffer;

    Work() = default;
    Work(const Work &rhs) = delete;
  };

  GLWorker();
  ~GLWorker();

  int Init();

  int DoWork(Work *work);

 private:
  bool initialized_;
  pthread_t thread_;
  pthread_mutex_t lock_;
  pthread_cond_t work_ready_cond_;
  pthread_cond_t work_done_cond_;
  Work *worker_work_;
  bool work_ready_;
  bool worker_exit_;
  int worker_ret_;

  void WorkerRoutine();
  int DoComposition(GLWorkerCompositor &compositor, Work *work);

  int SignalWorker(Work *work, bool worker_exit);

  static void *StartRoutine(void *arg);
};
}

#endif
