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
class DisplayPlaneManager;
class DisplayPlaneState;
class NativeSurface;
struct OverlayLayer;

typedef std::vector<DisplayPlaneState> DisplayPlaneStateList;

class DisplayPlaneState {
 public:
  enum ReValidationType {
    kNone = 0,              // No Revalidation Needed.
    kScanout = 1 << 0,      // Check if layer can be scanned out directly.
    kUpScalar = 1 << 1,     // Check if layer can use plane scalar.
    kDownScaling = 1 << 2,  // Check if layer can be downscaled.
    kRotation = 1 << 3      // Check if display transform can be supported.
  };

  enum class RotationType : int32_t {
    kDisplayRotation,  // Plane will be rotated during display composition.
    kGPURotation       // Plane will be rotated during 3D composition.
  };

  DisplayPlaneState() = default;
  DisplayPlaneState(DisplayPlaneState &&rhs) = default;
  DisplayPlaneState &operator=(DisplayPlaneState &&other) = default;
  DisplayPlaneState(DisplayPlane *plane, OverlayLayer *layer,
                    DisplayPlaneManager *plane_manager, uint32_t index,
                    uint32_t plane_transform);

  // Copies plane state from state.
  void CopyState(DisplayPlaneState &state);

  void AddLayer(const OverlayLayer *layer);

  // This API should be called only when source_layers being
  // shown by this plane might be removed in this frame.
  void ResetLayers(const std::vector<OverlayLayer> &layers, size_t remove_index,
                   bool *rects_updated);

  // Updates display frame and source rects combined region for
  // this plane.
  void RefreshLayerRects(const std::vector<OverlayLayer> &layers);

  // Forces GPU Rendering of content for this plane.
  void ForceGPURendering();

  void DisableGPURendering();

  // Set's layer to be scanned out for this plane. This layer
  // can be associated with NativeSurface in case the content
  // need's to be rendered before being scanned out.
  void SetOverlayLayer(const OverlayLayer *layer);

  // SetOffcreen Surface for this plane.
  void SetOffScreenTarget(NativeSurface *target);

  const HwcRect<int> &GetDisplayFrame() const;

  const HwcRect<float> &GetSourceCrop() const;

  const OverlayLayer *GetOverlayLayer() const;

  NativeSurface *GetOffScreenTarget() const;

  // Returns all NativeSurfaces associated with this plane.
  // These can be empty if the plane doesn't need to go
  // through any composition pass before being scanned out.
  const std::vector<NativeSurface *> &GetSurfaces() const;

  // Removes surfaces for this plane. Caller is responsible
  // for ensuring the surfaces are released/recycled.
  void ReleaseSurfaces();

  // Updates all offscreen surfaces to have same display frame
  // and source rect as displayplanestate. Also, resets
  // composition region to null. Also updates if layer
  // needs to use scalar or not depending on what
  // IsUsingPlaneScalar returns. clear_surface determines
  // if all offscreen surfaces of this plane are partially or
  // fully cleared.
  void RefreshSurfaces(NativeSurface::ClearType clear_surface,
                       bool force = false);

  void UpdateDamage(const HwcRect<int> &surface_damage);

  DisplayPlane *GetDisplayPlane() const;

  void SetDisplayPlane(DisplayPlane *plane);

  // Returns source layers for this plane.
  const std::vector<size_t> &GetSourceLayers() const;

  // Returns composition region used by this plane.
  std::vector<CompositionRegion> &GetCompositionRegion();

  // Resets composition region to null.
  void ResetCompositionRegion();

  bool IsCursorPlane() const;

  bool HasCursorLayer() const;

  bool IsVideoPlane() const;

  // Set true if this Plane State is handling Video layer and
  // needs to go through the Media Compositor for any
  // offscreen composition usage.
  void SetVideoPlane(bool enable);

  // Updates plane state to using plane scalar if enable is
  // true. Forces clearing all offscreen surfaces if force_refresh
  // is true. This setting makes sure we set the right source
  // crop to the surface layer.
  void UsePlaneScalar(bool enable, bool force_refresh = true);

  // Returns true if we intend to use display scalar with this
  // plane.
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
  bool NeedsOffScreenComposition() const;

  // Returns type of validation needed by the plane
  // with current source layer. This will be the case
  // when plane had multiple layers and they where
  // removed leaving it with single layer now.
  uint32_t RevalidationType() const;

  // Plane has been revalidated by DisplayPlaneManager.
  void RevalidationDone(uint32_t validation_done);

  // Call this to determine what kind of re-validation
  // is needed by this plane for this frame.
  void ValidateReValidation();

  // Returns true if we can squash this with other plane.
  // i.e. This will be the case when it's having only
  // gpu composited layer and could potentially be
  // squashed with another having similar composition.
  // This should be used only as a hint.
  bool CanSquash() const;

  // Returns true if we can benefit by using display scalar
  // for this plane.
  // This should be used only as a hint.
  bool CanUseDisplayUpScaling() const;

  // Returns true if we can benefit by using gpu for
  // downscaling content of this plane.
  // This should be used only as a hint.
  bool CanUseGPUDownScaling() const;

  // Set if Plane rotation needs to be handled
  // using GPU or Display.
  void SetRotationType(RotationType type, bool refresh);

  // Returns if Plane rotation is handled by
  // GPU or Display. Return kNone in case
  // plane is not rotated.
  RotationType GetRotationType() const;

  void SetDisplayDownScalingFactor(uint32_t factor, bool clear_surfaces);

  uint32_t GetDownScalingFactor() const;

  // Helper to check if we need to allocate
  // an offscreen surface for this plane.
  bool NeedsSurfaceAllocation() const {
    return needs_surface_allocation_;
  }

  // Put's current OffscreenSurface to back in the
  // list if not already done.
  void SwapSurfaceIfNeeded();

  // Reset's surface age and surface order as previous frame.
  void HandleCommitFailure();

  // Returns true if OffscreenSurface is recycled.
  bool IsSurfaceRecycled() const {
    return recycled_surface_;
  }

  // Returns true in case the associated DisplayPlaneState
  // can support Video.
  bool CanSupportVideo() const {
    return private_data_->supports_video_;
  }

  void Dump();

 private:
  void CalculateSourceCrop(HwcRect<float> &source_crop) const;

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

    ~DisplayPlanePrivateState();

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
    // Can benefit using display scalar.
    bool can_use_display_scalar_ = false;
    // Using GPU for downscaling.
    bool use_down_scaling_ = false;
    // Can benefit by downscaling using
    // GPU.
    bool can_use_downscaling_ = false;
    // Rects(Either Display Frame/Source Rect) have
    // changed.
    bool rect_updated_ = true;

    bool refresh_surface_ = true;

    bool supports_video_ = false;

    // Display cannot support the required rotation.
    bool unsupported_display_rotation_ = false;
    uint32_t down_scaling_factor_ = 1;
    // Any offscreen surfaces used by this
    // plane.
    std::vector<NativeSurface *> surfaces_;
    PlaneType type_ = PlaneType::kNormal;
    uint32_t plane_transform_ = kIdentity;
    RotationType rotation_type_ = RotationType::kDisplayRotation;
    DisplayPlaneManager *plane_manager_ = NULL;
  };

  bool recycled_surface_ = true;
  bool surface_swapped_ = false;
  bool needs_surface_allocation_ = true;
  uint32_t re_validate_layer_ = ReValidationType::kNone;
  std::shared_ptr<DisplayPlanePrivateState> private_data_;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANESTATE_H_
