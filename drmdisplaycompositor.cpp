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
#define LOG_TAG "hwc-drm-display-compositor"

#include "drmdisplaycompositor.h"

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <sstream>
#include <vector>

#include <log/log.h>
#include <drm/drm_mode.h>
#include <sync/sync.h>
#include <utils/Trace.h>

#include "autolock.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"

namespace android {

DrmDisplayCompositor::DrmDisplayCompositor()
    : drm_(NULL),
      display_(-1),
      initialized_(false),
      active_(false),
      use_hw_overlays_(true),
      dump_frames_composited_(0),
      dump_last_timestamp_ns_(0) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    return;
  dump_last_timestamp_ns_ = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
}

DrmDisplayCompositor::~DrmDisplayCompositor() {
  if (!initialized_)
    return;

  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

  if (mode_.blob_id)
    drm_->DestroyPropertyBlob(mode_.blob_id);
  if (mode_.old_blob_id)
    drm_->DestroyPropertyBlob(mode_.old_blob_id);

  active_composition_.reset();

  ret = pthread_mutex_unlock(&lock_);
  if (ret)
    ALOGE("Failed to acquire compositor lock %d", ret);

  pthread_mutex_destroy(&lock_);
}

int DrmDisplayCompositor::Init(DrmResources *drm, int display) {
  drm_ = drm;
  display_ = display;

  int ret = pthread_mutex_init(&lock_, NULL);
  if (ret) {
    ALOGE("Failed to initialize drm compositor lock %d\n", ret);
    return ret;
  }

  initialized_ = true;
  return 0;
}

std::unique_ptr<DrmDisplayComposition> DrmDisplayCompositor::CreateComposition()
    const {
  return std::unique_ptr<DrmDisplayComposition>(new DrmDisplayComposition());
}

std::tuple<uint32_t, uint32_t, int>
DrmDisplayCompositor::GetActiveModeResolution() {
  DrmConnector *connector = drm_->GetConnectorForDisplay(display_);
  if (connector == NULL) {
    ALOGE("Failed to determine display mode: no connector for display %d",
          display_);
    return std::make_tuple(0, 0, -ENODEV);
  }

  const DrmMode &mode = connector->active_mode();
  return std::make_tuple(mode.h_display(), mode.v_display(), 0);
}

int DrmDisplayCompositor::DisablePlanes(DrmDisplayComposition *display_comp) {
  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  int ret;
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();
  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->crtc_property().id(), 0) < 0 ||
          drmModeAtomicAddProperty(pset, plane->id(), plane->fb_property().id(),
                                   0) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d disable to pset", plane->id());
      drmModeAtomicFree(pset);
      return ret;
    }
  }

  ret = drmModeAtomicCommit(drm_->fd(), pset, 0, drm_);
  if (ret) {
    ALOGE("Failed to commit pset ret=%d\n", ret);
    drmModeAtomicFree(pset);
    return ret;
  }

  drmModeAtomicFree(pset);
  return 0;
}

