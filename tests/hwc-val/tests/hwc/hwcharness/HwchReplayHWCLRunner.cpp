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

#include "HwcTestLog.h"
#include "HwcTestUtil.h"

#include "HwchFrame.h"
#include "HwchLayer.h"
#include "HwchLayers.h"
#include "HwchTest.h"

#include "HwchReplayLayer.h"
#include "HwchReplayPattern.h"
#include "HwchReplayHWCLRunner.h"

bool Hwch::ReplayHWCLRunner::AddLayers(Hwch::Frame& frame, uint32_t display,
                                       layer_cache_t& layer_cache,
                                       layer_cache_t& prev_layer_cache,
                                       int32_t secs, int32_t msecs) {
  bool ret_val = true;
  uint64_t handle = 0;
  uint32_t layer_index = 0, transform = 0;

  // Cache the colours we would like to use (i.e. reserve black and white)
  static Hwch::RGBAColorType colour_lut[] = {
      Hwch::eRed,        Hwch::eGreen,     Hwch::eBlue,        Hwch::eYellow,
      Hwch::eCyan,       Hwch::ePurple,    Hwch::eGrey,        Hwch::eLightRed,
      Hwch::eLightGreen, Hwch::eLightCyan, Hwch::eLightPurple, Hwch::eLightGrey,
      Hwch::eDarkRed,    Hwch::eDarkGreen, Hwch::eDarkBlue,    Hwch::eDarkCyan,
      Hwch::eDarkPurple, Hwch::eDarkGrey};
  static uint32_t colour = 0;

  std::string line;
  while (std::getline(mFile, line)) {
    // Check preconditions
    if (line.empty() || !mParser->IsHwclLayer(line) ||
        !mParser->ParseHwclLayerIndex(line, layer_index) ||
        !mParser->ParseHwclLayerHandle(line, handle) ||
        !mParser->ParseHwclLayerTransform(line, transform)) {
      HWCLOGW_IF(REPLAY_HWCL_DEBUG, "Expected to see a layer, but saw: %s",
                 line.c_str());
      ret_val = -1;
      continue;
    }

    // We have seen a valid layer - print it in the HWC log and increment
    // the layer count.
    HWCLOGI_IF(REPLAY_HWCL_DEBUG, "Replay input: %s", line.c_str());
    mStats.parsed_layer_count++;

    bool is_skip_layer = mParser->IsHwclLayerSkip(line);
    if (is_skip_layer) {
      mStats.skip_layer_count++;
    } else if (mParser->IsHwclLayerFramebufferTarget(line)) {
      // The framebuffer target is the last in the list
      // and should not be added to the frame
      break;
    } else if (mParser->IsHwclLayerUnsupported(line)) {
      // Skip unsupported (i.e. '???') pixel formats
      HWCLOGW_IF(REPLAY_HWCL_DEBUG,
                 "Skipping layer with unsupported "
                 "colour space: %s",
                 line.c_str());
      continue;
    }

    // This statistic is the number of layers that will be processed.
    mStats.processed_layer_count++;

    // Compose the layer_index and the display into a key
    layer_cache_key_t key = MakeLayerCacheKey(layer_index, display);

    // This block of code implements buffer tracking across Geometry
    // Changes. The basic idea is simple. When a Geometry Change is
    // seen, the main loop in 'RunScenario' copies 'layer_cache' into
    // 'prev_layer_cache'. The following code then moves  any layers
    // that persist across the Geometry Change back into 'layer_cache'.
    // Note, only display 0 is considered - any layers cloned on other
    // displays are recreated below. This prevents state that is stale
    // from being treated as live (erroneously).
    if (frame.IsGeometryChanged(display) && (display == 0)) {
      for (std::map<layer_cache_key_t, std::shared_ptr<ReplayLayer>>::iterator itr = prev_layer_cache.begin(); itr != prev_layer_cache.end(); ++itr) {
        // Get a pointer to the layer
        Hwch::ReplayLayer& layer = *itr->second;

        // This part of the code implements the buffer tracking algorithms
        bool match = false;

        if (mReplayMatch == 0)  // Match on handles
        {
          match = (layer.IsKnownBuffer(handle) && !layer.IsAClone());
        } else if (!layer.IsAClone()) {
          // Parse the layer so that we can access its data fields
          std::unique_ptr<Hwch::ReplayLayer> test_layer =
              std::unique_ptr<Hwch::ReplayLayer>(new Hwch::ReplayLayer("Replay", 0, 0));
          mParser->ParseHwclLayer(line, *test_layer);

          // All of the matching algorithms require the buffer width/height
          // to match - check this first
          if ((layer.mWidth.mValue == test_layer->mWidth.mValue) &&
              (layer.mHeight.mValue == test_layer->mHeight.mValue)) {
            Hwch::LogDisplayRect& l_ldf = layer.mLogicalDisplayFrame;
            Hwch::LogDisplayRect& tl_ldf = test_layer->mLogicalDisplayFrame;
            Hwch::LogCropRect& l_lcf = layer.mLogicalCropf;
            Hwch::LogCropRect& tl_lcf = test_layer->mLogicalCropf;

            // These are the actual matching algoriths.
            //
            // Note: update the usage message in HwcHarness.cpp if these change.
            if (mReplayMatch == 1) {
              // Matches on buffer width/height and display frame coordinates
              match = ((l_ldf.left.mValue == tl_ldf.left.mValue) &&
                       (l_ldf.top.mValue == tl_ldf.top.mValue) &&
                       (l_ldf.right.mValue == tl_ldf.right.mValue) &&
                       (l_ldf.bottom.mValue == tl_ldf.bottom.mValue));
            } else if (mReplayMatch == 2) {
              // Matches on buffer width/height and Crop width and Display
              // frame width
              match = (((l_lcf.right.mValue - l_lcf.left.mValue) ==
                        (tl_lcf.right.mValue - tl_lcf.left.mValue)) &&
                       ((l_ldf.right.mValue - l_ldf.left.mValue) ==
                        (tl_ldf.right.mValue - tl_ldf.left.mValue)));
            } else if (mReplayMatch == 3) {
              // Matches on buffer width/height and Crop width/height
              // and Display frame width/height
              match = (((l_lcf.right.mValue - l_lcf.left.mValue) ==
                        (tl_lcf.right.mValue - tl_lcf.left.mValue)) &&
                       ((l_ldf.right.mValue - l_ldf.left.mValue) ==
                        (tl_ldf.right.mValue - tl_ldf.left.mValue)) &&
                       ((l_lcf.bottom.mValue - l_lcf.top.mValue) ==
                        (tl_lcf.bottom.mValue - tl_lcf.top.mValue)) &&
                       ((l_ldf.bottom.mValue - l_ldf.top.mValue) ==
                        (tl_ldf.bottom.mValue - tl_ldf.top.mValue)));
            } else if (mReplayMatch == 4) {
              // Matches on buffer height/width and either all of Display
              // frame or Crop
              match = (((l_ldf.left.mValue == tl_ldf.left.mValue) &&
                        (l_ldf.top.mValue == tl_ldf.top.mValue) &&
                        (l_ldf.right.mValue == tl_ldf.right.mValue) &&
                        (l_ldf.bottom.mValue == tl_ldf.bottom.mValue))) ||
                      ((l_lcf.left.mValue == tl_lcf.left.mValue) &&
                       (l_lcf.top.mValue == tl_lcf.top.mValue) &&
                       (l_lcf.right.mValue == tl_lcf.right.mValue) &&
                       (l_lcf.bottom.mValue == tl_lcf.bottom.mValue));
            }
          }
        }

        if (match) {
          // Found a layer for our display with a buffer handle that
          // is known for this layer. Move it back to 'layer_cache'.
          layer_cache.emplace(key, &layer);

          // Update the layer
          mParser->ParseHwclLayer(line, layer);

          // Add the layer to the frame. Check whether we need to clone
          // the layer to the display.
          layer.mFrame = NULL;
          frame.Add(layer, display);

          // Increment the number of matches
          mStats.match_count++;

          // Add some debug information to the HWC log
          HWCLOGI_IF(
              REPLAY_HWCL_DEBUG,
              "Frame (%5d) : %ds %3dms : "
              "%s : Added copy for %x on display: %d (layer_cache "
              "size: %d, prev_layer_cache_size: %d, key: %d)",
              mStats.parsed_onset_count, secs, msecs,
              frame.IsGeometryChanged(display) ? "GEO CHANGE" : "NOT CHANGE",
              handle, display, layer_cache.size(), prev_layer_cache.size(),
              key);
        }
      }
    }

    // See if the layer is in the cache i.e. was it present in the last frame
    if (layer_cache.find(key) != layer_cache.end()) {
      // This layer is not in the cache - see if this is a candidate
      // for cloning. Note, only clone from display 0 (i.e. the panel).
      // Don't clone from display 1 to display 0 (for example).
      Hwch::ReplayLayer* new_or_cloned_layer = NULL;
      if (display > 0) {
        layer_cache_key_t test_key = MakeLayerCacheKey(layer_index, 0);
        layer_cache_itr_t itr = layer_cache.find(test_key);
        if ((itr != layer_cache.end()) &&
            (itr->second->IsKnownBuffer(handle))) {
          // Found a layer on display 0 - clone it
          new_or_cloned_layer = layer_cache[test_key]->Dup();
          mParser->ParseHwclLayer(line, *new_or_cloned_layer);
          layer_cache.emplace(key, new_or_cloned_layer);

          // Add some debug information to the HWC log
          HWCLOGI_IF(
              REPLAY_HWCL_DEBUG,
              "Frame (%5d) : %ds %3dms : %s : "
              "Cloned layer for %x on display: %d (layer_cache size: %d, "
              "prev_layer_cache_size: %d, key: %d) (%d)",
              mStats.parsed_onset_count, secs, msecs,
              frame.IsGeometryChanged(display) ? "GEO CHANGE" : "NOT CHANGE",
              handle, display, layer_cache.size(), prev_layer_cache.size(), key,
              std::distance(layer_cache.find(key),layer_cache.begin()));
        }
      }

      // See if we created a clone
      if ((display == 0) || (layer_cache.find(key) != layer_cache.end())) {
        // No clone - so allocate a new layer. Note that layer takes
        // ownership of the pattern through a strong pointer.
        HWCLOGI_IF(
            REPLAY_HWCL_DEBUG,
            "Frame (%5d) : %ds %3dms : %s : Allocating "
            "new layer for %x on display: %d (layer_cache size: %d, "
            "prev_layer_cache_size: %d, key: %d) (%d)",
            mStats.parsed_onset_count, secs, msecs,
            frame.IsGeometryChanged(display) ? "GEO CHANGE" : "NOT CHANGE",
            handle, display, layer_cache.size(), prev_layer_cache.size(), key,
            std::distance(layer_cache.find(key),layer_cache.begin()));

        new_or_cloned_layer = new Hwch::ReplayLayer("Replay", 0, 0);
        mParser->ParseHwclLayer(line, *new_or_cloned_layer);

        if (is_skip_layer) {
          new_or_cloned_layer->SetSkip(true,
                                       !mParser->IsHwclLayerUnsupported(line));
        }

        // This code sets the background colour for the layer. If the layer is
        // a background layer (i.e. is layer 0 and is full screen), then set it
        // to white. Otherwise, iterate through the colour palette (but do not
        // use
        // black and white).
        if ((layer_index == 0) && new_or_cloned_layer->IsFullScreen(display)) {
          new_or_cloned_layer->SetPattern(
              new Hwch::ReplayPattern(Hwch::eWhite));
        } else {
          new_or_cloned_layer->SetPattern(
              new Hwch::ReplayPattern(Hwch::Alpha(colour_lut[colour], mAlpha)));

          colour = (colour + 1) % (sizeof(colour_lut) / sizeof(colour_lut[0]));
        }

        new_or_cloned_layer->AddKnownBuffer(handle);
        layer_cache.emplace(key, new_or_cloned_layer);

        // Increment the number of layer allocations
        mStats.allocation_count++;
      }

      // Add the new or cloned layer to the frame
      frame.Add(*new_or_cloned_layer, display);
    } else {
      // The layer was present in the last frame - get a pointer to it
      Hwch::ReplayLayer& layer = *layer_cache[key];

      // Add some debug to the log
      HWCLOGI_IF(
          REPLAY_HWCL_DEBUG,
          "Frame (%5d) : %ds %3dms : %s : Rotating for "
          "%x (layer_cache size: %d, prev_layer_cache_size: %d, key: %d)\n",
          mStats.parsed_onset_count, secs, msecs,
          frame.IsGeometryChanged(display) ? "GEO CHANGE" : "NOT CHANGE",
          handle, layer_cache.size(), prev_layer_cache.size(), key);

      // Check if the handle is different to last time
      if (handle != layer.GetLastHandle()) {
        layer.SetLastHandle(handle);

        if (!layer.IsKnownBuffer(handle)) {
          layer.AddKnownBuffer(handle);
        }

        uint32_t index = layer.GetKnownBufferIndex(handle);

        if (layer.mBufs.get() && layer.mPattern.get()) {
          layer.mBufs->SetNextBufferInstance(index);
          layer.mPattern->ForceUpdate();
        }
      }
    }
  }

  return ret_val;
}

