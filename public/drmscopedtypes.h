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

#ifndef DRM_SCOPED_TYPES_H_
#define DRM_SCOPED_TYPES_H_

#include <memory>
typedef struct _drmModeConnector drmModeConnector;
typedef struct _drmModeCrtc drmModeCrtc;
typedef struct _drmModeEncoder drmModeEncoder;
typedef struct _drmModeFB drmModeFB;
typedef struct _drmModeObjectProperties drmModeObjectProperties;
typedef struct _drmModePlane drmModePlane;
typedef struct _drmModePlaneRes drmModePlaneRes;
typedef struct _drmModeProperty drmModePropertyRes;
typedef struct _drmModeAtomicReq drmModeAtomicReq;
typedef struct _drmModePropertyBlob drmModePropertyBlobRes;
typedef struct _drmModeRes drmModeRes;
typedef struct _drmEventContext drmEventContext;
typedef struct _drmModeModeInfo drmModeModeInfo;

namespace hwcomposer {
struct DrmResourcesDeleter {
  void operator()(drmModeRes* resources) const;
};
struct DrmConnectorDeleter {
  void operator()(drmModeConnector* connector) const;
};
struct DrmCrtcDeleter {
  void operator()(drmModeCrtc* crtc) const;
};
struct DrmEncoderDeleter {
  void operator()(drmModeEncoder* encoder) const;
};
struct DrmObjectPropertiesDeleter {
  void operator()(drmModeObjectProperties* properties) const;
};
struct DrmPlaneDeleter {
  void operator()(drmModePlane* plane) const;
};
struct DrmPlaneResDeleter {
  void operator()(drmModePlaneRes* plane_res) const;
};
struct DrmPropertyDeleter {
  void operator()(drmModePropertyRes* property) const;
};

struct DrmAtomicReqDeleter {
  void operator()(drmModeAtomicReq* property) const;
};

typedef std::unique_ptr<drmModeRes, DrmResourcesDeleter> ScopedDrmResourcesPtr;
typedef std::unique_ptr<drmModeConnector, DrmConnectorDeleter>
    ScopedDrmConnectorPtr;
typedef std::unique_ptr<drmModeCrtc, DrmCrtcDeleter> ScopedDrmCrtcPtr;
typedef std::unique_ptr<drmModeEncoder, DrmEncoderDeleter> ScopedDrmEncoderPtr;
typedef std::unique_ptr<drmModeObjectProperties, DrmObjectPropertiesDeleter>
    ScopedDrmObjectPropertyPtr;
typedef std::unique_ptr<drmModePlane, DrmPlaneDeleter> ScopedDrmPlanePtr;
typedef std::unique_ptr<drmModePlaneRes, DrmPlaneResDeleter>
    ScopedDrmPlaneResPtr;
typedef std::unique_ptr<drmModePropertyRes, DrmPropertyDeleter>
    ScopedDrmPropertyPtr;
typedef std::unique_ptr<drmModeAtomicReq, DrmAtomicReqDeleter>
    ScopedDrmAtomicReqPtr;
}  // namespace hwcomposer
#endif  // DRM_SCOPED_TYPES_H_
