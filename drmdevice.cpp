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

#define LOG_TAG "hwc-drm-device"

#include "drmdevice.h"
#include "drmconnector.h"
#include "drmcrtc.h"
#include "drmencoder.h"
#include "drmeventlistener.h"
#include "drmplane.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cinttypes>

#include <cutils/properties.h>
#include <log/log.h>

namespace android {

DrmDevice::DrmDevice() : event_listener_(this) {
}

DrmDevice::~DrmDevice() {
  event_listener_.Exit();
}

std::tuple<int, int> DrmDevice::Init(const char *path, int num_displays) {
  /* TODO: Use drmOpenControl here instead */
  fd_.Set(open(path, O_RDWR));
  if (fd() < 0) {
    ALOGE("Failed to open dri- %s", strerror(-errno));
    return std::make_tuple(-ENODEV, 0);
  }

  int ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  if (ret) {
    ALOGE("Failed to set universal plane cap %d", ret);
    return std::make_tuple(ret, 0);
  }

  ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ALOGE("Failed to set atomic cap %d", ret);
    return std::make_tuple(ret, 0);
  }

#ifdef DRM_CLIENT_CAP_WRITEBACK_CONNECTORS
  ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1);
  if (ret) {
    ALOGI("Failed to set writeback cap %d", ret);
    ret = 0;
  }
#endif

  drmModeResPtr res = drmModeGetResources(fd());
  if (!res) {
    ALOGE("Failed to get DrmDevice resources");
    return std::make_tuple(-ENODEV, 0);
  }

  min_resolution_ = std::pair<uint32_t, uint32_t>(res->min_width,
                                                  res->min_height);
  max_resolution_ = std::pair<uint32_t, uint32_t>(res->max_width,
                                                  res->max_height);

  // Assumes that the primary display will always be in the first
  // drm_device opened.
  bool found_primary = num_displays != 0;

  for (int i = 0; !ret && i < res->count_crtcs; ++i) {
    drmModeCrtcPtr c = drmModeGetCrtc(fd(), res->crtcs[i]);
    if (!c) {
      ALOGE("Failed to get crtc %d", res->crtcs[i]);
      ret = -ENODEV;
      break;
    }

    std::unique_ptr<DrmCrtc> crtc(new DrmCrtc(this, c, i));
    drmModeFreeCrtc(c);

    ret = crtc->Init();
    if (ret) {
      ALOGE("Failed to initialize crtc %d", res->crtcs[i]);
      break;
    }
    crtcs_.emplace_back(std::move(crtc));
  }

  std::vector<int> possible_clones;
  for (int i = 0; !ret && i < res->count_encoders; ++i) {
    drmModeEncoderPtr e = drmModeGetEncoder(fd(), res->encoders[i]);
    if (!e) {
      ALOGE("Failed to get encoder %d", res->encoders[i]);
      ret = -ENODEV;
      break;
    }

    std::vector<DrmCrtc *> possible_crtcs;
    DrmCrtc *current_crtc = NULL;
    for (auto &crtc : crtcs_) {
      if ((1 << crtc->pipe()) & e->possible_crtcs)
        possible_crtcs.push_back(crtc.get());

      if (crtc->id() == e->crtc_id)
        current_crtc = crtc.get();
    }

    std::unique_ptr<DrmEncoder> enc(
        new DrmEncoder(e, current_crtc, possible_crtcs));
    possible_clones.push_back(e->possible_clones);
    drmModeFreeEncoder(e);

    encoders_.emplace_back(std::move(enc));
  }

  for (unsigned int i = 0; i < encoders_.size(); i++) {
    for (unsigned int j = 0; j < encoders_.size(); j++)
      if (possible_clones[i] & (1 << j))
        encoders_[i]->AddPossibleClone(encoders_[j].get());
  }

  for (int i = 0; !ret && i < res->count_connectors; ++i) {
    drmModeConnectorPtr c = drmModeGetConnector(fd(), res->connectors[i]);
    if (!c) {
      ALOGE("Failed to get connector %d", res->connectors[i]);
      ret = -ENODEV;
      break;
    }

    std::vector<DrmEncoder *> possible_encoders;
    DrmEncoder *current_encoder = NULL;
    for (int j = 0; j < c->count_encoders; ++j) {
      for (auto &encoder : encoders_) {
        if (encoder->id() == c->encoders[j])
          possible_encoders.push_back(encoder.get());
        if (encoder->id() == c->encoder_id)
          current_encoder = encoder.get();
      }
    }

    std::unique_ptr<DrmConnector> conn(
        new DrmConnector(this, c, current_encoder, possible_encoders));

    drmModeFreeConnector(c);

    ret = conn->Init();
    if (ret) {
      ALOGE("Init connector %d failed", res->connectors[i]);
      break;
    }

    if (conn->writeback())
      writeback_connectors_.emplace_back(std::move(conn));
    else
      connectors_.emplace_back(std::move(conn));
  }

  // First look for primary amongst internal connectors
  for (auto &conn : connectors_) {
    if (conn->internal() && !found_primary) {
      conn->set_display(num_displays);
      displays_[num_displays] = num_displays;
      ++num_displays;
      found_primary = true;
      break;
    }
  }

  // Then pick first available as primary and for the others assign
  // consecutive display_numbers.
  for (auto &conn : connectors_) {
    if (conn->external() || conn->internal()) {
      if (!found_primary) {
        conn->set_display(num_displays);
        displays_[num_displays] = num_displays;
        found_primary = true;
        ++num_displays;
      } else if (conn->display() < 0) {
        conn->set_display(num_displays);
        displays_[num_displays] = num_displays;
        ++num_displays;
      }
    }
  }

  if (res)
    drmModeFreeResources(res);

  // Catch-all for the above loops
  if (ret)
    return std::make_tuple(ret, 0);

  drmModePlaneResPtr plane_res = drmModeGetPlaneResources(fd());
  if (!plane_res) {
    ALOGE("Failed to get plane resources");
    return std::make_tuple(-ENOENT, 0);
  }

  for (uint32_t i = 0; i < plane_res->count_planes; ++i) {
    drmModePlanePtr p = drmModeGetPlane(fd(), plane_res->planes[i]);
    if (!p) {
      ALOGE("Failed to get plane %d", plane_res->planes[i]);
      ret = -ENODEV;
      break;
    }

    std::unique_ptr<DrmPlane> plane(new DrmPlane(this, p));

    drmModeFreePlane(p);

    ret = plane->Init();
    if (ret) {
      ALOGE("Init plane %d failed", plane_res->planes[i]);
      break;
    }

    planes_.emplace_back(std::move(plane));
  }
  drmModeFreePlaneResources(plane_res);
  if (ret)
    return std::make_tuple(ret, 0);

  ret = event_listener_.Init();
  if (ret) {
    ALOGE("Can't initialize event listener %d", ret);
    return std::make_tuple(ret, 0);
  }

  for (auto &conn : connectors_) {
    ret = CreateDisplayPipe(conn.get());
    if (ret) {
      ALOGE("Failed CreateDisplayPipe %d with %d", conn->id(), ret);
      return std::make_tuple(ret, 0);
    }
    if (!AttachWriteback(conn.get())) {
      ALOGI("Display %d has writeback attach to it", conn->display());
    }
  }
  return std::make_tuple(ret, displays_.size());
}

