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

#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <math.h>

#include "HwcTestState.h"
#include "HwcvalHwc2.h"
#include "HwcvalHwc2Content.h"
#include "HwcvalThreadTable.h"
#include "HwcvalStall.h"

#undef LOG_TAG
#define LOG_TAG "DRM_SHIM"

using namespace Hwcval;

/// Constructor
Hwcval::Hwc2::Hwc2()
    : mState(HwcTestState::getInstance()){
  memset(mHwcFrame,0,sizeof(mHwcFrame));
  mTestKernel = mState->GetTestKernel();
}

EXPORT_API void Hwcval::Hwc2::CheckValidateDisplayEntry(hwc2_display_t display) {
  // TODO Adding numDisplays as local var, enumerate correctly in future
  // HWC frame number
  ++mHwcFrame[display];
  ALOGE("display = %d FrameCount = %d",display, mHwcFrame[display]);
  bool divergeFrameNumbers = mState->IsOptionEnabled(eOptDivergeFrameNumbers);

  if (!divergeFrameNumbers) {
    mTestKernel->AdvanceFrame(display, mHwcFrame[display] - 1);
  }
  if (divergeFrameNumbers) {
    mTestKernel->AdvanceFrame(display);
  }
}

EXPORT_API void Hwcval::Hwc2::CheckValidateDisplayExit() {
  HWCLOGD("In CheckOnPrepareExit");
}

