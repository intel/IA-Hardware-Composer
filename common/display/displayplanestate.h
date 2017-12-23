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

#ifndef COMMON_DISPLAY_DISPLAYPLANESTATE_H_
#define COMMON_DISPLAY_DISPLAYPLANESTATE_H_

#include <stdint.h>
#include <vector>

#include "compositionregion.h"
#include "displayplane.h"
#include "nativesurface.h"
#include "overlaylayer.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class NativeSurface;
struct OverlayLayer;

typedef std::vector<DisplayPlaneState> DisplayPlaneStateList;

class DisplayPlaneState {
 public:
  DisplayPlaneState() = default;
  DisplayPlaneState(DisplayPlaneState &&rhs) = default;
  DisplayPlaneState &operator=(DisplayPlaneState &&other) = default;
  DisplayPlaneState(DisplayPlane *plane, OverlayLayer *layer, uint32_t index);

  // Copies plane state from state.
  void CopyState(DisplayPlaneState &state);

  void SetSourceCrop(const HwcRect<float> &crop);

  void ResetSourceRectToDisplayFrame();

  void AddLayer(const OverlayLayer *layer);

  // This API should be called only when Cursor layer is being
  // shown by this plane and is removed in this frame.
  void ResetLayers(const std::vector<size_t> &source_layers,
                   const std::vector<OverlayLayer> &layers);

  // Updates Display frame rect of this plane to include
  // display_frame.
  void UpdateDisplayFrame(const HwcRect<int> &display_frame);

  // Forces GPU Rendering of content for this plane.
  void ForceGPURendering();

  // Set's layer to be scanned out for this plane. This layer
  // can be associated with NativeSurface in case the content
  // need's to be rendered before being scanned out.
  void SetOverlayLayer(const OverlayLayer *layer);

  // Reuses last shown surface for current frame.
  void ReUseOffScreenTarget();

  // This will be called by DisplayPlaneManager when adding
  // cursor layer to any existing overlay.
  void SwapSurfaceIfNeeded();

  // SetOffcreen Surface for this plane.
  void SetOffScreenTarget(NativeSurface *target);

  // Put's current OffscreenSurface to back in the
  // list.
  void SwapSurface();

  const HwcRect<int> &GetDisplayFrame() const;

  const HwcRect<float> &GetSourceCrop() const;

  bool SurfaceRecycled() const;

  const OverlayLayer *GetOverlayLayer() const;

  NativeSurface *GetOffScreenTarget() const;

  // Returns all NativeSurfaces associated with this plane.
  // These can be empty if the plane doesn't need to go
  // through any composition pass before being scanned out.
  const std::vector<NativeSurface *> &GetSurfaces() const;

  // Marks all surfaces used by this plane as not in use
  // and removes surfaces from this plane. After this call
  // this plane will not have have any associated offscreen
  // surfaces. If only_release is set to true than we dont
  // change the use flag.
  void ReleaseSurfaces(bool only_release);

  // Updates all offscreen surfaces to have same display frame
  // and source rect as displayplanestate. Also, resets
  // composition region to null. Also updates if layer
  // needs to use scalar or not depending on what
  // IsUsingPlaneScalar returns.
  void RefreshSurfaces(bool clear_surface);

  DisplayPlane *GetDisplayPlane() const;

  // Returns source layers for this plane.
  const std::vector<size_t> &GetSourceLayers() const;

  // Returns composition region used by this plane.
  std::vector<CompositionRegion> &GetCompositionRegion();

  // Resets composition region to null.
  void ResetCompositionRegion();

  bool IsCursorPlane() const;

  bool HasCursorLayer() const;

  bool IsVideoPlane() const;

  void SetVideoPlane();

  void UsePlaneScalar(bool enable);

  bool IsUsingPlaneScalar() const;

  // This state means that the content scanned out
  // by this plane needs to be post processed to
  // take into account any video effects.
  void SetApplyEffects(bool apply_effects);

  // Returns true if layer associated with this
  // plane needs to be processed to apply needed
  // video effects.
  bool ApplyEffects() const;

  // Returns true if layer associated with
  // this plane can be scanned out directly.
  bool Scanout() const;

  // Returns true if offscreen composition
  // is needed for this plane.
  bool NeedsOffScreenComposition();

 private:
  class DisplayPlanePrivateState {
   public:
    enum class PlaneType : int32_t {
      kCursor,  // Plane is compositing only Cursor.
      kVideo,   // Plane is compositing only Media content.
      kNormal   // Plane is compositing different types of content.
    };

    enum class State : int32_t {
      kScanout,  // Scanout the layer directly.
      kRender,   // Needs to render the contents to
                 // layer before scanning out.
    };

    State state_ = State::kScanout;
    DisplayPlane *plane_ = NULL;
    const OverlayLayer *layer_ = NULL;
    HwcRect<int> display_frame_;
    HwcRect<float> source_crop_;
    std::vector<size_t> source_layers_;
    std::vector<CompositionRegion> composition_region_;

    bool use_plane_scalar_ = false;
    // Even if layer can be scanned out
    // directly, post processing for
    // applying video effects is needed.
    bool apply_effects_ = false;
    // This plane shows cursor.
    bool has_cursor_layer_ = false;
    // Any offscreen surfaces used by this
    // plane.
    std::vector<NativeSurface *> surfaces_;
    PlaneType type_ = PlaneType::kNormal;
  };

  bool recycled_surface_ = false;
  bool surface_swapped_ = true;
  std::shared_ptr<DisplayPlanePrivateState> private_data_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANESTATE_H_
