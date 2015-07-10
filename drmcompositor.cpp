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

#define LOG_TAG "hwc-drm-compositor"

#include "drmcompositor.h"
#include "drmdisplaycompositor.h"
#include "drmresources.h"

#include <sstream>
#include <stdlib.h>

#include <cutils/log.h>

namespace android {

DrmCompositor::DrmCompositor(DrmResources *drm) : drm_(drm) {
}

DrmCompositor::~DrmCompositor() {
}

int DrmCompositor::Init() {
  for (DrmResources::ConnectorIter iter = drm_->begin_connectors();
       iter != drm_->end_connectors(); ++iter) {
    int display = (*iter)->display();
    int ret = compositor_map_[display].Init(drm_, display);
    if (ret) {
      ALOGE("Failed to initialize display compositor for %d", display);
      return ret;
    }
  }

  return 0;
}

Composition *DrmCompositor::CreateComposition(Importer *importer) {
  DrmComposition *composition = new DrmComposition(drm_, importer);
  if (!composition) {
    ALOGE("Failed to allocate drm composition");
    return NULL;
  }
  int ret = composition->Init();
  if (ret) {
    ALOGE("Failed to initialize drm composition %d", ret);
    delete composition;
    return NULL;
  }
  return composition;
}

int DrmCompositor::QueueComposition(Composition *composition) {
  DrmComposition *drm_composition = (DrmComposition *)composition;
  for (DrmResources::ConnectorIter iter = drm_->begin_connectors();
       iter != drm_->end_connectors(); ++iter) {
    int display = (*iter)->display();
    int ret = compositor_map_[display].QueueComposition(
        drm_composition->TakeDisplayComposition(display));
    if (ret) {
      ALOGE("Failed to queue composition for display %d", display);
      delete composition;
      return ret;
    }
  }

  return 0;
}

int DrmCompositor::Composite() {
  /*
   * This shouldn't be called, we should be calling Composite() on the display
   * compositors directly.
   */
  ALOGE("Calling base drm compositor Composite() function");
  return -EINVAL;
}

void DrmCompositor::Dump(std::ostringstream *out) const {
  *out << "DrmCompositor stats:\n";
  for (DrmResources::ConnectorIter iter = drm_->begin_connectors();
       iter != drm_->end_connectors(); ++iter)
    compositor_map_[(*iter)->display()].Dump(out);
}
}
