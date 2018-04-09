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

#ifndef WSI_DISPLAYPLANE_H_
#define WSI_DISPLAYPLANE_H_

#include <stdlib.h>
#include <stdint.h>

namespace hwcomposer {

struct OverlayLayer;

class DisplayPlane {
 public:
  virtual ~DisplayPlane() {
  }

  virtual uint32_t id() const = 0;

  virtual bool ValidateLayer(const OverlayLayer* layer) = 0;

  virtual bool IsSupportedFormat(uint32_t format) = 0;

  /**
   * API for querying if transform is supported by this
   * plane.
   */
  virtual bool IsSupportedTransform(uint32_t transform) const = 0;

  /**
   * API for querying preferred Video format supported by this
   * plane.
   */
  virtual uint32_t GetPreferredVideoFormat() const = 0;

  /**
   * API for querying preferred format supported by this
   * plane for non-media content.
   */
  virtual uint32_t GetPreferredFormat() const = 0;

  /**
   * API for querying preferred modifier supported by this
   * plane's preferred format for non-media content.
   */
  virtual uint64_t GetPreferredFormatModifier() const = 0;

  /**
   * API for blacklisting preferred format modifier.
   * This happens in case we failed to create FB for the
   * buffer.
   */
  virtual void BlackListPreferredFormatModifier() = 0;

  /**
   * API for informing Display Plane that
   * preferred format modifier has been validated
   * to work by DisplayPlaneManager. If this is not
   * called before BlackListPreferredFormatModifier
   * than PreferredFormatModifier should be set to 0.
   */
  virtual void PreferredFormatModifierValidated() = 0;

  virtual void SetInUse(bool in_use) = 0;

  virtual bool InUse() const = 0;

  /**
   * API for querying if this plane can support
   * content other than cursor or can be used only
   * for cursor. Should return false if this plane
   * cannot be used for anything else than cursor.
   */
  virtual bool IsUniversal() = 0;

  virtual void Dump() const = 0;
};

}  // namespace hwcomposer
#endif  // WSI_DISPLAYPLANE_H_
