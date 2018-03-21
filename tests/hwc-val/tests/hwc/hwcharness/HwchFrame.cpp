/*
// Copyright (c) 2018 Intel Corporation
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

#include <platformdefines.h>

#include "HwchDefs.h"
#include "HwchFrame.h"
#include "HwchSystem.h"
#include "HwchPattern.h"
#include "HwcTestReferenceComposer.h"
#include "HwchInterface.h"
#include "HwcTestLog.h"
#include "HwcTestState.h"
#include "HwcTestUtil.h"
#include "HwcTestUtil.h"  // for sync headers
#include "HwchLayers.h"

static HwcTestReferenceComposer sRefCmp;

uint32_t Hwch::Frame::mFrameCount = 0;

Hwch::Frame::Frame(Hwch::Interface& interface)
    : mFlags(0),
      mHwcAcquireDelay(0),
      mInterface(interface),
      mSystem(Hwch::System::getInstance()) {
  interface.bufHandler = mSystem.bufferHandler;
  sRefCmp.setBufferHandler(mSystem.bufferHandler);
  Clear();
  RotateTo(hwcomposer::HWCRotation::kRotateNone);
}

int32_t Hwch::Frame::GetIndexOfCloneFromLayerList(
    const Hwch::Frame::LayerList list, const Hwch::Layer* layer) {
  // Layer lists don't tend to be 'big' so search linearly
  for (uint32_t index = 0; index < list.size(); ++index) {
    if (list[index]->mIsACloneOf == layer) {
      return index;
    }
  }

  return -1;
}

// Frame copy constructor
Hwch::Frame::Frame(const Hwch::Frame& rhs)
    : mFlags(rhs.mFlags),
      mHwcAcquireDelay(rhs.mHwcAcquireDelay),
      mInterface(rhs.mInterface),
      mSystem(rhs.mSystem) {
  Clear();

  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    mNumFBLayers[i] = rhs.mNumFBLayers[i];
    mNumLayers[i] = rhs.mNumLayers[i];

    for (uint32_t j = 0; j < rhs.mLayers[i].size(); ++j) {
      Hwch::Layer* srcLayer = rhs.mLayers[i][j];
      Hwch::Layer* newLayer = new Hwch::Layer(*srcLayer, false);
      newLayer->mFrame = this;

      mLayers[i].push_back(newLayer);
    }
  }

  // The layer copy constructor is designed to implement cloning just by copying
  // a layer. However, when we copy a frame, this means that the cloning points
  // to
  // the layers in the original frame and not to the newly created copies above.
  // This loop goes through and fixes up the cloning.
  for (uint32_t j = 0; j < rhs.mLayers[0].size(); ++j) {
    Layer* layer = mLayers[0][j];
    Layer* srcLayer = rhs.mLayers[0][j];

    layer->mIsForCloning = srcLayer->mIsForCloning;

    if (layer->IsForCloning()) {
      for (uint32_t d = 1; d < MAX_DISPLAYS; ++d) {
        Layer* srcClone = srcLayer->mClonedLayers[d];

        if (srcClone) {
          // Lookup the index of the clone
          int32_t clone_index =
              GetIndexOfCloneFromLayerList(rhs.mLayers[d], srcLayer);

          if (clone_index != -1) {
            // Found it! Remap it in the destination (this) frame
            Layer* clone = mLayers[d][clone_index];
            layer->mClonedLayers[d] = clone;
            clone->mIsACloneOf = layer;

            HWCLOGV_COND(eLogHarness,
                         "Copy Layer %s@%p [%d] has D%d clone %p [%d]",
                         layer->GetName(), layer, j, d, clone, clone_index);
          }
        }
      }
    }
  }

  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    for (uint32_t j = 0; j < NumLayers(i); ++j) {
      Layer* layer = mLayers[i].at(j);

      HWCLOGV_COND(eLogHarness,
                   "Copy Layer D%d[%d] %s@%p: mIsACloneOf %p: %s dynamic", i, j,
                   layer->GetName(), layer, layer->mIsACloneOf,
                   (layer->mIsACloneOf) ? "NOT" : "");

      // We own all the copy layers, except clones which belong to the parent
      // layer
      if (!layer->IsAClone()) {
        mDynamicLayers.emplace(layer);
      }
    }
  }

  if (HwcTestState::getInstance()->IsOptionEnabled(eLogLayerAlloc)) {
    // Dump out the state
    for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
      for (uint32_t j = 0; j < mLayers[i].size(); ++j) {
        HWCLOGD("Frame::CopyCon:: [%d][%d] : %p : mIsForCloning %d = %d", i, j,
                mLayers[i][j], mLayers[i][j]->mIsForCloning,
                rhs.mLayers[i][j]->mIsForCloning);
        HWCLOGD("Frame::CopyCon:: [%d][%d] : %p : mIsACloneOf %p = %p", i, j,
                mLayers[i][j], mLayers[i][j]->mIsACloneOf,
                rhs.mLayers[i][j]->mIsACloneOf);
        HWCLOGD(
            "Frame::CopyCon:: [%d][%d] : %p : mClonedLayers: %p %p %p = %p %p "
            "%p",
            i, j, mLayers[i][j], mLayers[i][j]->mClonedLayers[0],
            mLayers[i][j]->mClonedLayers[1], mLayers[i][j]->mClonedLayers[2],
            rhs.mLayers[i][j]->mClonedLayers[0],
            rhs.mLayers[i][j]->mClonedLayers[1],
            rhs.mLayers[i][j]->mClonedLayers[2]);
      }
    }
  }
}

Hwch::Frame::~Frame() {
  Clear();
}

void Hwch::Frame::Clear(void) {
  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    // Disassociate layers from this frame
    for (uint32_t j = 0; j < mLayers[i].size(); ++j) {
      Layer* layer = mLayers[i].at(j);
      layer->SetFrame(0);
    }

    mLayers[i].clear();
    mGeometryChanged[i] = true;
    mNumFBLayers[i] = 0;
    mNumLayers[i] = 0;
  }

  // Delete any layers that have been constructed dynamically
  for (std::set<Layer*>::iterator it = mDynamicLayers.begin();it != mDynamicLayers.end();++it) {
    delete *it;
  }

  mDynamicLayers.clear();
}

void Hwch::Frame::Release() {
  Clear();
  Send();
}

void Hwch::Frame::ClearGeometryChanged() {
  for (uint32_t disp = 0; disp < MAX_DISPLAYS; ++disp) {
    mGeometryChanged[disp] = false;
  }
}

void Hwch::Frame::SetGeometryChanged(uint32_t disp) {
  mGeometryChanged[disp] = true;
}

bool Hwch::Frame::IsGeometryChanged(uint32_t disp) {
  return mGeometryChanged[disp];
}

// Printout the frame
void Hwch::Frame::display(void) {
  for (uint32_t disp = 0; disp < MAX_DISPLAYS; ++disp) {
    unsigned int num = mLayers[disp].size();
    HWCLOGI("Display %d: mLayers=%d\n", disp, num);
    for (unsigned int i = 0; i < num; i++) {
      const Layer& l = *(mLayers[disp][i]);

      HWCLOGI(
          "[%d] cmpTyp=%d hints=%d flags=%d handle=%p transform=%d blend=%d "
          "srcCrp=(%f,%f,%f,%f) "
          "disFrm=(%d,%d,%d,%d)\n",
          i, l.mCompType, l.mHints, l.mFlags, l.mBufs->GetHandle(),
          l.mPhysicalTransform, l.mBlending, (double)l.mSourceCropf.left,
          (double)l.mSourceCropf.top, (double)l.mSourceCropf.right,
          (double)l.mSourceCropf.bottom, l.mDisplayFrame.left,
          l.mDisplayFrame.top, l.mDisplayFrame.right, l.mDisplayFrame.bottom);
    }
  }
}

void Hwch::Frame::Add(Layer& layer, int disp) {
  if (layer.mFrame) {
    HWCERROR(eCheckFrameworkProgError,
             "Layer %s is already attached to a frame.", layer.mName.c_str());
    return;
  }

  if (disp >= 0) {
    mLayers[disp].push_back(&layer);
    mGeometryChanged[disp] = true;
    layer.SetFrame(this).SetForCloning(false);
  } else {
    // Clone to all connected displays
    mLayers[0].push_back(&layer);
    mGeometryChanged[0] = true;
    layer.SetFrame(this).SetForCloning(true);

    // Remove any previously cloned layers
    for (uint32_t disp = 0; disp < MAX_DISPLAYS; ++disp) {
      delete layer.mClonedLayers[disp];
      layer.mClonedLayers[disp] = 0;
    }
  }
}

void Hwch::Frame::AddDynamic(Layer* layer, int disp) {
  // Frame takes ownership of the layer - must be deleted when frame is deleted.
  Add(*layer, disp);
  mDynamicLayers.emplace(layer);
}

// Method to add a layer after a specified previous one.
// If not found, we add at the end of the list.
void Hwch::Frame::AddAfter(Layer* previousLayer, Layer& newLayer, int disp) {
  ALOG_ASSERT(this);
  ALOG_ASSERT(&newLayer);
  ALOG_ASSERT(disp < MAX_DISPLAYS);

  if (newLayer.mFrame) {
    HWCERROR(eCheckFrameworkProgError,
             "Layer %s is already attached to a frame.",
             newLayer.mName.c_str());
    return;
  }

  uint32_t ix;

  if (disp < 0) {
    newLayer.SetForCloning(true);
    disp = 0;
  }

  newLayer.SetFrame(this);

  for (ix = 0; ix < mLayers[disp].size(); ++ix) {
    if (mLayers[disp].at(ix) == previousLayer) {
      ++ix;
      break;
    }
  }

  if (ix > mLayers[disp].size()) {
    mLayers[disp].push_back(&newLayer);
  } else {
    mLayers[disp].insert(mLayers[disp].cbegin()+ix,&newLayer);
  }

  mGeometryChanged[disp] = true;
}

// Method to add a layer after a specified previous one.
// If not found, we add at the end of the list.
void Hwch::Frame::AddAt(uint32_t ix, Layer& newLayer, int disp) {
  if (newLayer.mFrame) {
    HWCERROR(eCheckFrameworkProgError,
             "Layer %s is already attached to a frame.",
             newLayer.mName.c_str());
    return;
  }

  if (disp < 0) {
    newLayer.SetForCloning(true);
    disp = 0;
  }

  newLayer.SetFrame(this);

  if (ix > mLayers[disp].size()) {
    mLayers[disp].push_back(&newLayer);
  } else {
    mLayers[disp].insert(mLayers[disp].cbegin()+ix,&newLayer);
  }

  mGeometryChanged[disp] = true;
}

// Method to add a layer before a specified one.
// If nextLayer is null, we add at the start of the list.
// If nextLayer is not found, we add at the end of the list.
void Hwch::Frame::AddBefore(Layer* nextLayer, Layer& newLayer, int disp) {
  if (newLayer.mFrame) {
    HWCERROR(eCheckFrameworkProgError,
             "Layer %s is already attached to a frame.",
             newLayer.mName.c_str());
    return;
  }

  uint32_t ix = 0;

  if (disp < 0) {
    newLayer.SetForCloning(true);
    disp = 0;
  }

  newLayer.SetFrame(this);

  if (nextLayer) {
    for (ix = 0; ix < mLayers[disp].size(); ++ix) {
      if (mLayers[disp].at(ix) == nextLayer) {
        break;
      }
    }
  }

  if (ix > mLayers[disp].size()) {
    mLayers[disp].push_back(&newLayer);
  } else {
    mLayers[disp].insert(mLayers[disp].cbegin()+ix,&newLayer);
  }

  mGeometryChanged[disp] = true;
}

Hwch::Layer* Hwch::Frame::GetLayer(uint32_t ix, uint32_t disp) {
  return mLayers[disp].at(ix);
}

void Hwch::Frame::Remove(Layer& layer) {
  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    // Remove any clone of the layer
    Layer* clone = layer.mClonedLayers[i];

    if (clone) {
      HWCLOGD_COND(eLogLayerAlloc,
                   "Frame::Remove: Layer %s: display %d: removing clone",
                   layer.mName.c_str(), i);
      Remove(*clone);
      delete clone;
      layer.mClonedLayers[i] = 0;
      mGeometryChanged[i] = true;
    }

    for (uint32_t j = 0; j < mLayers[i].size(); ++j) {
      if (mLayers[i].at(j) == &layer) {
        HWCLOGD_COND(eLogLayerAlloc,
                     "Frame::Remove: Layer %s removed from display %d at %d",
                     layer.mName.c_str(), i, j);
        RemoveLayerAt(j, i);
        layer.SetFrame(0);

        // Reset layer state ready for next time it is added to a frame
        layer.mUpdatedSinceFBComp = HWCH_ALL_DISPLAYS_UPDATED;
      }
    }
  }
}

uint32_t Hwch::Frame::GetFlags(uint32_t disp) {
  if (mGeometryChanged[disp]) {
    // HWCLOGV("Display %d Geometry changed, flags=%x", disp, mFlags |
    // HWC_GEOMETRY_CHANGED);
    return mFlags;  //| HWC_GEOMETRY_CHANGED;
  } else {
    // HWCLOGV("Geometry NOT changed, flags=%x", disp, mFlags);
    return mFlags;
  }
}

void Hwch::Frame::RotateTo(hwcomposer::HWCRotation rot, bool animate,
                           uint32_t disp) {
  hwcomposer::HWCRotation relativeRotation =
      SubtractRotation(rot, mSystem.GetDisplay(disp).GetRotation());
  RotateBy(relativeRotation, animate, disp);
}

void Hwch::Frame::RotateBy(hwcomposer::HWCRotation rot, bool animate,
                           uint32_t disp) {
  if (rot == hwcomposer::HWCRotation::kRotateNone) {
    return;
  }

  if (disp != 0) {
    HWCERROR(eCheckFrameworkProgError, "Rotation only supported for panel.");
    return;
  }

  // rot is the amount of rotation the user has given to the panel.
  // The layers actually need the inverse rotation.
  Display& display = mSystem.GetDisplay(disp);
  hwcomposer::HWCRotation prevRotation = display.GetRotation();

  // Tell the display what the new rotation is
  display.SetRotation(AddRotation(prevRotation, rot));

  // Insert a rotation animation (if requested)
  bool commandLineOverride = Hwch::System::getInstance().IsRotationAnimation();
  if (animate || commandLineOverride) {
    RotationAnimation(disp);
  } else {
    mGeometryChanged[disp] = true;
  }
}

void Hwch::Frame::RotationAnimationCheck() {
  // Check that all layers have a bufferSet. If not, then send
  // a single frame to allocate one.
  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    for (uint32_t j = 0; j < mLayers[i].size(); ++j) {
      if (mLayers[i][j] && mLayers[i][j]->mBufs == nullptr) {
        Send();
        return;
      }
    }
  }
}

void Hwch::Frame::RotationAnimation(uint32_t disp) {
  HWCLOGD("Hwch::Frame::RotationAnimation %d", disp);

  // Perform sanity checks
  RotationAnimationCheck();

  // Create a copy of the frame and use this for the animation.
  Hwch::Frame frameCopy(*this);

  // Add a fullscreen RGBA:L layer to model the snapshot layer.
  // Set this to 50% translucent so that we can see whats behind.
  Display& display = mSystem.GetDisplay(disp);
  Hwch::RGBALayer snapshot(display.GetLogicalWidth(),
                           display.GetLogicalHeight(), 0.0, eBlack,
                           Alpha(eBlack, 128));
  frameCopy.Add(snapshot);

  // Send one frame to update internal state
  frameCopy.Send();

  // Look for a fullscreen video layer on D1 and perturb it if it exists.
  // This is to exercise the ClonedVideoLayerFilter code.
  Hwch::Layer* perturbed_layer = nullptr;
  uint32_t perturbWidth = 0, perturbHeight = 0;
  for (uint32_t i = 0; i < frameCopy.mLayers[1].size(); ++i) {
    Hwch::Layer* current_layer = frameCopy.GetLayer(i, 1);

    if (current_layer && current_layer->HasNV12Format() &&
        current_layer->IsFullScreenRotated(mSystem.GetDisplay(1))) {
      HWCLOGD("RotationAnimation: Perturbing fullscreen NV12 input layer\n");

      int32_t width = current_layer->mDisplayFrame.right -
                      current_layer->mDisplayFrame.left;
      int32_t height = current_layer->mDisplayFrame.bottom -
                       current_layer->mDisplayFrame.top;
      if ((width < HWCH_ROTATION_ANIMATION_MIN_PERTURB_VALUE) ||
          (height < HWCH_ROTATION_ANIMATION_MIN_PERTURB_VALUE)) {
        // Display frame too small to perturb
        HWCLOGD("RotationAnimation: Display frame too small to perturb\n");
        continue;
      }

      perturbWidth = width / HWCH_ROTATION_ANIMATION_PERTURB_DIVISOR;
      perturbHeight = height / HWCH_ROTATION_ANIMATION_PERTURB_DIVISOR;

      // Perturb the layer
      perturbed_layer = current_layer;

      perturbed_layer->mDisplayFrame.top += perturbHeight;
      perturbed_layer->mDisplayFrame.left += perturbWidth;
      perturbed_layer->mDisplayFrame.bottom -= perturbHeight;
      perturbed_layer->mDisplayFrame.right -= perturbWidth;

      break;
    }
  }

  // Send frame with snapshot layer (and possibly perturbed video)
  frameCopy.Send(HWCH_ROTATION_ANIMATION_SNAPSHOT_FRAMES);

  // If we have perturbed a layer - return it to normal
  if (perturbed_layer) {
    perturbed_layer->mDisplayFrame.top -= perturbHeight;
    perturbed_layer->mDisplayFrame.left -= perturbWidth;
    perturbed_layer->mDisplayFrame.bottom += perturbHeight;
    perturbed_layer->mDisplayFrame.right += perturbWidth;
  }

  // Create SKIP layers. SF creates one SKIP layer for each layer that's
  // in the frame before the rotation. These are inserted in front of the
  // snapshot layer.
  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    for (uint32_t j = 0; j < mLayers[i].size(); ++j) {
      Hwch::Layer* skipLayer = new Hwch::Layer(*mLayers[i][j], false);
      ALOG_ASSERT(skipLayer);

      // The skip layer has no pattern or buffer set
      skipLayer->mPattern = 0;
      skipLayer->mBufs = 0;
      skipLayer->mName += " Skip";

      skipLayer->SetSkip(
          true, false);  // We need a skip layer, but we do not need a buffer
      skipLayer->SetForCloning(false);
      skipLayer->SetIsACloneOf(nullptr);

      frameCopy.AddDynamic(skipLayer, i);
    }
  }
  frameCopy.Send(HWCH_ROTATION_ANIMATION_SKIP_FRAMES);

  // Signal Geometry Changed when we return to previous state
  for (uint32_t d = 0; d < MAX_DISPLAYS; ++d) {
    mGeometryChanged[d] = true;
  }

  HWCLOGD("Hwch::Frame::RotationAnimation Exit");
}

bool Hwch::Frame::IsRotated90() {
  return RotIs90Or270(mSystem.GetDisplay(0).GetRotation());
}

void Hwch::Frame::SetHwcAcquireDelay(uint32_t delay, int disp) {
  if (disp >= 0) {
    mSystem.GetDisplay(disp).GetFramebufferTarget().SetHwcAcquireDelay(delay);
  } else {
    for (uint32_t d = 0; d < MAX_DISPLAYS; ++d) {
      Display& display = mSystem.GetDisplay(d);

      if (display.IsConnected()) {
        mSystem.GetDisplay(d).GetFramebufferTarget().SetHwcAcquireDelay(delay);
      }
    }

    mHwcAcquireDelay = delay;
  }
}

void Hwch::Frame::Free() {
  Clear();
  HWCLOGI("Test: final send empty list");
  SetHwcAcquireDelay(0);
  Send(2);
}

int Hwch::Frame::Send() {
  // Update the interface with any hotplugs that may have occurred
  mInterface.UpdateDisplays(mHwcAcquireDelay);

  uint32_t numDisplays = mInterface.NumDisplays();
  bool connected[HWCVAL_MAX_CRTCS];

  if (mInterface.GetDevice()) {
    // Do we wait for specified offset from VSync?
    if (mSystem.GetSyncOption() == eCompose) {
      HWCLOGV_COND(eLogEventHandler, "Waiting for VSync before Compose");
      mSystem.GetVSync().WaitForOffsetVSync();
    }

    // Update all the cloning & geometry for display 0
    Layer* lastClonedLayer[MAX_DISPLAYS];
    memset(lastClonedLayer, 0, sizeof(lastClonedLayer));

    HWCLOGD_COND(eLogHarness, "Calculating %d rects for D0", mLayers[0].size());
    for (uint32_t i = 0; i < mLayers[0].size(); ++i) {
      Layer* layer = mLayers[0].at(i);
      Hwch::Display& d0 = mSystem.GetDisplay(0);

      if (layer->IsGeometryChanged() || mGeometryChanged[0]) {
        mGeometryChanged[0] = true;
        layer->SetGeometryChanged(true);
        layer->CalculateRects(d0);
        layer->DoCloning(lastClonedLayer, this);
        layer->SetGeometryChanged(false);
      } else {
        // Keep track of cloned layers so we get the Z-order right
        // if there are "new" cloned layers after this one
        layer->DoCloning(lastClonedLayer, this);
      }
    }

    // Update the geometry for additional displays
    for (uint32_t disp = 0; disp < numDisplays; ++disp) {
      Hwch::Display& display = mSystem.GetDisplay(disp);
      connected[disp] = display.IsConnected();
      if (connected[disp]) {
        HWCLOGD_COND(eLogHarness, "Calculating %d rects for D%d",
                     mLayers[disp].size(), disp);
        for (uint32_t i = 0; i < mLayers[disp].size(); ++i) {
          Layer* layer = mLayers[disp].at(i);

          if (layer->IsAClone()) {
            layer->AdoptBufFromPanel();
          }

          if (layer->IsGeometryChanged() || mGeometryChanged[disp] ||
              display.HasScreenSizeChanged()) {
            if (!layer->IsAutomaticClone()) {
              layer->CalculateRects(mSystem.GetDisplay(disp));
            }

            mGeometryChanged[disp] = true;
            layer->SetGeometryChanged(false);
          }
        }
      }

      display.RecordScreenSize();
    }
    // Update the geometry for displays where the number of layers has changed
    // but nothing else.
    for (uint32_t disp=0; disp<numDisplays; ++disp)
    {
      if (mLayers[disp].size() != mNumLayers[disp])
      {
        mGeometryChanged[disp] = true;
      }
      mNumLayers[disp] = mLayers[disp].size();
    }
    // Allocate enough space for a frame with all it's layers
    hwcval_display_contents_t dcs[MAX_DISPLAYS];
    memset(dcs, 0, MAX_DISPLAYS * sizeof(hwcval_display_contents_t));
    /*        size_t displayStructSizes[MAX_DISPLAYS];
            size_t totalSize=0;

            for (uint32_t disp=0; disp<numDisplays; ++disp)
            {
                size_t size = 0;
                if (connected[disp])
                {
                    uint32_t numLayers = mLayers[disp].size();
                    size = sizeof(hwc2_display_t) + (numLayers+1) *
       sizeof(hwc2_layer_t);
                }
                displayStructSizes[disp] = size;
                totalSize += size;
            }

            // Allocate a buffer for the layer list including the visible
       regions
            size_t allocSize = totalSize + (sizeof(hwcomposer::HwcRect<int>) *
       MAX_VISIBLE_REGIONS);
            char* memBuf = new char[allocSize];*/
    // hwcval_display_contents_t displayContents;
    // memset (&displayContents, 0, sizeof(hwcval_display_contents_t));
    hwc_rect_t visibleRegions[MAX_VISIBLE_REGIONS];
    uint32_t visibleRegionCount = 0;

    for (uint32_t disp = 0; disp < numDisplays; ++disp) {
      float dispVideoRate = 0;
      uint32_t videoCount = 0;
      if (connected[disp]) {
        mGeometryChanged[disp] = true;
        uint32_t numLayers = mLayers[disp].size();
        hwcval_display_contents_t* dc = &dcs[disp];
        dcs[disp].display = 0;

        dc->outPresentFence = -1;
        dc->numHwLayers = numLayers + 1;

        // dc->outbuf to point
        // to a real buffer in an Hwch::BufferSet.
        if (mSystem.IsVirtualDisplayEmulationEnabled() &&
            mSystem.GetDisplay(disp).IsVirtualDisplay()) {
            dc->outbuf = mSystem.GetDisplay(disp).GetNextExternalBuffer();
        } else {
            dc->outbuf = NULL;
        }

        // Each layer must now populate HWC's layer list (in Layer::Send).
        // Also, we determine the video rate, if there is one.
        HWCLOGI("Frame::Send: Display %d: dc->numHwLayers=%d", disp,
                numLayers + 1);

        for (uint32_t i = 0; i < numLayers; i++) {
          Hwch::Layer* layer = mLayers[disp].at(i);
          ALOG_ASSERT(layer);

          hwc2_layer_t outLayer;
          mInterface.CreateLayer(disp, &outLayer);
          dc->hwLayers[i].gralloc_handle = layer->gralloc_handle =
              layer->Send();
          mInterface.setLayerBuffer(disp, outLayer,
                                    layer->gralloc_handle->handle_, -1);
          mInterface.setLayerCompositionType(disp, outLayer,
                                             layer->mCurrentCompType);
          dc->hwLayers[i].compositionType = layer->compositionType =
              layer->mCurrentCompType;
          mInterface.setLayerTransform(disp, outLayer,
                                       layer->mPhysicalTransform);
          mInterface.setLayerSourceCrop(disp, outLayer, layer->mSourceCropf);
          mInterface.setLayerDisplayFrame(disp, outLayer, layer->mDisplayFrame);
          mInterface.setLayerPlaneAlpha(disp, outLayer, layer->mPlaneAlpha);

          hwc_region_t region;
          region.rects =
              layer->AssignVisibleRegions(visibleRegions, visibleRegionCount);
          region.numRects = visibleRegionCount;

          dc->hwLayers[i].visibleRegionScreen = region;
          dc->hwLayers[i].displayFrame.left = layer->mDisplayFrame.left;
          dc->hwLayers[i].displayFrame.right = layer->mDisplayFrame.right;
          dc->hwLayers[i].displayFrame.top = layer->mDisplayFrame.top;
          dc->hwLayers[i].displayFrame.bottom = layer->mDisplayFrame.bottom;
          dc->hwLayers[i].planeAlpha = layer->mPlaneAlpha;

          mInterface.setLayerVisibleRegion(disp, outLayer, region);

          if (mGeometryChanged[disp]) {
            dc->hwLayers[i].compositionType = HWC2_COMPOSITION_CLIENT;
          }

          // If this is a video layer, get the rate and make sure we don't have
          // different conflicting video rates
          if (layer->HasPattern() && layer->HasNV12Format()) {
            dispVideoRate = layer->GetPattern().GetUpdateFreq();
            ++videoCount;
          }
        }

        Hwch::Layer& target = mSystem.GetDisplay(disp).GetFramebufferTarget();
        hwc2_layer_t targetLayer;
        mInterface.CreateLayer(disp, &targetLayer);
        mInterface.setLayerCompositionType(disp, targetLayer,
                                           target.mCurrentCompType);
        dc->hwLayers[numLayers].compositionType = target.compositionType = target.mCurrentCompType;
        mInterface.setLayerTransform(disp, targetLayer,
                                     target.mPhysicalTransform);
        mInterface.setLayerSourceCrop(disp, targetLayer, target.mSourceCropf);
        mInterface.setLayerDisplayFrame(disp, targetLayer,
                                        target.mDisplayFrame);
        mInterface.setLayerPlaneAlpha(disp, targetLayer, target.mPlaneAlpha);
        dc->hwLayers[numLayers].gralloc_handle = target.gralloc_handle =
            target.Send();
        mInterface.setLayerBuffer(disp, targetLayer,
                                  target.gralloc_handle->handle_, -1);

        hwc_region_t targetregion;
        visibleRegionCount = 0;
        targetregion.rects =
            target.AssignVisibleRegions(visibleRegions, visibleRegionCount);
        targetregion.numRects = visibleRegionCount;
        dc->hwLayers[numLayers].visibleRegionScreen = targetregion;
        dc->hwLayers[numLayers].planeAlpha = target.mPlaneAlpha;
        mInterface.setLayerVisibleRegion(disp, targetLayer, targetregion);

        uint32_t num_rects = targetregion.numRects;
        hwcomposer::HwcRegion hwc_region;

        for (size_t rect = 0; rect < num_rects; ++rect) {
          hwc_region.emplace_back(
              targetregion.rects[rect].left, targetregion.rects[rect].top,
              targetregion.rects[rect].right, targetregion.rects[rect].bottom);
        }

        hwcomposer::HwcRect<int> displayFrame;
        ResetRectToRegion(hwc_region, displayFrame);
        dc->hwLayers[numLayers].displayFrame.left = displayFrame.left;
        dc->hwLayers[numLayers].displayFrame.right = displayFrame.right;
        dc->hwLayers[numLayers].displayFrame.top = displayFrame.top;
        dc->hwLayers[numLayers].displayFrame.bottom = displayFrame.bottom;

        // target.Send(dc->hwLayers[numLayers], visibleRegions,
        // visibleRegionCount);

      } else {
        dcs[disp].display = 0;
      }
      // pDc += displayStructSizes[disp];

      if (videoCount > 1) {
        dispVideoRate = 0;
      }
      HwcTestState::getInstance()->SetVideoRate(disp, dispVideoRate);
    }

    // Repaint needed flag has been consumed for this frame
    mInterface.ClearRepaintNeeded();

    // Send the requests to HWC
    if (mSystem.IsFrameToBeSent(mFrameCount)) {
      // Do we wait for specified offset from VSync?
      if (mSystem.GetSyncOption() == ePrepare) {
        HWCLOGV_COND(eLogEventHandler, "Waiting for VSync before Prepare");
        mSystem.GetVSync().WaitForOffsetVSync();
      }

      for (uint32_t disp = 0; disp < numDisplays; ++disp) {
        if (connected[disp]) {
          uint32_t outNumTypes = 0, outNumRequests = 0;
          mInterface.ValidateDisplay(disp, &outNumTypes, &outNumRequests);
        }
      }

      // Populate the FRAMEBUFFER_TARGETs
      for (uint32_t disp = 0; disp < numDisplays; ++disp) {
        // Are there any FRAMEBUFFER layers which were updated this frame?
        bool framebufferTargetNeedsUpdate = false;

        hwcval_display_contents_t* dc = &dcs[disp];

        if (dc) {
          uint32_t numLayers = mLayers[disp].size();

          uint32_t numFBLayers = 0;

          for (uint32_t i = 0; i < numLayers; ++i) {
            Layer* layer = mLayers[disp].at(i);

            if (dc->hwLayers[i].compositionType == HWC2_COMPOSITION_CLIENT) {
              if (layer->HasPattern()) {
                Pattern& pattern = layer->GetPattern();
                if (pattern.IsUpdatedSinceLastFBComp()) {
                  framebufferTargetNeedsUpdate = true;
                }
              }

              if (layer->IsUpdatedSinceLastFBComp(disp) || layer->IsSkip()) {
                framebufferTargetNeedsUpdate = true;
              }

              layer->ClearUpdatedSinceLastFBComp(disp);
              ++numFBLayers;
            } else {
	      // layer->SetAcquireFence(-1);
            }
          }

          Display& display = mSystem.GetDisplay(disp);
          Layer& targetLayer = display.GetFramebufferTarget();

          // Check the number of framebuffer (FB) layers. If any framebuffer
          // layers have been deleted
          // since the last composition, then we still need to update the
          // framebuffer target.
          if ((numFBLayers > 0) && (numFBLayers != mNumFBLayers[disp])) {
            framebufferTargetNeedsUpdate = true;
          }

          // Cache the number of layers marked FB for the next frame
          mNumFBLayers[disp] = numFBLayers;

          if (framebufferTargetNeedsUpdate) {
            // Yes - so do the simulated composition
            // dc->hwLayers[numLayers].handle =
            // targetLayer.mBufs->GetNextBuffer();
            HWCNativeHandle buf = targetLayer.mBufs->Get();

            // Initialize the background of the buffer
            // we could economize by skipping this if any of the (other) layers
            // is full screen
            uint32_t width = buf->meta_data_.width_;
            uint32_t height = buf->meta_data_.height_;

            HWCLOGD_COND(eLogHarness, "Filling FBT %dx%d", width, height);
            // targetLayer.mBufs->WaitReleaseFence(mSystem.GetFenceTimeout(),
            // targetLayer.mName);

            if (!mSystem.GetNoCompose()) {
              sRefCmp.Compose(numLayers, dc->hwLayers, dc->hwLayers + numLayers,
                              false);
            }
                        // Our acquire fences are for testing, and hence are not in sync with the composition
                        // Merge and close any fences on FB layers, and use the merged fence in the FBT.
                        int mergedFence = -1;

                        for (uint32_t i=0; i<numLayers; ++i)
                        {
                            if (dc->hwLayers[i].compositionType == HWC2_COMPOSITION_CLIENT)
                            {
                                int fence = dc->hwLayers[i].acquireFence;
                                dc->hwLayers[i].acquireFence = -1;

                                if (fence > 0)
                                {
                                    if (mergedFence == -1)
                                    {
                                        mergedFence = fence;
                                    }
                                    else
                                    {
                                        int newFence = sync_merge("Hwch FBT merged fence", fence, mergedFence);
                                        HWCLOGD_COND(eLogTimeline, "Hwch::Frame Acquire Fence %d=%d+%d, display %d, layer %d",
                                            newFence, mergedFence, fence, disp, i);
                                        CloseFence(mergedFence);
                                        CloseFence(fence);
                                        mergedFence = newFence;
                                    }
                                }
                            }
                        }

                        // Set the fence for the framebuffer target
			targetLayer.SetAcquireFence(mergedFence);
          } else {
            // Framebuffer Target has not changed
            // Though overlays might have
          }
        }
      }

      // Do we wait for specified offset from VSync?
      if (mSystem.GetSyncOption() == eSet) {
        HWCLOGV_COND(eLogEventHandler, "Waiting for VSync before Set");
        mSystem.GetVSync().WaitForOffsetVSync();
      }

      // If shims are not installed, signal that we are about to enter OnSet
      HwcTestState* state = HwcTestState::getInstance();
      if (state->GetTestKernel() == 0) {
        state->TriggerOnSetCondition();
      }

      // Note, these are return values from the HWC, you have to close them
      for (uint32_t disp = 0; disp < numDisplays; ++disp) {
        hwcval_display_contents_t* dc = &dcs[disp];
        if (!connected[disp])
          continue;

        if (dc) {
          mInterface.PresentDisplay(dc, disp, &dc->outPresentFence);

          uint32_t numLayers = mLayers[disp].size();
          if (dc->outPresentFence > 0)
          {
               if (hwcomposer::HWCPoll(dc->outPresentFence, HWCVAL_SYNC_WAIT_100MS) < 0) {
                   HWCERROR(eCheckGlFail,
                      "outPresentFence: fence timeout");
               }
               dc->outPresentFence = -1;
          }

          for (uint32_t i = 0; i < numLayers; ++i) {
            // Save composition type for next time
            Hwch::Layer* layer = mLayers[disp].at(i);

            if (layer->HasPattern()) {
              layer->GetPattern().ClearUpdatedSinceLastFBComp();
            }
          }

          // mSystem.GetDisplay(disp).GetFramebufferTarget().PostFrame(
          //   HWC_FRAMEBUFFER_TARGET, -1);
        }
      }

      // delete [] memBuf;
      ClearGeometryChanged();
    } else {
      // To speed up the test, we are discarding this frame.
      // Just need to close the acquire fences and set the release fence to -1.
      HWCLOGD("Harness skipping frame %d", mFrameCount);

      for (uint32_t disp = 0; disp < numDisplays; ++disp) {
        hwcval_display_contents_t* dc = &dcs[disp];

        if (dc) {
          uint32_t numLayers = mLayers[disp].size();

          for (uint32_t i = 0; i < numLayers; ++i) {
            // Save composition type for next time
            Hwch::Layer* layer = mLayers[disp].at(i);
            // layer->PostFrame(dc->hwLayers[i].compositionType, -1);
            int acquireFence = dc->hwLayers[i].acquireFence;
            if (acquireFence > 0)
            {
                 close(acquireFence);
            }
          }
        }
      }
      // delete [] memBuf;
    }
  }

  // Now we have something new queued for display, it's safe to delete any
  // buffer sets that were
  // pending deletion
  mSystem.FlushRetainedBufferSets();

  // Update the interface with any hotplugs that may have occurred
  mInterface.UpdateDisplays(mHwcAcquireDelay);

  ++mFrameCount;
  return 1;
}

