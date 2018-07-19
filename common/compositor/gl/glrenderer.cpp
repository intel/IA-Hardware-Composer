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

#include "glrenderer.h"

#include "glprogram.h"
#include "hwctrace.h"
#include "nativesurface.h"
#include "renderstate.h"
#include "shim.h"
#ifdef COMPOSITOR_TRACING
#include "hwcutils.h"
#endif

namespace hwcomposer {

GLRenderer::~GLRenderer() {
  if (!context_.MakeCurrent()) {
    ETRACE("Failed make current context.");
    return;
  }

  if (vertex_array_)
    glDeleteVertexArraysOES(1, &vertex_array_);
}

bool GLRenderer::Init() {
  // clang-format off
  const GLfloat verts[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f,
                           0.0f, 2.0f, 2.0f, 0.0f, 2.0f, 0.0f};
  // clang-format on
  if (!context_.Init()) {
    ETRACE("Failed to initialize EGLContext.");
    return false;
  }

  if (!context_.MakeCurrent()) {
    ETRACE("Failed to initialize GLRenderer.");
    return false;
  }

  InitializeShims();

  // generate the VAO & bind
  GLuint vertex_array;
  glGenVertexArraysOES(1, &vertex_array);
  glBindVertexArrayOES(vertex_array);

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  std::unique_ptr<GLProgram> program(new GLProgram());
  if (program->Init(1)) {
    programs_.emplace_back(std::move(program));
  }

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, NULL);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                        (void *)(sizeof(float) * 2));

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  vertex_array_ = vertex_array;

  return true;
}

bool GLRenderer::Draw(const std::vector<RenderState> &render_states,
                      NativeSurface *surface) {
  GLuint frame_width = surface->GetWidth();
  GLuint frame_height = surface->GetHeight();
  GLuint left = 0;
  GLuint top = 0;

  // GL rendere should not support protected
  surface->GetLayer()->SetProtected(false);

  if (!surface->MakeCurrent())
    return false;
#ifdef COMPOSITOR_TRACING
  ICOMPOSITORTRACE("Draw starts \n");
#endif

  bool clear_surface = surface->ClearSurface();
  bool partial_clear = surface->IsPartialClear();

  surface->SetClearSurface(NativeSurface::kNone);

  glViewport(left, top, frame_width, frame_height);

  if (clear_surface || partial_clear) {
    const HwcRect<int> &damage = surface->GetSurfaceDamage();
    GLuint clear_width = damage.right - damage.left;
    GLuint clear_height = damage.bottom - damage.top;
    if (surface->IsOnScreen() &&
        ((frame_width != clear_width) || (frame_height != clear_height))) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(damage.left, damage.top, clear_width, clear_height);
      glClear(GL_COLOR_BUFFER_BIT);
    } else {
      glClear(GL_COLOR_BUFFER_BIT);
      glEnable(GL_SCISSOR_TEST);
    }
  } else {
    glEnable(GL_SCISSOR_TEST);
  }

#ifdef COMPOSITOR_TRACING
  const HwcRect<int> &damage = surface->GetSurfaceDamage();
  uint32_t total_width = 0;
  uint32_t total_height = 0;
  ICOMPOSITORTRACE(
      "Full clear: %d Partial clear: %d Skipped clear: %d damage.left: %d "
      "damage.top: %d damage.right - "
      "damage.left %d damage.bottom - damage.top %d \n",
      clear_surface, partial_clear, !(clear_surface || partial_clear),
      damage.left, damage.top, damage.right - damage.left,
      damage.bottom - damage.top);
#endif
  for (const RenderState &state : render_states) {
    unsigned size = state.layer_state_.size();
    GLProgram *program = GetProgram(size);
    if (!program)
      continue;

    program->UseProgram(state, frame_width, frame_height);
#ifdef COMPOSITOR_TRACING
    ICOMPOSITORTRACE(
        "scissor_x_: %d state.scissor_y_: %d scissor_width_: %d "
        "scissor_height_: %d \n",
        state.scissor_x_, state.scissor_y_, state.scissor_width_,
        state.scissor_height_);
    total_width += std::max(total_width, state.scissor_width_);
    total_height += state.scissor_height_;
    const HwcRect<int> &damage = surface->GetSurfaceDamage();
    if (AnalyseOverlap(
            damage, HwcRect<int>(state.scissor_x_, state.scissor_y_,
                                 state.scissor_x_ + state.scissor_width_,
                                 state.scissor_y_ + state.scissor_height_)) ==
        kOutside) {
      ICOMPOSITORTRACE("ALERT: Rendering Layer outside Damaged Region. \n");
    }
#endif
    glScissor(state.scissor_x_, state.scissor_y_, state.scissor_width_,
              state.scissor_height_);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    for (unsigned src_index = 0; src_index < size; src_index++) {
      glActiveTexture(GL_TEXTURE0 + src_index);
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    }
  }

  glDisable(GL_SCISSOR_TEST);

  if (!disable_explicit_sync_)
    surface->SetNativeFence(context_.GetSyncFD());

  surface->ResetDamage();
#ifdef COMPOSITOR_TRACING
  if ((clear_surface || partial_clear) &&
      ((total_width != surface->GetLayer()->GetDisplayFrameWidth()) ||
       (total_height != surface->GetLayer()->GetDisplayFrameHeight()))) {
    ICOMPOSITORTRACE(
        "Alert Wong composition total_width: %d "
        "surface->GetLayer()->GetDisplayFrameWidth() %d total_height %d "
        "surface->GetLayer()->GetDisplayFrameHeight() %d. \n",
        total_width, surface->GetLayer()->GetDisplayFrameWidth(), total_height,
        surface->GetLayer()->GetDisplayFrameHeight());
  }
  ICOMPOSITORTRACE("Draw Ends. \n");
#endif
  return true;
}

void GLRenderer::InsertFence(int32_t kms_fence) {
  if (kms_fence > 0) {
    EGLint attrib_list[] = {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, static_cast<EGLint>(kms_fence),
        EGL_NONE,
    };
    EGLSyncKHR fence = eglCreateSyncKHR(
        context_.GetDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, attrib_list);
    eglWaitSyncKHR(context_.GetDisplay(), fence, 0);
    eglDestroySyncKHR(context_.GetDisplay(), fence);
  } else {
    glFlush();
  }
}

void GLRenderer::SetExplicitSyncSupport(bool disable_explicit_sync) {
  disable_explicit_sync_ = disable_explicit_sync;
}

GLProgram *GLRenderer::GetProgram(unsigned texture_count) {
  if (programs_.size() >= texture_count) {
    GLProgram *program = programs_[texture_count - 1].get();
    if (program != 0)
      return program;
  }

  std::unique_ptr<GLProgram> program(new GLProgram());
  if (program->Init(texture_count)) {
    if (programs_.size() < texture_count)
      programs_.resize(texture_count);

    programs_[texture_count - 1] = std::move(program);
    return programs_[texture_count - 1].get();
  }

  return 0;
}

}  // namespace hwcomposer
