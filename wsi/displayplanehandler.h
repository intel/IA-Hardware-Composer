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

#ifndef WSI_DISPLAYPLANEHANDLER_H_
#define WSI_DISPLAYPLANEHANDLER_H_

namespace hwcomposer {

class DisplayPlane;
struct OverlayLayer;

struct OverlayPlane {
 public:
  OverlayPlane(DisplayPlane* plane, const OverlayLayer* layer)
      : plane(plane), layer(layer) {
  }
  DisplayPlane* plane;
  const OverlayLayer* layer;
};

class DisplayPlaneHandler {
 public:
  virtual ~DisplayPlaneHandler() {
  }

  virtual bool PopulatePlanes(
      std::unique_ptr<DisplayPlane>& primary_plane,
      std::unique_ptr<DisplayPlane>& cursor_plane,
      std::vector<std::unique_ptr<DisplayPlane>>& overlay_planes) = 0;

  virtual bool TestCommit(
      const std::vector<OverlayPlane>& commit_planes) const = 0;
};

}  // namespace hwcomposer
#endif  // WSI_DISPLAYPLANEHANDLER_H_
