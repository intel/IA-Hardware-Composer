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

#include "drmscopedtypes.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace hwcomposer {
void DrmResourcesDeleter::operator()(drmModeRes* resources) const {
  drmModeFreeResources(resources);
}

void DrmConnectorDeleter::operator()(drmModeConnector* connector) const {
  drmModeFreeConnector(connector);
}

void DrmCrtcDeleter::operator()(drmModeCrtc* crtc) const {
  drmModeFreeCrtc(crtc);
}

void DrmEncoderDeleter::operator()(drmModeEncoder* encoder) const {
  drmModeFreeEncoder(encoder);
}

void DrmObjectPropertiesDeleter::operator()(
    drmModeObjectProperties* properties) const {
  drmModeFreeObjectProperties(properties);
}

void DrmPlaneDeleter::operator()(drmModePlane* plane) const {
  drmModeFreePlane(plane);
}

void DrmPlaneResDeleter::operator()(drmModePlaneRes* plane) const {
  drmModeFreePlaneResources(plane);
}

void DrmPropertyDeleter::operator()(drmModePropertyRes* property) const {
  drmModeFreeProperty(property);
}

void DrmAtomicReqDeleter::operator()(drmModeAtomicReq* property) const {
  drmModeAtomicFree(property);
}

}  // namespace hwcomposer
