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

#ifndef ANDROID_DRM_COMPOSITOR_H_
#define ANDROID_DRM_COMPOSITOR_H_

#include "compositor.h"
#include "drm_hwcomposer.h"
#include "drmcomposition.h"
#include "drmcompositorworker.h"
#include "drmplane.h"
#include "importer.h"

#include <pthread.h>
#include <queue>
#include <sstream>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

class Drm;

class DrmCompositor : public Compositor {
 public:
  DrmCompositor(DrmResources *drm);
  ~DrmCompositor();

  virtual int Init();

  virtual Targeting *targeting() {
    return NULL;
  }

  virtual Composition *CreateComposition(Importer *importer);

  virtual int QueueComposition(Composition *composition);
  virtual int Composite();
  virtual void Dump(std::ostringstream *out) const;

  bool HaveQueuedComposites() const;

 private:
  DrmCompositor(const DrmCompositor &);

  int CompositeDisplay(DrmCompositionLayerMap_t::iterator begin,
                       DrmCompositionLayerMap_t::iterator end);

  DrmResources *drm_;

  DrmCompositorWorker worker_;

  std::queue<DrmComposition *> composite_queue_;
  DrmComposition *active_composition_;

  uint64_t frame_no_;

  bool initialized_;

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t lock_;

  // State tracking progress since our last Dump(). These are mutable since
  // we need to reset them on every Dump() call.
  mutable uint64_t dump_frames_composited_;
  mutable uint64_t dump_last_timestamp_ns_;
};
}

#endif  // ANDROID_DRM_COMPOSITOR_H_