int DrmDisplayCompositor::CommitFrame(DrmDisplayComposition *display_comp,
                                      bool test_only) {
  ATRACE_CALL();

  int ret = 0;

  std::vector<DrmHwcLayer> &layers = display_comp->layers();
  std::vector<DrmCompositionPlane> &comp_planes =
      display_comp->composition_planes();
  uint64_t out_fences[drm_->crtcs().size()];

  DrmConnector *connector = drm_->GetConnectorForDisplay(display_);
  if (!connector) {
    ALOGE("Could not locate connector for display %d", display_);
    return -ENODEV;
  }
  DrmCrtc *crtc = drm_->GetCrtcForDisplay(display_);
  if (!crtc) {
    ALOGE("Could not locate crtc for display %d", display_);
    return -ENODEV;
  }

  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  if (crtc->out_fence_ptr_property().id() != 0) {
    ret = drmModeAtomicAddProperty(pset, crtc->id(), crtc->out_fence_ptr_property().id(),
                                   (uint64_t) &out_fences[crtc->pipe()]);
    if (ret < 0) {
      ALOGE("Failed to add OUT_FENCE_PTR property to pset: %d", ret);
      drmModeAtomicFree(pset);
      return ret;
    }
  }

  if (mode_.needs_modeset) {
    ret = drmModeAtomicAddProperty(pset, crtc->id(), crtc->active_property().id(), 1);
    if (ret < 0) {
      ALOGE("Failed to add crtc active to pset\n");
      drmModeAtomicFree(pset);
      return ret;
    }

    ret = drmModeAtomicAddProperty(pset, crtc->id(), crtc->mode_property().id(),
                                   mode_.blob_id) < 0 ||
          drmModeAtomicAddProperty(pset, connector->id(),
                                   connector->crtc_id_property().id(),
                                   crtc->id()) < 0;
    if (ret) {
      ALOGE("Failed to add blob %d to pset", mode_.blob_id);
      drmModeAtomicFree(pset);
      return ret;
    }
  }

  for (DrmCompositionPlane &comp_plane : comp_planes) {
    DrmPlane *plane = comp_plane.plane();
    DrmCrtc *crtc = comp_plane.crtc();
    std::vector<size_t> &source_layers = comp_plane.source_layers();

    int fb_id = -1;
    int fence_fd = -1;
    hwc_rect_t display_frame;
    hwc_frect_t source_crop;
    uint64_t rotation = 0;
    uint64_t alpha = 0xFFFF;

    if (comp_plane.type() != DrmCompositionPlane::Type::kDisable) {
      if (source_layers.size() > 1) {
        ALOGE("Can't handle more than one source layer sz=%zu type=%d",
              source_layers.size(), comp_plane.type());
        continue;
      }

      if (source_layers.empty() || source_layers.front() >= layers.size()) {
        ALOGE("Source layer index %zu out of bounds %zu type=%d",
              source_layers.front(), layers.size(), comp_plane.type());
        break;
      }
      DrmHwcLayer &layer = layers[source_layers.front()];
      if (!layer.buffer) {
        ALOGE("Expected a valid framebuffer for pset");
        break;
      }
      fb_id = layer.buffer->fb_id;
      fence_fd = layer.acquire_fence.get();
      display_frame = layer.display_frame;
      source_crop = layer.source_crop;
      if (layer.blending == DrmHwcBlending::kPreMult)
        alpha = layer.alpha;

      rotation = 0;
      if (layer.transform & DrmHwcTransform::kFlipH)
        rotation |= DRM_MODE_REFLECT_X;
      if (layer.transform & DrmHwcTransform::kFlipV)
        rotation |= DRM_MODE_REFLECT_Y;
      if (layer.transform & DrmHwcTransform::kRotate90)
        rotation |= DRM_MODE_ROTATE_90;
      else if (layer.transform & DrmHwcTransform::kRotate180)
        rotation |= DRM_MODE_ROTATE_180;
      else if (layer.transform & DrmHwcTransform::kRotate270)
        rotation |= DRM_MODE_ROTATE_270;
      else
        rotation |= DRM_MODE_ROTATE_0;

      if (fence_fd >= 0) {
        int prop_id = plane->in_fence_fd_property().id();
        if (prop_id == 0) {
                ALOGE("Failed to get IN_FENCE_FD property id");
                break;
        }
        ret = drmModeAtomicAddProperty(pset, plane->id(), prop_id, fence_fd);
        if (ret < 0) {
          ALOGE("Failed to add IN_FENCE_FD property to pset: %d", ret);
          break;
        }
      }
    }

    // Disable the plane if there's no framebuffer
    if (fb_id < 0) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->crtc_property().id(), 0) < 0 ||
            drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->fb_property().id(), 0) < 0;
      if (ret) {
        ALOGE("Failed to add plane %d disable to pset", plane->id());
        break;
      }
      continue;
    }

    // TODO: Once we have atomic test, this should fall back to GL
    if (rotation != DRM_MODE_ROTATE_0 && plane->rotation_property().id() == 0) {
      ALOGV("Rotation is not supported on plane %d", plane->id());
      ret = -EINVAL;
      break;
    }

    // TODO: Once we have atomic test, this should fall back to GL
    if (alpha != 0xFFFF && plane->alpha_property().id() == 0) {
      ALOGV("Alpha is not supported on plane %d", plane->id());
      ret = -EINVAL;
      break;
    }

    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->crtc_property().id(), crtc->id()) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->fb_property().id(), fb_id) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_x_property().id(),
                                    display_frame.left) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_y_property().id(),
                                    display_frame.top) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->crtc_w_property().id(),
               display_frame.right - display_frame.left) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->crtc_h_property().id(),
               display_frame.bottom - display_frame.top) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_x_property().id(),
                                    (int)(source_crop.left) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_y_property().id(),
                                    (int)(source_crop.top) << 16) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->src_w_property().id(),
               (int)(source_crop.right - source_crop.left) << 16) < 0;
    ret |= drmModeAtomicAddProperty(
               pset, plane->id(), plane->src_h_property().id(),
               (int)(source_crop.bottom - source_crop.top) << 16) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d to set", plane->id());
      break;
    }

    if (plane->rotation_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->rotation_property().id(),
                                     rotation) < 0;
      if (ret) {
        ALOGE("Failed to add rotation property %d to plane %d",
              plane->rotation_property().id(), plane->id());
        break;
      }
    }

    if (plane->alpha_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->alpha_property().id(),
                                     alpha) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->alpha_property().id(), plane->id());
        break;
      }
    }
  }

  if (!ret) {
    uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
    if (test_only)
      flags |= DRM_MODE_ATOMIC_TEST_ONLY;

    ret = drmModeAtomicCommit(drm_->fd(), pset, flags, drm_);
    if (ret) {
      if (!test_only)
        ALOGE("Failed to commit pset ret=%d\n", ret);
      drmModeAtomicFree(pset);
      return ret;
    }
  }
  if (pset)
    drmModeAtomicFree(pset);

  if (!test_only && mode_.needs_modeset) {
    ret = drm_->DestroyPropertyBlob(mode_.old_blob_id);
    if (ret) {
      ALOGE("Failed to destroy old mode property blob %" PRIu32 "/%d",
            mode_.old_blob_id, ret);
      return ret;
    }

    /* TODO: Add dpms to the pset when the kernel supports it */
    ret = ApplyDpms(display_comp);
    if (ret) {
      ALOGE("Failed to apply DPMS after modeset %d\n", ret);
      return ret;
    }

    connector->set_active_mode(mode_.mode);
    mode_.old_blob_id = mode_.blob_id;
    mode_.blob_id = 0;
    mode_.needs_modeset = false;
  }

  if (crtc->out_fence_ptr_property().id()) {
    display_comp->set_out_fence((int) out_fences[crtc->pipe()]);
  }

  return ret;
}

