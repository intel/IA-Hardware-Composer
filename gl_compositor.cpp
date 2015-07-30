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
#define LOG_TAG "GLCompositor"

#include <vector>

#include <cutils/log.h>

#include <ui/GraphicBuffer.h>
#include <utils/Trace.h>

#include <sync/sync.h>
#include <sw_sync.h>

#include "drm_hwcomposer.h"

#include "gl_compositor.h"
#include "glworker.h"

namespace android {

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
  bool forgotten;
  unsigned composition_count;

  GLTarget() : forgotten(true), composition_count(0) {
  }

  void Reset() {
    fb.clear();
    forgotten = true;
    composition_count = 0;
  }

  bool is_some() const {
    return fb != NULL;
  }
};

struct GLCompositor::priv_data {
  int current_target;
  std::vector<GLTarget> targets;
  std::vector<GLComposition *> compositions;

  GLWorker worker;

  priv_data() : current_target(-1) {
  }
};

class GLComposition : public Composition {
 public:
  struct LayerData {
    hwc_layer_1 layer;
    hwc_drm_bo bo;
  };

  GLComposition(GLCompositor *owner, Importer *imp)
      : compositor(owner), importer(imp), target_handle(-1), timeline_fd(-1) {
    int ret = sw_sync_timeline_create();
    if (ret < 0) {
      ALOGE("Failed to create sw sync timeline %d", ret);
    }
    timeline_fd = ret;
  }

  virtual ~GLComposition() {
    if (timeline_fd >= 0)
      close(timeline_fd);

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

    layer->releaseFenceFd = sw_sync_fence_create(
        timeline_fd, "GLComposition release fence", layers.size() + 1);

    layers.push_back(*layer);

    return importer->ReleaseBuffer(bo);
  }

  virtual unsigned GetRemainingLayers(int display, unsigned num_needed) const {
    (void)display;
    return num_needed;
  }

  GLCompositor *compositor;
  Importer *importer;
  int target_handle;
  int timeline_fd;
  std::vector<hwc_layer_1> layers;
};

GLCompositor::GLCompositor() {
  priv_ = new priv_data;
}

GLCompositor::~GLCompositor() {
  for (std::vector<GLComposition *>::iterator it = priv_->compositions.end();
       it != priv_->compositions.begin(); it = priv_->compositions.end()) {
    --it;

    // Prevents compositor from trying to erase itself
    (*it)->compositor = NULL;
    delete *it;
    priv_->compositions.erase(it);
  }

  delete priv_;
}

int GLCompositor::Init() {
  return priv_->worker.Init();
}

Targeting *GLCompositor::targeting() {
  return (Targeting *)this;
}

int GLCompositor::CreateTarget(sp<GraphicBuffer> &buffer) {
  int ret;

  int target_handle = AllocResource(priv_->targets);
  GLTarget *target = &priv_->targets[target_handle];

  target->fb = buffer;
  target->forgotten = false;

  return target_handle;
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
    GLComposition *gl_composition = (GLComposition *)composition;
    int ret = DoComposition(gl_composition);
    gl_composition->timeline_fd = -1;
    delete composition;
    return ret;
  }

  ALOGE("Failed to queue composition because of invalid composition handle");

  return -EINVAL;
}

int GLCompositor::Composite() {
  return 0;
}

int GLCompositor::DoComposition(GLComposition *composition) {
  ATRACE_CALL();
  int ret = 0;

  GLTarget *target = &priv_->targets[composition->target_handle];
  GLWorker::Work work;
  work.layers = composition->layers.data();
  work.num_layers = composition->layers.size();
  work.timeline_fd = composition->timeline_fd;
  work.framebuffer = target->fb;

  ret = priv_->worker.DoWork(&work);

  if (work.timeline_fd >= 0) {
    sw_sync_timeline_inc(work.timeline_fd, work.num_layers + 1);
    close(work.timeline_fd);
  }

  return ret;
}

}  // namespace android