void Hwch::ReplayHWCLRunner::PrintStatistics(void) {
  // Print the replay statistics
  std::printf(
      "HWC log replay complete. Statistics are as follows:\n"
      "\t%d 'onSet Entry' statements parsed (all displays)\n"
      "\t%d layers parsed (including framebuffer targets)\n"
      "\t%d skip layers parsed\n"
      "\t%d frames sent to the HWC \n"
      "\t%d layers sent to HWC \n"
      "\t%d layers tracked across geometry changes\n"
      "\t%d layers allocated due to buffer tracking misses\n"
      "\t%d hot plug events detected (%d connects - %d disconnects)\n"
      "\t%d blanking events detected (%d blanks - %d unblanks)\n",
      mStats.parsed_onset_count, mStats.parsed_layer_count,
      mStats.skip_layer_count, mStats.hwc_frame_count, mStats.allocation_count,
      mStats.hotplug_connects_count, mStats.hotplug_disconnects_count,
      mStats.blanking_count, mStats.blanking_blank_count,
      mStats.blanking_unblank_count);
}

int Hwch::ReplayHWCLRunner::RunScenario(void) {
  Hwch::Frame frame(mInterface);

  // Timestamp of the current and previous frame
  int32_t secs = 0, msecs = 0, nsecs = 0;
  int32_t prev_secs = 0, prev_msecs = 0, prev_nsecs = 0;

  // Frame id and previous frame id
  int32_t frame_id = 0, prev_frame_id = -1;

  // Cache the id of the last display that was processed
  int32_t last_display_processed = -1;

  // Create caches of the layer pointers for deallocation
  // and also for the buffer tracking
  layer_cache_t layer_cache, prev_layer_cache;

  // Flag to ensure that frames are sent when they have been updated
  bool send_frame = false;

  // struct timespec to hold the interframe spacing value
  timespec interframe_spacing = {0, 0};

  // Parse the replay file line-by-line
  std::string line;
  while (std::getline(mFile, line)) {
    int32_t flags = 0, display = 0;
    bool hotplug_connected = false, blanking = false;

    // Skip empty lines
    if (line.empty()) {
      continue;
    }

    // Look for hot plug events
    if (mParser->ParseHotPlug(line, hotplug_connected)) {
      ++mStats.hotplug_count;

      bool can_hotplug = SimulateHotPlug(hotplug_connected);

      if (can_hotplug) {
        if (hotplug_connected) {
          ++mStats.hotplug_connects_count;
        } else {
          ++mStats.hotplug_disconnects_count;
        }
      }

      HWCLOGD("Parsed hot plug event as: %s. SimulateHotPlug returned: %s",
              hotplug_connected ? "connected" : "disconnected",
              can_hotplug ? "can hotplug" : "can not hotplug");
    }
    // Look for blanking events
    else if (mParser->ParseBlanking(line, blanking, display)) {
      ++mStats.blanking_count;

      Blank(blanking);

      if (blanking) {
        ++mStats.blanking_blank_count;
      } else {
        ++mStats.blanking_unblank_count;
      }
    }
    // Look for OnSets
    else if (mParser->ParseHwclOnSet(line, secs, msecs, nsecs, frame_id,
                                     display, flags)) {
      // Update the statistics
      mStats.parsed_onset_count++;

      // Cache the display so that we know when we have processed the whole
      // frame
      last_display_processed = display;

      // Check if this is the start of a new frame. If there is no frame_id,
      // then check to see
      // if the frame has a different timestamp. If so, calculate the interframe
      // time spacing and
      // update the previous timestamps.
      if ((mReplayNoTiming == false) && (display == 0) &&
          (((frame_id != -1) && (frame_id != prev_frame_id)) ||
           ((secs != prev_secs) || (msecs != prev_msecs) ||
            (nsecs != prev_nsecs)))) {
        if (mStats.parsed_onset_count > 1)  // skip the first frame
        {
          // Calculate the interframe time spacing.
          uint64_t prev_time = ((prev_secs * mNanosPerSec) +
                                (prev_msecs * mMillisPerSec) + prev_nsecs);
          uint64_t curr_time =
              ((secs * mNanosPerSec) + (msecs * mMillisPerSec) + nsecs);
          uint64_t spacing = curr_time - prev_time;

          // If the inter-frame spacing is greater than the vsync period for
          // this
          // display, then adjust for it. If spacing is less than or equal to
          // the
          // vsync period, do nothing as interframe_spacing will be set to zero.
          uint32_t vsync_period =
              Hwch::System::getInstance().GetDisplay(display).GetVsyncPeriod();
          if (spacing > vsync_period) {
            interframe_spacing.tv_sec = (spacing - vsync_period) / mNanosPerSec;
            interframe_spacing.tv_nsec =
                (spacing - vsync_period) % mNanosPerSec;
          }

          // Skip the inter-frame spacing if the delay is greater than the
          // bound.
          // This can occur (for example) if the user has combined log files
          // i.e.
          // there is a point in the input where the timestamps are completely
          // different.
          if (interframe_spacing.tv_sec >= mInterframeBound) {
            std::memset(&interframe_spacing, 0, sizeof(interframe_spacing));
          }

          HWCLOGD(
              "Calculated interframe spacing as: %ds %dms %dns (%llu) - "
              "%ds %dms %dns (%llu) - %u = %lld.%.9lds",
              prev_secs, prev_msecs, prev_nsecs, prev_time, secs, msecs, nsecs,
              curr_time, vsync_period, (long long)interframe_spacing.tv_sec,
              interframe_spacing.tv_nsec);
        }

        prev_frame_id = frame_id;
        prev_secs = secs;
        prev_msecs = msecs;
        prev_nsecs = nsecs;
      }

      // Process Geometry Changes
      if (flags & TEMPHWC_GEOMETRY_CHANGED) {
        // In a multi-display system clear everything only on the Geometry
        // Change for the first display (otherwise we will clear the D0 entries
        // when we process the Geometry Change for D1).
        if (display == 0) {
          prev_layer_cache = layer_cache;
          layer_cache.clear();
          frame.Clear();
        } else {
          // If this is a Geometry Change on a display other than 0 (e.g. 1)
          // then clear the previous layer cache (we have finished using it).
          prev_layer_cache.clear();
        }

        frame.SetGeometryChanged(display);
      }

      // Look for and add the layers to the frame
      send_frame =
          AddLayers(frame, display, layer_cache, prev_layer_cache, secs, msecs);
    } else {
      // There are no blanking events, hot plugs or further 'onSet' lines. IF
      // the frame
      // contains valid layers, then send it to the HWC and clean up.
      if (send_frame && frame.NumLayers(0) &&
          (display <= last_display_processed)) {
        // Space the frames according to the original timestamps
        nanosleep(&interframe_spacing, &interframe_spacing);
        std::memset(&interframe_spacing, 0, sizeof(interframe_spacing));

        frame.Send();
        send_frame = false;
        mStats.hwc_frame_count++;
      }
    }
  }

  HWCLOGI_IF(
      REPLAY_HWCL_DEBUG,
      "Replay completed: Parsed %d frames and %d layers "
      "(processed %d, matched %d (%.2f%%) and allocated %d)\n",
      mStats.parsed_onset_count, mStats.parsed_layer_count,
      mStats.processed_layer_count, mStats.match_count,
      ((double)mStats.match_count / (double)mStats.processed_layer_count) *
          100.0,
      mStats.allocation_count);

  PrintStatistics();
  return 0;
}