/// Called by HWC Shim to notify that Present Display has occurred, and pass
/// in
/// the
/// contents of the display structures
EXPORT_API void Hwcval::Hwc2::CheckPresentDisplayEnter(hwcval_display_contents_t* displays, hwc2_display_t display) {
// Dump memory usage, if enabled
  DumpMemoryUsage();
  ALOGE("%p layers = %d",displays,displays->numHwLayers);
  Hwcval::PushThreadState ts("CheckPresentDisplayEnter (locking)");
  HWCVAL_LOCK(_l, mTestKernel->GetMutex());
  Hwcval::SetThreadState("CheckPresentDisplayEnter (locked)");

  // Process any pending events.
  // This should always be the first thing we do after placing the lock.
  mTestKernel->ProcessWorkLocked();

  // Create a copy of the layer lists for each display in internal form.
  // Later on we will push this to the LLQ.
  memset(mContent, 0, sizeof(mContent));
 // for (uint32_t d = 0; d < numDisplays; ++d) {
    const hwcval_display_contents_t* disp = displays;

    if (disp) {
      Hwcval::LayerList* ll = new Hwcval::Hwc2LayerList(disp);
      mContent[display] = ll;
    } else {
      mContent[display] = 0;
    }
 // }

  // The idea of the buffer monitor enable was to be able to turn off the
  // majority of the validation for specific performance tests.
  // This has not been used in anger and would not work.
  // TODO: Fix or remove buffer monitor enable.
  if (mState->IsBufferMonitorEnabled()) {

    // This loop has the following purposes:
    // 1. Record the state of each of the input buffers. That means that we
    // create a DrmShimBuffer object
    //    and track it by our internal data structures. These data structures
    //    are then augmented by later information
    //    from intercepted DRM calls that will allow us to understand the
    //    relationships between gralloc buffer handle,
    //    buffer object and frambuffer ID.
    //
    // 2. If any buffers are to be surface flinger composed - i.e. they have a
    // composition type of HWC2_COMPOSITION_CLIENT -
    //    then a transform mapping is created to track this surface flinger
    //    composition. This is then attached to
    //    the DrmShimBuffer of the framebuffer target.
    //
    // 3. Determining for each display whether there is full screen video.
    //    These are then combined to create the flags that are needed for
    //    extended mode validation.
    //    They is then saved within the VideoFlags of the internal layer list.
    //
    // 4. Recording of protected content validity. To avoid spurious errors it
    // is important that this is recorded
    //    at the right time, so we are actually caching in the layer list a
    //    state that was recorded during onValidity.
    //
    // 5. Some additional flag setting and statistic recording.
    //
    bool allScreenVideo = true;  // assumption that all screens have video on
                                 // top layer until we know otherwise

      Hwcval::LogDisplay& ld = mTestKernel->GetLogDisplay(display);
      mTestKernel->VideoInit(display);


      mTestKernel->SetActiveDisplay(display, true);

      HWCLOGD(
          "HwcTestKernel::CheckPresentDisplayEnter - Display %d has %u layers (frame:%d)",
          display, disp->numHwLayers, mHwcFrame[display]);

      DrmShimTransformVector framebuffersComposedForThisTarget;
      bool sfCompositionRequired = false;
      uint32_t skipLayerCount = 0;

      if (disp->numHwLayers == 0) {
        // No content on this screen, so definitely no video
        HWCLOGV_COND(eLogVideo,
                     "No content on screen %d, so definitely no video",
                     display);
        allScreenVideo = false;
      }

      const hwcval_layer_t* fbtLayer = disp->hwLayers + disp->numHwLayers - 1;
      ALOG_ASSERT(fbtLayer->compositionType == HWC2_COMPOSITION_DEVICE);

      const hwc_rect_t& fbtRect = fbtLayer->displayFrame;
      if (fbtRect.right != ld.GetWidth() ||
          fbtRect.bottom != ld.GetHeight() || fbtRect.left != 0 ||
          fbtRect.top != 0) {
        HWCERROR(eCheckLayerOnScreen,
                 "D%d FBT (%d, %d, %d, %d) but display size %dx%d", display,
                 fbtRect.left, fbtRect.top, fbtRect.right, fbtRect.bottom,
                 ld.GetWidth(), ld.GetHeight());
      }

      for (uint32_t i = 0; i < disp->numHwLayers; ++i) {
        const hwcval_layer_t* layer = disp->hwLayers + i;
        const char* bufferType = "Unknown";
        std::shared_ptr<DrmShimBuffer> buf;
        char notes[HWCVAL_DEFAULT_STRLEN];
        notes[0] = '\0';

        switch (layer->compositionType) {
          case HWC2_COMPOSITION_CLIENT: {
            sfCompositionRequired = true;
            bufferType = "Framebuffer";

            if (layer->flags & HWC_SKIP_LAYER) {
              ++skipLayerCount;
            }

            if (layer->gralloc_handle == 0) {
              buf = 0;
            } else {
              buf = mTestKernel->RecordBufferState(
                  layer->gralloc_handle, Hwcval::BufferSourceType::Input, notes);

              if ((layer->flags & HWC_SKIP_LAYER) == 0) {
		  hwcomposer::HwcRect<int> temp;
		  hwcomposer::HwcRect<int> fbt_temp;
		  temp.left = layer->displayFrame.left;
		  temp.right = layer->displayFrame.right;
		  temp.top = layer->displayFrame.top;
		  temp.bottom = layer->displayFrame.bottom;

		  fbt_temp.left = fbtRect.left;
		  fbt_temp.right = fbtRect.right;
		  fbt_temp.top = fbtRect.top;
		  fbt_temp.bottom = fbtRect.bottom;

		mTestKernel->ValidateHwcDisplayFrame(temp,
						     fbt_temp, display, i);
                DrmShimTransform transform(buf, i, layer);
                framebuffersComposedForThisTarget.push_back(transform);

                mTestKernel->AddSfScaleStat(transform.GetXScale());
                mTestKernel->AddSfScaleStat(transform.GetYScale());
              }
            }

            break;
          }
          case HWC2_COMPOSITION_DEVICE: {
            if (layer->gralloc_handle == 0) {
              buf = 0;
            } else if ((layer->flags & HWC_SKIP_LAYER) == 0) {
              bufferType = "Overlay";
	      hwcomposer::HwcRect<int> temp;
	      hwcomposer::HwcRect<int> fbt_temp;
	      temp.left = layer->displayFrame.left;
	      temp.right = layer->displayFrame.right;
	      temp.top = layer->displayFrame.top;
	      temp.bottom = layer->displayFrame.bottom;

	      fbt_temp.left = fbtRect.left;
	      fbt_temp.right = fbtRect.right;
	      fbt_temp.top = fbtRect.top;
	      fbt_temp.bottom = fbtRect.bottom;

	      mTestKernel->ValidateHwcDisplayFrame(temp, fbt_temp,
                                                   display, i);
              buf = mTestKernel->RecordBufferState(
                  layer->gralloc_handle, Hwcval::BufferSourceType::Input, notes);
            } else {
              bufferType = "Overlay (SKIP)";
              buf = mTestKernel->RecordBufferState(
                  layer->gralloc_handle, Hwcval::BufferSourceType::Input, notes);
              DrmShimTransform transform(buf, i, layer);
              framebuffersComposedForThisTarget.push_back(transform);
            }

            break;
          }
        }

        if (buf.get() &&
            buf->GetHandle() == mState->GetFutureTransparentLayer()) {
          HWCLOGW("Actually transparent: %p AppearanceCount %d",
                  buf->GetHandle(), buf->GetAppearanceCount());
          buf->SetTransparentFromHarness();
        }
        ALOGE("%d", layer->visibleRegionScreen.numRects);

	Hwcval::Hwc2Layer valLayer(layer, buf);
        // Work out if we are full screen video on each display
        mTestKernel->DetermineFullScreenVideo(display, i, valLayer, notes);

        // Skip layers will be subject to the skip layer usage check
        HWCCHECK_ADD(eCheckSkipLayerUsage, skipLayerCount);

        // Are we skipping all layers? That means rotation animation
        HwcTestCrtc* crtc = mTestKernel->GetHwcTestCrtcByDisplayIx(i);
        if (crtc) {
          crtc->SetSkipAllLayers((disp->numHwLayers > 1) &&
                                 (skipLayerCount == (disp->numHwLayers - 1)));
        }

        // Add the layer to our internal layer list copy
        mContent[display]->Add(valLayer);
      }

    // Work out combined video state flags, by looking at the current state of
    // all displays
    Hwcval::LayerList::VideoFlags videoFlags = mTestKernel->AnalyzeVideo();

    // Set the combined video state flags on each of the current displays before
    // we push them.
    // (Question: does this leave us in a mess if a display is not updated? Does
    // that mean
    // it could end up with us thinking it is in the wrong mode?)
    for (uint32_t d = 0; d < HWCVAL_MAX_CRTCS; ++d) {
      Hwcval::LayerList* ll = NULL;
      if(d == display)
        Hwcval::LayerList* ll = mContent[display];

      if (ll) {
        HWCLOGV_COND(eLogVideo,
                     "Frame:%d: Content@%p: Setting video flags for D%d",
                     mHwcFrame[display], ll, d);
        ll->SetVideoFlags(videoFlags);
        ll->GetVideoFlags().Log("CheckPresentDisplayEnter", d,
                                mHwcFrame[display]);
        mTestKernel->GetLLQ(d).Push(
            ll,
            mHwcFrame[display]);  // FIX: frame number needs to be per-display
      }
    }

    if (mState->IsCheckEnabled(eCheckSfCompMatchesRef)) {
      // Validate surface flinger composition against reference composer
      for (uint32_t d = 0; d < HWCVAL_MAX_CRTCS; ++d) {
        Hwcval::LayerList* ll = mContent[d];

        if (ll && (ll->GetNumLayers() > 0)) {
          uint32_t fbTgtLayerIx = ll->GetNumLayers() - 1;
          Hwcval::ValLayer& fbTgtLayer = ll->GetLayer(fbTgtLayerIx);

          std::shared_ptr<DrmShimBuffer> fbTgtBuf = fbTgtLayer.GetBuf();

          if (fbTgtBuf.get()) {
            Hwcval::LayerList srcLayers;
            for (uint32_t i = 0; i < fbTgtLayerIx; ++i) {
              Hwcval::ValLayer& layer = ll->GetLayer(i);
              if (layer.GetCompositionType() == CompositionType::SF) {
                srcLayers.Add(layer);
              }
            }

            if (srcLayers.GetNumLayers() > 0) {
              HWCLOGD("Sf Comp Val: Starting for handle %p",
                      fbTgtBuf->GetHandle());
              mTestKernel->GetCompVal()->Compose(fbTgtBuf, srcLayers,
                                                 fbTgtLayer);
              mTestKernel->GetCompVal()->Compare(fbTgtBuf);
            } else {
              HWCLOGD("Sf Comp Val: No layers for handle %p",
                      fbTgtBuf->GetHandle());
            }
          }
        }
      }
    }
  }
}