bool DrmDevice::HandlesDisplay(int display) const {
  return displays_.find(display) != displays_.end();
}

DrmConnector *DrmDevice::GetConnectorForDisplay(int display) const {
  for (auto &conn : connectors_) {
    if (conn->display() == display)
      return conn.get();
  }
  return NULL;
}

DrmConnector *DrmDevice::GetWritebackConnectorForDisplay(int display) const {
  for (auto &conn : writeback_connectors_) {
    if (conn->display() == display)
      return conn.get();
  }
  return NULL;
}

// TODO what happens when hotplugging
DrmConnector *DrmDevice::AvailableWritebackConnector(int display) const {
  DrmConnector *writeback_conn = GetWritebackConnectorForDisplay(display);
  DrmConnector *display_conn = GetConnectorForDisplay(display);
  // If we have a writeback already attached to the same CRTC just use that,
  // if possible.
  if (display_conn && writeback_conn &&
      writeback_conn->encoder()->CanClone(display_conn->encoder()))
    return writeback_conn;

  // Use another CRTC if available and doesn't have any connector
  for (auto &crtc : crtcs_) {
    if (crtc->display() == display)
      continue;
    display_conn = GetConnectorForDisplay(crtc->display());
    // If we have a display connected don't use it for writeback
    if (display_conn && display_conn->state() == DRM_MODE_CONNECTED)
      continue;
    writeback_conn = GetWritebackConnectorForDisplay(crtc->display());
    if (writeback_conn)
      return writeback_conn;
  }
  return NULL;
}

DrmCrtc *DrmDevice::GetCrtcForDisplay(int display) const {
  for (auto &crtc : crtcs_) {
    if (crtc->display() == display)
      return crtc.get();
  }
  return NULL;
}

DrmPlane *DrmDevice::GetPlane(uint32_t id) const {
  for (auto &plane : planes_) {
    if (plane->id() == id)
      return plane.get();
  }
  return NULL;
}

const std::vector<std::unique_ptr<DrmCrtc>> &DrmDevice::crtcs() const {
  return crtcs_;
}

uint32_t DrmDevice::next_mode_id() {
  return ++mode_id_;
}

int DrmDevice::TryEncoderForDisplay(int display, DrmEncoder *enc) {
  /* First try to use the currently-bound crtc */
  DrmCrtc *crtc = enc->crtc();
  if (crtc && crtc->can_bind(display)) {
    crtc->set_display(display);
    enc->set_crtc(crtc);
    return 0;
  }

  /* Try to find a possible crtc which will work */
  for (DrmCrtc *crtc : enc->possible_crtcs()) {
    /* We've already tried this earlier */
    if (crtc == enc->crtc())
      continue;

    if (crtc->can_bind(display)) {
      crtc->set_display(display);
      enc->set_crtc(crtc);
      return 0;
    }
  }

  /* We can't use the encoder, but nothing went wrong, try another one */
  return -EAGAIN;
}