int Hwch::Frame::Send(uint32_t numFrames) {
  for (uint32_t i = 0; i < numFrames; ++i) {
    if (!Send()) {
      return 0;
    }
  }
  return 1;
}

bool Hwch::Frame::FindLayer(const Layer& layer, uint32_t& ix, uint32_t& disp) {
  for (uint32_t i = 0; i < MAX_DISPLAYS; ++i) {
    for (uint32_t j = 0; j < mLayers[i].size(); ++j) {
      if (mLayers[i].at(j) == &layer) {
        ix = j;
        disp = i;
        return true;
      }
    }
  }
  return false;
}

void Hwch::Frame::WaitForCompValToComplete() {
  HwcTestState::getInstance()->WaitForCompValToComplete();
}

Hwch::Layer* Hwch::Frame::RemoveLayerAt(uint32_t ix, uint32_t disp) {
  Layer* layer = mLayers[disp].at(ix);

  if (disp > 0) {
    Layer* parent = layer->mIsACloneOf;
    if (parent) {
      // Delete ourself from the parent
      parent->RemoveClone(layer);
    }
  }

  layer->mFrame = 0;

  mLayers[disp].at(ix);
  mGeometryChanged[disp] = true;
  return layer;
}

void Hwch::Frame::InsertLayerAt(Layer& layer, uint32_t ix, uint32_t disp) {
  mLayers[disp].insert(mLayers[disp].cbegin()+ix,&layer);
  mGeometryChanged[disp] = true;
}

uint32_t Hwch::Frame::NumLayers(uint32_t disp) {
  return mLayers[disp].size();
}