int DrmDisplayCompositor::ApplyDpms(DrmDisplayComposition *display_comp) {
  DrmConnector *conn = drm_->GetConnectorForDisplay(display_);
  if (!conn) {
    ALOGE("Failed to get DrmConnector for display %d", display_);
    return -ENODEV;
  }

  const DrmProperty &prop = conn->dpms_property();
  int ret = drmModeConnectorSetProperty(drm_->fd(), conn->id(), prop.id(),
                                        display_comp->dpms_mode());
  if (ret) {
    ALOGE("Failed to set DPMS property for connector %d", conn->id());
    return ret;
  }
  return 0;
}

std::tuple<int, uint32_t> DrmDisplayCompositor::CreateModeBlob(
    const DrmMode &mode) {
  struct drm_mode_modeinfo drm_mode;
  memset(&drm_mode, 0, sizeof(drm_mode));
  mode.ToDrmModeModeInfo(&drm_mode);

  uint32_t id = 0;
  int ret = drm_->CreatePropertyBlob(&drm_mode,
                                     sizeof(struct drm_mode_modeinfo), &id);
  if (ret) {
    ALOGE("Failed to create mode property blob %d", ret);
    return std::make_tuple(ret, 0);
  }
  ALOGE("Create blob_id %" PRIu32 "\n", id);
  return std::make_tuple(ret, id);
}