void Hwcval::Hwc2::CheckPresentDisplayExit(hwcval_display_contents_t* displays, hwc2_display_t display, int32_t *outPresentFence) {
  HWCLOGI("CheckSetExit frame:%d", mHwcFrame[display]);
  PushThreadState ts("CheckSetExit");

  // Clear the future transparent layer notification from the harness
  mState->SetFutureTransparentLayer(0);

  mActiveDisplays = 0;
  // Count the number of active displays
  // We may need to add a flag so users of this variable know if it has changed
  // recently
  // so they don't validate too harshly.
    HwcTestCrtc* crtc = mTestKernel->GetHwcTestCrtcByDisplayIx(display);
    ALOGE("crtc = %p",crtc);
    hwcval_display_contents_t* disp = displays;

    if (crtc && disp && crtc->IsDisplayEnabled()) {
      ++mActiveDisplays;
    ALOGE("mActiveDisplays = %d",mActiveDisplays);
    }

  mTestKernel->SetActiveDisplays(mActiveDisplays);

  if ( displays) {
    int retireFence[HWCVAL_MAX_CRTCS];

      retireFence[display] = -1;

    // Sort the retire fences back to the original displays. This is because the
    // HWC
    // will move the retire fence index from the secondary display to D0 in
    // extended mode.
      hwcval_display_contents_t* disp = displays;

      // If no display, continue
      ALOGE("*outPresentFence%d ",*outPresentFence);
      int rf = *outPresentFence;

      // Assign the fence, no longer worry about original display.
      retireFence[display] = rf;

      Hwcval::LayerListQueue& llq = mTestKernel->GetLLQ(display);

      if (disp && llq.BackNeedsValidating()) {
        Hwcval::LayerList* ll = llq.GetBack();
        int hwcFence = retireFence[display];
        HWCCHECK(eCheckFenceNonZero);
        if (hwcFence == 0) {
          HWCERROR(eCheckFenceNonZero,
                   "Zero retire fence detected on display %d", display);
        }

        if (hwcFence >= 0) {
          int rf = hwcFence;

          // We were having trouble with zero fences.
          // This turned out to be because, owing to another bug of our own and
          // a lack of checking in HWC,
          // HWC was closing FD 0.
          //
          // This code makes us more tolerant of FD 0 if it arises (but it is
          // definitely a bad thing).
          HWCCHECK(eCheckFenceNonZero);
          HWCCHECK(eCheckFenceAllocation);
          if (rf < 0) {
            HWCLOGD("Display %d: Failed to dup retire fence %d", display,
                    hwcFence);
          }

          ll->SetRetireFence(rf);
          HwcTestCrtc* crtc = mTestKernel->GetHwcTestCrtcByDisplayIx(display);

          if (crtc == 0) {
            HWCLOGW("CheckSetExit: Display %d: No CRTC known", display);
          } else {
            crtc->NotifyRetireFence(rf);
          }
        } else {
          ll->SetRetireFence(-1);
        }

        // HWCLOGI_COND(eLogFence, "  -- Display %d retire fence %d dup
        // %d", d, disp->retireFenceFd, ll->GetRetireFence());
      }

    if ((mTestKernel->GetHwcTestCrtcByDisplayIx(0, true) == 0) &&
        (mHwcFrame[display] == 100)) {
      HWCERROR(eCheckInternalError, "No D0 defined within first 100 frames.");
    }
  }

  // Optimization mode is decided in onPrepare so it is correct to do this here
  // rather than on page flip event.
  Hwcval::LayerList* ll = mTestKernel->GetLLQ(eDisplayIxFixed).GetBack();
  if (ll) {
    HWCLOGV_COND(eLogVideo,
                 "Frame:%d Validating optimization mode for D%d (content@%p)",
                 mHwcFrame[display], eDisplayIxFixed, ll);
    ll->GetVideoFlags().Log("CheckSetExit", eDisplayIxFixed, mHwcFrame[display]);
    mTestKernel->ValidateOptimizationMode(ll);
  }

  // This works best here, because it avoids causing errors from display
  // blanking
  // at the start of the next frame.
  {
    PushThreadState ts("CheckSetExit (locking)");
    HWCVAL_LOCK(_l, mTestKernel->GetMutex());
    SetThreadState("CheckSetExit (locked)");
    mTestKernel->ProcessWorkLocked();

    mTestKernel->IterateAllBuffers();
  }

  // Dump memory usage, if enabled
  DumpMemoryUsage();
}

void Hwcval::Hwc2::GetDisplayConfigsExit(int disp, uint32_t* configs,
                                         uint32_t numConfigs) {
  if (disp < HWCVAL_MAX_LOG_DISPLAYS) {
    mTestKernel->GetLogDisplay(disp).SetConfigs(configs, numConfigs);
  } else {
    HWCERROR(eCheckHwcParams, "getDisplayConfigs D%d", disp);
  }
}


void Hwcval::Hwc2::GetDisplayAttributesExit(uint32_t disp, uint32_t config, const int32_t attribute, int32_t* values)
{
    if (disp < HWCVAL_MAX_LOG_DISPLAYS)
    {
        mTestKernel->GetLogDisplay(disp).SetDisplayAttributes(config, attribute, values);
    }
    else
    {
        HWCERROR(eCheckHwcParams, "getDisplayAttributes D%d config %d", disp, config);
    }
}
