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

#ifndef DRM_HWCOMPOSER_COMPOSITOR_H_
#define DRM_HWCOMPOSER_COMPOSITOR_H_

#include "importer.h"

#include <sstream>

struct hwc_layer_1;
struct hwc_drm_bo;

namespace android {

class GraphicBuffer;
template <typename T>
class sp;

class Targeting {
 public:
  // Prepares the given framebuffer for use as output of this compositor. On
  // success, takes a reference to the given buffer and returns a non- negative
  // integer that is used as a handle to the prepared target. On failure,
  // returns a negative integer.
  virtual int CreateTarget(sp<android::GraphicBuffer> &buffer) = 0;

  // Sets the target framebuffer of all subsequent composite calls. The target
  // must be an integer previously returned by a successful call to createTarget
  // of this compositor or the target can be -1 to indicate that no custom
  // buffer should be used for subsequent calls.
  virtual void SetTarget(int target) = 0;

  // Releases the reference to the buffer underlying the given target. The given
  // target will no longer be valid for use for setTarget. Calling this on a
  // target that was used in the last setTarget call or that is the target of a
  // composition that has not yet signaled its fence is undefined behavior.
  virtual void ForgetTarget(int target) = 0;

 protected:
  ~Targeting();
};

class Composition {
 public:
  // Releases and invalidates the composition.
  virtual ~Composition();

  // Adds the given layer, whose handle has been imported into the given buffer
  // object, to the given display of the composition. The layer may be modified
  // to include a releaseFenceFd.
  //
  // Upon success, the compositor takes ownership of bo and is responsible
  // for calling importer->ReleaseBuffer(bo), where importer is the importer
  // provided on CreateComposition(). Returns 0 on success.
  virtual int AddLayer(int display, hwc_layer_1 *layer, hwc_drm_bo *bo) = 0;

  // Gets the number of successful AddLayer calls that can be made on the
  // composition and display, up to num_needed.
  virtual unsigned GetRemainingLayers(int display,
                                      unsigned num_needed) const = 0;
};

class Compositor {
 public:
  virtual ~Compositor();

  // This must be called once before any other methods called. It must be called
  // on the thread the Compositor is meant to operate on to initialize thread
  // local variables. Returns 0 on success.
  virtual int Init() = 0;

  // If this compositor supports targeting to output buffers, this returns a
  // non-null pointer. Otherwise, returns null.
  virtual Targeting *targeting() = 0;

  // Starts a fresh composition.
  virtual Composition *CreateComposition(Importer *importer) = 0;

  // On success returns a syncpoint fd that will be signaled when composition is
  // complete or -1 if compositing was completed by this method's return. On
  // error returns an integer less than -1. The composition is invalid after
  // this call.
  virtual int QueueComposition(Composition *composition) = 0;

  // compositors require that every QueueComposition be paired with a Composite
  // on a worker thread. Each Composite call handles one composition that was
  // submitted via QueueComposition in FIFO order. Returns 0 on success.
  virtual int Composite() = 0;

  // Dumps state from the Compositor to the out stream
  virtual void Dump(std::ostringstream *out) const;
};

}  // namespace android

#endif