void DrmDisplayCompositor::ClearDisplay() {
  AutoLock lock(&lock_, "compositor");
  int ret = lock.Lock();
  if (ret)
    return;

  if (!active_composition_)
    return;

  if (DisablePlanes(active_composition_.get()))
    return;

  active_composition_.reset(NULL);
}

void DrmDisplayCompositor::ApplyFrame(
    std::unique_ptr<DrmDisplayComposition> composition, int status) {
  int ret = status;

  if (!ret)
    ret = CommitFrame(composition.get(), false);

  if (ret) {
    ALOGE("Composite failed for display %d", display_);
    // Disable the hw used by the last active composition. This allows us to
    // signal the release fences from that composition to avoid hanging.
    ClearDisplay();
    return;
  }
  ++dump_frames_composited_;

  ret = pthread_mutex_lock(&lock_);
  if (ret)
    ALOGE("Failed to acquire lock for active_composition swap");

  active_composition_.swap(composition);

  if (!ret)
    ret = pthread_mutex_unlock(&lock_);
  if (ret)
    ALOGE("Failed to release lock for active_composition swap");
}

int DrmDisplayCompositor::ApplyComposition(
    std::unique_ptr<DrmDisplayComposition> composition) {
  int ret = 0;
  switch (composition->type()) {
    case DRM_COMPOSITION_TYPE_FRAME:
      if (composition->geometry_changed()) {
        // Send the composition to the kernel to ensure we can commit it. This
        // is just a test, it won't actually commit the frame.
        ret = CommitFrame(composition.get(), true);
        if (ret) {
          ALOGE("Commit test failed for display %d, FIXME", display_);
          return ret;
        }
      }

      ApplyFrame(std::move(composition), ret);
      break;
    case DRM_COMPOSITION_TYPE_DPMS:
      active_ = (composition->dpms_mode() == DRM_MODE_DPMS_ON);
      ret = ApplyDpms(composition.get());
      if (ret)
        ALOGE("Failed to apply dpms for display %d", display_);
      return ret;
    case DRM_COMPOSITION_TYPE_MODESET:
      mode_.mode = composition->display_mode();
      if (mode_.blob_id)
        drm_->DestroyPropertyBlob(mode_.blob_id);
      std::tie(ret, mode_.blob_id) = CreateModeBlob(mode_.mode);
      if (ret) {
        ALOGE("Failed to create mode blob for display %d", display_);
        return ret;
      }
      mode_.needs_modeset = true;
      return 0;
    default:
      ALOGE("Unknown composition type %d", composition->type());
      return -EINVAL;
  }

  return ret;
}

int DrmDisplayCompositor::TestComposition(DrmDisplayComposition *composition) {
  return CommitFrame(composition, true);
}

void DrmDisplayCompositor::Dump(std::ostringstream *out) const {
  int ret = pthread_mutex_lock(&lock_);
  if (ret)
    return;

  uint64_t num_frames = dump_frames_composited_;
  dump_frames_composited_ = 0;

  struct timespec ts;
  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret) {
    pthread_mutex_unlock(&lock_);
    return;
  }

  uint64_t cur_ts = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  uint64_t num_ms = (cur_ts - dump_last_timestamp_ns_) / (1000 * 1000);
  float fps = num_ms ? (num_frames * 1000.0f) / (num_ms) : 0.0f;

  *out << "--DrmDisplayCompositor[" << display_
       << "]: num_frames=" << num_frames << " num_ms=" << num_ms
       << " fps=" << fps << "\n";

  dump_last_timestamp_ns_ = cur_ts;

  pthread_mutex_unlock(&lock_);
}
}
