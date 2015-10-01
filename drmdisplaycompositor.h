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

#ifndef ANDROID_DRM_DISPLAY_COMPOSITOR_H_
#define ANDROID_DRM_DISPLAY_COMPOSITOR_H_

#include "drm_hwcomposer.h"
#include "drmcomposition.h"
#include "drmcompositorworker.h"
#include "drmframebuffer.h"

#include <pthread.h>
#include <queue>
#include <sstream>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#define DRM_DISPLAY_BUFFERS 2

namespace android {

class GLWorkerCompositor;

class DrmDisplayCompositor {
 public:
  DrmDisplayCompositor();
  ~DrmDisplayCompositor();

  int Init(DrmResources *drm, int display);

  int QueueComposition(std::unique_ptr<DrmDisplayComposition> composition);
  int Composite();
  void Dump(std::ostringstream *out) const;

  bool HaveQueuedComposites() const;

 private:
  DrmDisplayCompositor(const DrmDisplayCompositor &) = delete;

  // Set to 83ms (~12fps) which is somewhere between a reasonable amount of
  // time to wait for a long render and a small enough delay to limit jank.
  static const int kAcquireWaitTimeoutMs = 83;

  int ApplyPreComposite(DrmDisplayComposition *display_comp);
  int ApplyFrame(DrmDisplayComposition *display_comp);
  int ApplyDpms(DrmDisplayComposition *display_comp);

  DrmResources *drm_;
  int display_;

  DrmCompositorWorker worker_;

  std::queue<std::unique_ptr<DrmDisplayComposition>> composite_queue_;
  std::unique_ptr<DrmDisplayComposition> active_composition_;

  bool initialized_;
  bool active_;

  DrmMode next_mode_;
  bool needs_modeset_;

  int framebuffer_index_;
  DrmFramebuffer framebuffers_[DRM_DISPLAY_BUFFERS];
  std::unique_ptr<GLWorkerCompositor> pre_compositor_;

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t lock_;

  // State tracking progress since our last Dump(). These are mutable since
  // we need to reset them on every Dump() call.
  mutable uint64_t dump_frames_composited_;
  mutable uint64_t dump_last_timestamp_ns_;
};
}

#endif  // ANDROID_DRM_DISPLAY_COMPOSITOR_H_