int DrmDevice::CreateDisplayPipe(DrmConnector *connector) {
  int display = connector->display();
  /* Try to use current setup first */
  if (connector->encoder()) {
    int ret = TryEncoderForDisplay(display, connector->encoder());
    if (!ret) {
      return 0;
    } else if (ret != -EAGAIN) {
      ALOGE("Could not set mode %d/%d", display, ret);
      return ret;
    }
  }

  for (DrmEncoder *enc : connector->possible_encoders()) {
    int ret = TryEncoderForDisplay(display, enc);
    if (!ret) {
      connector->set_encoder(enc);
      return 0;
    } else if (ret != -EAGAIN) {
      ALOGE("Could not set mode %d/%d", display, ret);
      return ret;
    }
  }
  ALOGE("Could not find a suitable encoder/crtc for display %d",
        connector->display());
  return -ENODEV;
}

// Attach writeback connector to the CRTC linked to the display_conn
int DrmDevice::AttachWriteback(DrmConnector *display_conn) {
  DrmCrtc *display_crtc = display_conn->encoder()->crtc();
  if (GetWritebackConnectorForDisplay(display_crtc->display()) != NULL) {
    ALOGE("Display already has writeback attach to it");
    return -EINVAL;
  }
  for (auto &writeback_conn : writeback_connectors_) {
    if (writeback_conn->display() >= 0)
      continue;
    for (DrmEncoder *writeback_enc : writeback_conn->possible_encoders()) {
      for (DrmCrtc *possible_crtc : writeback_enc->possible_crtcs()) {
        if (possible_crtc != display_crtc)
          continue;
        // Use just encoders which had not been bound already
        if (writeback_enc->can_bind(display_crtc->display())) {
          writeback_enc->set_crtc(display_crtc);
          writeback_conn->set_encoder(writeback_enc);
          writeback_conn->set_display(display_crtc->display());
          writeback_conn->UpdateModes();
          return 0;
        }
      }
    }
  }
  return -EINVAL;
}

int DrmDevice::CreatePropertyBlob(void *data, size_t length,
                                  uint32_t *blob_id) {
  struct drm_mode_create_blob create_blob;
  memset(&create_blob, 0, sizeof(create_blob));
  create_blob.length = length;
  create_blob.data = (__u64)data;

  int ret = drmIoctl(fd(), DRM_IOCTL_MODE_CREATEPROPBLOB, &create_blob);
  if (ret) {
    ALOGE("Failed to create mode property blob %d", ret);
    return ret;
  }
  *blob_id = create_blob.blob_id;
  return 0;
}

int DrmDevice::DestroyPropertyBlob(uint32_t blob_id) {
  if (!blob_id)
    return 0;

  struct drm_mode_destroy_blob destroy_blob;
  memset(&destroy_blob, 0, sizeof(destroy_blob));
  destroy_blob.blob_id = (__u32)blob_id;
  int ret = drmIoctl(fd(), DRM_IOCTL_MODE_DESTROYPROPBLOB, &destroy_blob);
  if (ret) {
    ALOGE("Failed to destroy mode property blob %" PRIu32 "/%d", blob_id, ret);
    return ret;
  }
  return 0;
}

DrmEventListener *DrmDevice::event_listener() {
  return &event_listener_;
}

int DrmDevice::GetProperty(uint32_t obj_id, uint32_t obj_type,
                           const char *prop_name, DrmProperty *property) {
  drmModeObjectPropertiesPtr props;

  props = drmModeObjectGetProperties(fd(), obj_id, obj_type);
  if (!props) {
    ALOGE("Failed to get properties for %d/%x", obj_id, obj_type);
    return -ENODEV;
  }

  bool found = false;
  for (int i = 0; !found && (size_t)i < props->count_props; ++i) {
    drmModePropertyPtr p = drmModeGetProperty(fd(), props->props[i]);
    if (!strcmp(p->name, prop_name)) {
      property->Init(p, props->prop_values[i]);
      found = true;
    }
    drmModeFreeProperty(p);
  }

  drmModeFreeObjectProperties(props);
  return found ? 0 : -ENOENT;
}

int DrmDevice::GetPlaneProperty(const DrmPlane &plane, const char *prop_name,
                                DrmProperty *property) {
  return GetProperty(plane.id(), DRM_MODE_OBJECT_PLANE, prop_name, property);
}

int DrmDevice::GetCrtcProperty(const DrmCrtc &crtc, const char *prop_name,
                               DrmProperty *property) {
  return GetProperty(crtc.id(), DRM_MODE_OBJECT_CRTC, prop_name, property);
}

int DrmDevice::GetConnectorProperty(const DrmConnector &connector,
                                    const char *prop_name,
                                    DrmProperty *property) {
  return GetProperty(connector.id(), DRM_MODE_OBJECT_CONNECTOR, prop_name,
                     property);
}
}  // namespace android
